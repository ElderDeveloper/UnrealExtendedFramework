// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PerfSentinelAnalyzeCommandlet.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Common/ProviderLock.h"
#include "HAL/PlatformFileManager.h"
#include "TraceServices/ITraceServicesModule.h"
#include "Misc/EngineVersion.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/Model/AllocationsProvider.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/Bookmarks.h"
#include "TraceServices/Model/ContextSwitches.h"
#include "TraceServices/Model/Counters.h"
#include "TraceServices/Model/Frames.h"
#include "TraceServices/Model/LoadTimeProfiler.h"
#include "TraceServices/Model/Log.h"
#include "TraceServices/Model/Memory.h"
#include "TraceServices/Model/Modules.h"
#include "TraceServices/Model/NetProfiler.h"
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 8, 0)
#include "TraceServices/Model/ObjectProvider.h"
#endif
#include "TraceServices/Model/Screenshot.h"
#include "TraceServices/Model/StackSamples.h"
#include "TraceServices/Model/TasksProfiler.h"
#include "TraceServices/Model/Threads.h"
#include "TraceServices/Model/TimingProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PerfSentinelAnalyzeCommandlet)

namespace
{
constexpr int32 MaxHitchWindows = 200;
constexpr int32 MaxTimingEvents = 250000;
constexpr int32 MaxTasks = 20000;
constexpr int32 MaxLogMessages = 5000;
constexpr int32 MaxLoadRows = 50000;
constexpr int32 MaxObjectClasses = 5000;
constexpr int32 MaxStackFrames = 100000;
constexpr int32 MaxStackSampleEvents = 100000;
constexpr int32 MaxNetworkConnections = 1024;
constexpr int32 MaxNetworkTimeBins = 20000;
constexpr int32 MaxNetworkContentRows = 5000;
constexpr int32 MaxNetworkContentTypesPerConnection = 20000;
constexpr int64 MaxNetworkContentEvents = 20000000;
constexpr int32 MaxNetworkPacketSamples = 10000;
constexpr int32 MaxNetworkHitchRows = 20000;
constexpr int32 MaxTimingScopeRowsPerKind = 5000;

TSharedPtr<FJsonValue> JsonObjectValue(const TSharedRef<FJsonObject>& Object)
{
	return MakeShared<FJsonValueObject>(Object);
}

TSharedRef<FJsonObject> MakeCoverageEntry(bool bAvailable, int64 Count = -1)
{
	TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetBoolField(TEXT("available"), bAvailable);
	if (Count >= 0)
	{
		Entry->SetNumberField(TEXT("count"), static_cast<double>(Count));
	}
	return Entry;
}

/** Coverage for a provider this engine's TraceServices does not have: not the same as an empty capture. */
TSharedRef<FJsonObject> MakeUnsupportedCoverageEntry()
{
	TSharedRef<FJsonObject> Entry = MakeCoverageEntry(false);
	Entry->SetStringField(TEXT("status"), TEXT("not_applicable"));
	return Entry;
}

const TraceServices::ITimingProfilerTimerReader& TimerReaderOf(const TraceServices::ITimingProfilerProvider& Provider)
{
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 8, 0)
	return Provider.GetTimerReader();
#else
	// Before 5.8 the reader is only handed to a callback; 5.8's own GetTimerReader() fallback keeps it the same way.
	const TraceServices::ITimingProfilerTimerReader* Reader = nullptr;
	Provider.ReadTimers([&Reader](const TraceServices::ITimingProfilerTimerReader& Out) { Reader = &Out; });
	check(Reader);
	return *Reader;
#endif
}

struct FHitchFrame
{
	uint64 Index = 0;
	double Start = 0.0;
	double End = 0.0;
	double DurationMs = 0.0;
};

constexpr int32 MaxThreadScopeRowsPerThread = 60;
/** Beyond this many focused stretches, aggregate the whole capture window once and say unfocused time is included. */
constexpr int32 MaxAggregationIntervals = 64;

/**
 * The part of the trace that is the capture: from the PerfSentinel.CaptureStart bookmark to PerfSentinel.CaptureStop,
 * minus the stretches between PerfSentinel.Unfocused.Begin and .End. The file opens with the trace system's buffered
 * history from before the capture, and unfocused frames measure OS scheduling and background throttling.
 * Traces without these bookmarks use the whole session.
 */
struct FAnalysisWindow
{
	double Start = 0.0;
	double End = 0.0;
	/** First game frame in the file. Trace time counts from process start, not from the start of the file. */
	double TraceStart = 0.0;
	bool bFromBookmarks = false;
	TArray<TPair<double, double>> Unfocused;
	TArray<TPair<double, double>> Focused;

	bool IncludesFrame(double FrameStart, double FrameEnd) const
	{
		if (FrameStart < Start || FrameStart >= End)
		{
			return false;
		}
		for (const TPair<double, double>& Span : Unfocused)
		{
			if (FrameStart < Span.Value && FrameEnd > Span.Key)
			{
				return false;
			}
		}
		return true;
	}

	double FocusedSeconds() const
	{
		double Seconds = 0.0;
		for (const TPair<double, double>& Span : Focused) { Seconds += FMath::Max(0.0, Span.Value - Span.Key); }
		return Seconds;
	}

	double UnfocusedSeconds() const
	{
		double Seconds = 0.0;
		for (const TPair<double, double>& Span : Unfocused) { Seconds += FMath::Max(0.0, Span.Value - Span.Key); }
		return Seconds;
	}

	/** Focused stretches for aggregation, or the whole window when there are too many to aggregate one by one. */
	TArray<TPair<double, double>> AggregationIntervals(bool& bOutUnfocusedIncluded) const
	{
		bOutUnfocusedIncluded = Focused.Num() > MaxAggregationIntervals;
		return bOutUnfocusedIncluded ? TArray<TPair<double, double>>{ { Start, End } } : Focused;
	}

	void Write(const TSharedRef<FJsonObject>& Root) const
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("source"), bFromBookmarks ? TEXT("perfsentinel_bookmarks") : TEXT("first_frame_to_trace_end"));
		Item->SetNumberField(TEXT("start_seconds"), Start);
		Item->SetNumberField(TEXT("end_seconds"), End);
		Item->SetNumberField(TEXT("trace_start_seconds"), TraceStart);
		Item->SetNumberField(TEXT("window_seconds"), End - Start);
		Item->SetNumberField(TEXT("focused_seconds"), FocusedSeconds());
		Item->SetNumberField(TEXT("unfocused_seconds"), UnfocusedSeconds());
		Item->SetNumberField(TEXT("unfocused_intervals"), Unfocused.Num());
		Item->SetStringField(TEXT("timebase"), TEXT("trace_seconds"));
		Root->SetObjectField(TEXT("analysis_window"), Item);
	}
};

FAnalysisWindow ComputeAnalysisWindow(const TraceServices::IAnalysisSession& Session, double SessionDuration, double TraceStartSeconds)
{
	FAnalysisWindow Window;
	Window.TraceStart = FMath::Clamp(TraceStartSeconds, 0.0, SessionDuration);
	Window.Start = Window.TraceStart;
	Window.End = SessionDuration;
	double CaptureStart = -1.0, CaptureStop = -1.0, OpenSince = -1.0;
	TArray<TPair<double, double>> Unfocused;
	if (const TraceServices::IBookmarkProvider* Provider = Session.ReadProvider<TraceServices::IBookmarkProvider>(TraceServices::GetBookmarkProviderName()))
	{
		TArray<TPair<double, FString>> Marks;
		Provider->EnumerateBookmarks(0.0, SessionDuration, [&Marks](const TraceServices::FBookmark& Bookmark)
		{
			if (Bookmark.Text && FCString::Strncmp(Bookmark.Text, TEXT("PerfSentinel."), 13) == 0)
			{
				Marks.Emplace(Bookmark.Time, Bookmark.Text);
			}
		});
		Marks.Sort([](const TPair<double, FString>& A, const TPair<double, FString>& B) { return A.Key < B.Key; });
		// The buffered history at the head of the file can hold an earlier capture's markers: this capture is
		// the last CaptureStart, and only markers after it count.
		for (const TPair<double, FString>& Mark : Marks)
		{
			if (Mark.Value == TEXT("PerfSentinel.CaptureStart")) { CaptureStart = Mark.Key; }
		}
		for (const TPair<double, FString>& Mark : Marks)
		{
			if (Mark.Key < CaptureStart) { continue; }
			if (Mark.Value == TEXT("PerfSentinel.CaptureStop")) { CaptureStop = Mark.Key; }
			else if (Mark.Value == TEXT("PerfSentinel.Unfocused.Begin")) { if (OpenSince < 0.0) { OpenSince = Mark.Key; } }
			else if (Mark.Value == TEXT("PerfSentinel.Unfocused.End") && OpenSince >= 0.0) { Unfocused.Emplace(OpenSince, Mark.Key); OpenSince = -1.0; }
		}
		// Focus lost before the capture started and still lost at the start: the monitor opens a stretch at activation.
	}
	if (CaptureStart >= 0.0) { Window.Start = CaptureStart; Window.bFromBookmarks = true; }
	if (CaptureStop > Window.Start) { Window.End = CaptureStop; Window.bFromBookmarks = true; }
	// The trace stops before the monitor can write the closing bookmark of a stretch still open at the end.
	if (OpenSince >= 0.0 && Window.End > OpenSince) { Unfocused.Emplace(OpenSince, Window.End); }
	double Cursor = Window.Start;
	for (const TPair<double, double>& Span : Unfocused)
	{
		const double SpanStart = FMath::Clamp(Span.Key, Window.Start, Window.End);
		const double SpanEnd = FMath::Clamp(Span.Value, Window.Start, Window.End);
		if (SpanEnd <= SpanStart) { continue; }
		Window.bFromBookmarks = true;
		Window.Unfocused.Emplace(SpanStart, SpanEnd);
		if (SpanStart > Cursor) { Window.Focused.Emplace(Cursor, SpanStart); }
		Cursor = FMath::Max(Cursor, SpanEnd);
	}
	if (Window.End > Cursor) { Window.Focused.Emplace(Cursor, Window.End); }
	return Window;
}

struct FScopeAccumulator
{
	const TraceServices::FTimingProfilerTimer* Timer = nullptr;
	uint64 InstanceCount = 0;
	double InclusiveSeconds = 0.0, ExclusiveSeconds = 0.0, MaxInclusiveSeconds = 0.0, MaxExclusiveSeconds = 0.0;
};

/** Runs one aggregation per interval and merges the rows by timer. Means are recomputed from the merged totals. */
void AccumulateAggregation(const TraceServices::ITimingProfilerProvider& Provider, const TraceServices::FCreateAggregationParams& BaseParams,
	const TArray<TPair<double, double>>& Intervals, TMap<uint32, FScopeAccumulator>& Out)
{
	for (const TPair<double, double>& Interval : Intervals)
	{
		if (Interval.Value <= Interval.Key) { continue; }
		TraceServices::FCreateAggregationParams Params = BaseParams;
		Params.IntervalStart = Interval.Key;
		Params.IntervalEnd = Interval.Value;
		Params.SortBy = TraceServices::FCreateAggregationParams::ESortBy::DontSort;
		Params.SortOrder = TraceServices::FCreateAggregationParams::ESortOrder::DontSort;
		Params.TableEntryLimit = 0;
		TUniquePtr<TraceServices::ITable<TraceServices::FTimingProfilerAggregatedStats>> Table(Provider.CreateAggregation(Params));
		if (!Table) { continue; }
		TUniquePtr<TraceServices::ITableReader<TraceServices::FTimingProfilerAggregatedStats>> Reader(Table->CreateReader());
		for (; Reader && Reader->IsValid(); Reader->NextRow())
		{
			const TraceServices::FTimingProfilerAggregatedStats* Row = Reader->GetCurrentRow();
			if (!Row || !Row->Timer || Row->InstanceCount == 0 || !FMath::IsFinite(Row->TotalInclusiveTime) || !FMath::IsFinite(Row->TotalExclusiveTime)) { continue; }
			FScopeAccumulator& Accumulator = Out.FindOrAdd(Row->Timer->Id);
			Accumulator.Timer = Row->Timer;
			Accumulator.InstanceCount += Row->InstanceCount;
			Accumulator.InclusiveSeconds += Row->TotalInclusiveTime;
			Accumulator.ExclusiveSeconds += Row->TotalExclusiveTime;
			Accumulator.MaxInclusiveSeconds = FMath::Max(Accumulator.MaxInclusiveSeconds, Row->MaxInclusiveTime);
			Accumulator.MaxExclusiveSeconds = FMath::Max(Accumulator.MaxExclusiveSeconds, Row->MaxExclusiveTime);
		}
	}
}

TArray<FScopeAccumulator> SortedScopes(const TMap<uint32, FScopeAccumulator>& Scopes, bool bByExclusive)
{
	TArray<FScopeAccumulator> Rows;
	Scopes.GenerateValueArray(Rows);
	Rows.Sort([bByExclusive](const FScopeAccumulator& A, const FScopeAccumulator& B)
	{
		return bByExclusive ? A.ExclusiveSeconds > B.ExclusiveSeconds : A.InclusiveSeconds > B.InclusiveSeconds;
	});
	return Rows;
}

void WriteScopeRow(const FScopeAccumulator& Row, const TSharedRef<FJsonObject>& Item)
{
	Item->SetNumberField(TEXT("timer_id"), Row.Timer->Id);
	Item->SetStringField(TEXT("timer"), Row.Timer->Name ? Row.Timer->Name : TEXT("<unnamed>"));
	Item->SetNumberField(TEXT("instance_count"), static_cast<double>(Row.InstanceCount));
	Item->SetNumberField(TEXT("inclusive_ms"), Row.InclusiveSeconds * 1000.0);
	Item->SetNumberField(TEXT("exclusive_ms"), Row.ExclusiveSeconds * 1000.0);
	Item->SetNumberField(TEXT("max_inclusive_ms"), Row.MaxInclusiveSeconds * 1000.0);
	Item->SetNumberField(TEXT("max_exclusive_ms"), Row.MaxExclusiveSeconds * 1000.0);
	Item->SetNumberField(TEXT("mean_inclusive_ms"), Row.InstanceCount > 0 ? Row.InclusiveSeconds * 1000.0 / Row.InstanceCount : 0.0);
	Item->SetNumberField(TEXT("mean_exclusive_ms"), Row.InstanceCount > 0 ? Row.ExclusiveSeconds * 1000.0 / Row.InstanceCount : 0.0);
}

/**
 * Exclusive scope time per key thread (game, render, RHI, task workers) and per GPU queue over the capture window.
 * This is the steady-state "where does frame time go" view; whole-process totals mix every thread together.
 */
void ExtractThreadScopeTotals(const TraceServices::ITimingProfilerProvider* Provider, const TraceServices::IThreadProvider* ThreadProvider,
	const FAnalysisWindow& Window, const TSharedRef<FJsonObject>& Root)
{
	TArray<TSharedPtr<FJsonValue>> Rows;
	TArray<TSharedPtr<FJsonValue>> ThreadSummaries;
	if (!Provider || !ThreadProvider)
	{
		Root->SetArrayField(TEXT("thread_scope_totals"), Rows);
		return;
	}
	bool bUnfocusedIncluded = false;
	const TArray<TPair<double, double>> Intervals = Window.AggregationIntervals(bUnfocusedIncluded);

	struct FGroup { FString Name; FString Kind; TSet<uint32> CpuThreads; int32 GpuQueue = -1; bool bOldGpu = false; };
	TArray<FGroup> Groups;
	FGroup Workers{ TEXT("Task workers"), TEXT("cpu_group") };
	ThreadProvider->EnumerateThreads([&](const TraceServices::FThreadInfo& Thread)
	{
		const FString Name = Thread.Name ? Thread.Name : TEXT("");
		const FString Group = Thread.GroupName ? Thread.GroupName : TEXT("");
		if (Name == TEXT("GameThread") || Name.StartsWith(TEXT("RenderThread")) || Name == TEXT("RHIThread"))
		{
			FGroup Entry{ Name, TEXT("cpu_thread") };
			Entry.CpuThreads.Add(Thread.Id);
			Groups.Add(MoveTemp(Entry));
		}
		else if (Group.Contains(TEXT("Workers")) || Group == TEXT("TBB"))
		{
			Workers.CpuThreads.Add(Thread.Id);
		}
	});
	if (Workers.CpuThreads.Num() > 0) { Groups.Add(MoveTemp(Workers)); }
	bool bAnyQueue = false;
	Provider->EnumerateGpuQueues([&](const TraceServices::FGpuQueueInfo& Queue)
	{
		FGroup Entry{ Queue.GetDisplayName(), TEXT("gpu_queue") };
		Entry.GpuQueue = static_cast<int32>(Queue.Id);
		Groups.Add(MoveTemp(Entry));
		bAnyQueue = true;
	});
	uint32 OldGpuTimeline = ~0u;
	if (!bAnyQueue && Provider->GetGpuTimelineIndex(OldGpuTimeline))
	{
		FGroup Entry{ TEXT("GPU"), TEXT("gpu_queue") };
		Entry.bOldGpu = true;
		Groups.Add(MoveTemp(Entry));
	}

	for (const FGroup& Group : Groups)
	{
		TraceServices::FCreateAggregationParams Params;
		if (Group.Kind == TEXT("gpu_queue"))
		{
			if (Group.bOldGpu) { Params.bIncludeOldGpu1 = true; }
			else { const uint32 QueueId = static_cast<uint32>(Group.GpuQueue); Params.GpuQueueFilter = [QueueId](uint32 Id) { return Id == QueueId; }; }
		}
		else
		{
			const TSet<uint32> ThreadIds = Group.CpuThreads;
			Params.CpuThreadFilter = [ThreadIds](uint32 Id) { return ThreadIds.Contains(Id); };
		}
		TMap<uint32, FScopeAccumulator> Scopes;
		AccumulateAggregation(*Provider, Params, Intervals, Scopes);
		const TArray<FScopeAccumulator> Sorted = SortedScopes(Scopes, true);
		double TotalExclusiveMs = 0.0;
		for (const FScopeAccumulator& Row : Sorted) { TotalExclusiveMs += Row.ExclusiveSeconds * 1000.0; }
		for (int32 Index = 0; Index < FMath::Min(MaxThreadScopeRowsPerThread, Sorted.Num()); ++Index)
		{
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("thread"), Group.Name);
			Item->SetStringField(TEXT("kind"), Group.Kind);
			WriteScopeRow(Sorted[Index], Item);
			Rows.Add(JsonObjectValue(Item));
		}
		TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
		Summary->SetStringField(TEXT("thread"), Group.Name);
		Summary->SetStringField(TEXT("kind"), Group.Kind);
		Summary->SetNumberField(TEXT("thread_count"), Group.Kind == TEXT("gpu_queue") ? 1 : Group.CpuThreads.Num());
		Summary->SetNumberField(TEXT("timer_count"), Sorted.Num());
		Summary->SetNumberField(TEXT("total_exclusive_ms"), TotalExclusiveMs);
		Summary->SetBoolField(TEXT("truncated"), Sorted.Num() > MaxThreadScopeRowsPerThread);
		ThreadSummaries.Add(JsonObjectValue(Summary));
	}
	TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetArrayField(TEXT("threads"), ThreadSummaries);
	Summary->SetNumberField(TEXT("rows_per_thread_limit"), MaxThreadScopeRowsPerThread);
	Summary->SetNumberField(TEXT("analysed_seconds"), bUnfocusedIncluded ? Window.End - Window.Start : Window.FocusedSeconds());
	Summary->SetBoolField(TEXT("unfocused_time_included"), bUnfocusedIncluded);
	Summary->SetStringField(TEXT("semantics"), TEXT("Exclusive (self) time of instrumented scopes per thread or GPU queue over the analysed window. Exclusive time does not overlap within one thread, so a thread's rows add up to its instrumented busy time; waits are scopes too (WaitForTasks, GameThreadWaitForTask)."));
	Root->SetArrayField(TEXT("thread_scope_totals"), Rows);
	Root->SetObjectField(TEXT("thread_scope_summary"), Summary);
}

constexpr int32 MaxHitchScopeWindows = 100;
constexpr int32 MaxHitchScopeRowsPerThread = 12;

/**
 * Exact self time per timer inside each retained slow frame, for the game, render and RHI threads and each GPU queue,
 * aggregated by TraceServices over the frame's interval. Unlike the raw event list it loses no short scopes and it sums
 * repeated calls, so five 21 ms shader-map waits read as one 104 ms row instead of five rows that rank below a wait.
 */
void ExtractHitchScopeTotals(const TraceServices::ITimingProfilerProvider* Provider, const TraceServices::IThreadProvider* ThreadProvider,
	const TArray<FHitchFrame>& HitchFrames, const TSharedRef<FJsonObject>& Root)
{
	TArray<TSharedPtr<FJsonValue>> Rows;
	if (Provider && ThreadProvider && HitchFrames.Num() > 0)
	{
		struct FTarget { FString Name; bool bGpu = false; uint32 Id = 0; };
		TArray<FTarget> Targets;
		ThreadProvider->EnumerateThreads([&](const TraceServices::FThreadInfo& Thread)
		{
			const FString Name = Thread.Name ? Thread.Name : TEXT("");
			if (Name == TEXT("GameThread") || Name.StartsWith(TEXT("RenderThread")) || Name == TEXT("RHIThread")) { Targets.Add({ Name, false, Thread.Id }); }
		});
		Provider->EnumerateGpuQueues([&](const TraceServices::FGpuQueueInfo& Queue) { Targets.Add({ FString(Queue.GetDisplayName()), true, Queue.Id }); });
		for (int32 HitchIndex = 0; HitchIndex < FMath::Min(MaxHitchScopeWindows, HitchFrames.Num()); ++HitchIndex)
		{
			const FHitchFrame& Hitch = HitchFrames[HitchIndex];
			const TArray<TPair<double, double>> Interval{ { Hitch.Start, Hitch.End } };
			for (const FTarget& Target : Targets)
			{
				TraceServices::FCreateAggregationParams Params;
				const uint32 Id = Target.Id;
				if (Target.bGpu) { Params.GpuQueueFilter = [Id](uint32 QueueId) { return QueueId == Id; }; }
				else { Params.CpuThreadFilter = [Id](uint32 ThreadId) { return ThreadId == Id; }; }
				TMap<uint32, FScopeAccumulator> Scopes;
				AccumulateAggregation(*Provider, Params, Interval, Scopes);
				const TArray<FScopeAccumulator> Sorted = SortedScopes(Scopes, true);
				for (int32 Index = 0; Index < FMath::Min(MaxHitchScopeRowsPerThread, Sorted.Num()); ++Index)
				{
					if (Sorted[Index].ExclusiveSeconds < 0.0001) { break; }
					TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
					Item->SetNumberField(TEXT("frame_index"), static_cast<double>(Hitch.Index));
					Item->SetStringField(TEXT("thread"), Target.Name);
					Item->SetStringField(TEXT("kind"), Target.bGpu ? TEXT("gpu_queue") : TEXT("cpu_thread"));
					WriteScopeRow(Sorted[Index], Item);
					Rows.Add(JsonObjectValue(Item));
				}
			}
		}
	}
	Root->SetArrayField(TEXT("hitch_scope_totals"), Rows);
	Root->SetNumberField(TEXT("hitch_scope_windows_limit"), MaxHitchScopeWindows);
	Root->SetStringField(TEXT("hitch_scope_semantics"), TEXT("Per slow frame (longest first, up to the limit), per thread or GPU queue: exclusive time per timer over the frame interval, summed over calls. Waits are timers too."));
}

/** JSON has no infinity or NaN; one open scope or read at trace end must not invalidate the whole file. */
int32 ReplaceNonFiniteNumbers(const TSharedPtr<FJsonValue>& Value)
{
	int32 Replaced = 0;
	if (!Value.IsValid()) { return 0; }
	if (Value->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		for (TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
		{
			if (Field.Value.IsValid() && Field.Value->Type == EJson::Number && !FMath::IsFinite(Field.Value->AsNumber()))
			{
				Field.Value = MakeShared<FJsonValueNull>();
				++Replaced;
			}
			else
			{
				Replaced += ReplaceNonFiniteNumbers(Field.Value);
			}
		}
	}
	else if (Value->Type == EJson::Array)
	{
		// The extractor emits arrays of objects only; objects are fixed in place.
		for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
		{
			Replaced += ReplaceNonFiniteNumbers(Item);
		}
	}
	return Replaced;
}

struct FObjectClassSummary
{
	int64 Count = 0;
	uint64 SystemBytes = 0;
	uint64 VideoBytes = 0;
};

struct FNetworkPacketTotals
{
	uint64 Packets = 0, Bytes = 0, ContentBits = 0;
	uint64 Delivered = 0, Dropped = 0, Unknown = 0, GapPlaceholders = 0;
	uint32 MaxBytes = 0;
	void Add(const TraceServices::FNetProfilerPacket& Packet)
	{
		++Packets;
		Bytes += Packet.TotalPacketSizeInBytes;
		ContentBits += Packet.ContentSizeInBits;
		MaxBytes = FMath::Max(MaxBytes, Packet.TotalPacketSizeInBytes);
		if (Packet.DeliveryStatus == TraceServices::ENetProfilerDeliveryStatus::Dropped)
		{
			++Dropped;
			// UE's analyzer synthesizes zero-byte incoming sequence-gap records.
			// This signature is exposed as a candidate, never counted as wire bytes.
			if (Packet.TotalPacketSizeInBytes == 0 && Packet.ContentSizeInBits == 0 && Packet.EventCount == 0)
			{
				++GapPlaceholders;
			}
		}
		else if (Packet.DeliveryStatus == TraceServices::ENetProfilerDeliveryStatus::Delivered) { ++Delivered; }
		else { ++Unknown; }
	}
	void Write(const TSharedRef<FJsonObject>& Item) const
	{
		Item->SetNumberField(TEXT("packet_count"), static_cast<double>(Packets));
		Item->SetNumberField(TEXT("total_bytes"), static_cast<double>(Bytes));
		Item->SetNumberField(TEXT("content_bits"), static_cast<double>(ContentBits));
		Item->SetNumberField(TEXT("max_packet_bytes"), MaxBytes);
		Item->SetNumberField(TEXT("delivered_packets"), static_cast<double>(Delivered));
		Item->SetNumberField(TEXT("dropped_packets"), static_cast<double>(Dropped));
		Item->SetNumberField(TEXT("unknown_packets"), static_cast<double>(Unknown));
		Item->SetNumberField(TEXT("gap_placeholder_candidates"), static_cast<double>(GapPlaceholders));
		if (Delivered + Dropped > 0)
		{
			Item->SetNumberField(TEXT("status_drop_ratio"), static_cast<double>(Dropped) / (Delivered + Dropped));
		}
	}
};

struct FNetworkScopeCost
{
	uint64 Count = 0, Inclusive = 0, Exclusive = 0;
	uint32 MaxInclusive = 0, MaxExclusive = 0;
};

struct FNetworkScopeStackEntry
{
	uint32 EventType = 0;
	uint32 Start = 0, End = 0;
	uint64 ChildBits = 0;
	bool bRetained = false;
};

void ExtractNetworkEvidence(const TraceServices::IAnalysisSession& Session, double SessionDuration, double TraceStartSeconds,
	const TArray<FHitchFrame>& HitchFrames, const TSharedRef<FJsonObject>& Root, const TSharedRef<FJsonObject>& Coverage)
{
	const TraceServices::INetProfilerProvider* Provider = TraceServices::ReadNetProfilerProvider(Session);
	const uint32 Version = Provider ? Provider->GetNetTraceVersion() : 0;
	// NetTraceAnalyzer does not maintain IAnalysisSession's duration in UE 5.8.
	// A custom net-only capture therefore needs its own clock bounds. Derive
	// them before computing rates, including finite connection/instance closes.
	// Keep the global session duration separate so this never invents CPU/GPU time.
	// Trace time counts from process start: the span starts at the file's first frame, and a connection or instance
	// lifetime that began earlier (FNetProfilerLifeTime::Begin defaults to 0) is clamped there, so rates never divide by uptime.
	const bool bHasSessionSpan = FMath::IsFinite(SessionDuration) && SessionDuration > 0.0;
	const double SpanFloor = bHasSessionSpan ? FMath::Clamp(TraceStartSeconds, 0.0, SessionDuration) : 0.0;
	double NetworkSpanStart = bHasSessionSpan ? SpanFloor : DBL_MAX;
	double NetworkSpanEnd = bHasSessionSpan ? SessionDuration : 0.0;
	uint64 TimedPacketCount = 0, PacketsBeyondSessionDuration = 0;
	auto IncludeTimestamp = [&](double Time)
	{
		if (!FMath::IsFinite(Time) || Time < 0.0) { return; }
		Time = FMath::Max(Time, SpanFloor);
		NetworkSpanStart = FMath::Min(NetworkSpanStart, Time);
		NetworkSpanEnd = FMath::Max(NetworkSpanEnd, Time);
	};
	if (Provider && Version > 0)
	{
		Provider->ReadGameInstances([&](const TraceServices::FNetProfilerGameInstance& Instance)
		{
			IncludeTimestamp(Instance.LifeTime.Begin); IncludeTimestamp(Instance.LifeTime.End);
			Provider->ReadConnections(Instance.GameInstanceIndex, [&](const TraceServices::FNetProfilerConnection& Connection)
			{
				IncludeTimestamp(Connection.LifeTime.Begin); IncludeTimestamp(Connection.LifeTime.End);
				for (uint8 ModeValue = 0; ModeValue < TraceServices::ENetProfilerConnectionMode::Count; ++ModeValue)
				{
					const TraceServices::ENetProfilerConnectionMode Mode = static_cast<TraceServices::ENetProfilerConnectionMode>(ModeValue);
					const uint32 PacketCount = Provider->GetPacketCount(Connection.ConnectionIndex, Mode);
					if (PacketCount == 0) { continue; }
					Provider->EnumeratePackets(Connection.ConnectionIndex, Mode, 0, PacketCount - 1,
						[&](const TraceServices::FNetProfilerPacket& Packet)
						{
							if (!FMath::IsFinite(Packet.TimeStamp) || Packet.TimeStamp < 0.0) { return; }
							IncludeTimestamp(Packet.TimeStamp); ++TimedPacketCount;
							if (Packet.TimeStamp > SessionDuration) { ++PacketsBeyondSessionDuration; }
						});
				}
			});
		});
	}
	if (NetworkSpanStart == DBL_MAX) { NetworkSpanStart = 0.0; }
	NetworkSpanEnd = FMath::Max(NetworkSpanStart, NetworkSpanEnd);
	TArray<TSharedPtr<FJsonValue>> Connections, TimeBins, ContentCosts, PacketSamples, HitchActivity;
	uint64 TotalPackets = 0, TotalContentEvents = 0, ProcessedContentEvents = 0;
	uint64 ConnectionRowsSeen = 0, TimeBinsSeen = 0, ContentRowsSeen = 0, HitchRowsSeen = 0;
	uint64 TimestampErrors = 0, HierarchyErrors = 0, UnretainedContentEvents = 0;
	uint64 ValidBunches = 0, ReliableBunches = 0, ReliableBunchBits = 0;
	uint64 ContentConnectionsTruncated = 0;
	uint64 UnbinnedPackets = 0;
	TArray<int32> OrderedHitchIndices;
	for (int32 Index = 0; Index < HitchFrames.Num(); ++Index) { OrderedHitchIndices.Add(Index); }
	OrderedHitchIndices.Sort([&](int32 A, int32 B) { return HitchFrames[A].Start < HitchFrames[B].Start; });
	if (Provider && Version > 0)
	{
		Provider->ReadGameInstances([&](const TraceServices::FNetProfilerGameInstance& Instance)
		{
			Provider->ReadConnections(Instance.GameInstanceIndex, [&](const TraceServices::FNetProfilerConnection& Connection)
			{
				for (uint8 ModeValue = 0; ModeValue < TraceServices::ENetProfilerConnectionMode::Count; ++ModeValue)
				{
					const TraceServices::ENetProfilerConnectionMode Mode = static_cast<TraceServices::ENetProfilerConnectionMode>(ModeValue);
					const uint32 PacketCount = Provider->GetPacketCount(Connection.ConnectionIndex, Mode);
					// Retain established connections even if one direction has zero packets.
					++ConnectionRowsSeen;
					const FString Direction = Mode == TraceServices::ENetProfilerConnectionMode::Outgoing ? TEXT("outgoing") : TEXT("incoming");
					const FString Identity = FString::Printf(TEXT("instance:%u/connection:%u/direction:%s"), Instance.GameInstanceIndex, Connection.ConnectionIndex, *Direction);
					auto WriteIdentity = [&](const TSharedRef<FJsonObject>& Item)
					{
						Item->SetStringField(TEXT("identity"), Identity);
						Item->SetNumberField(TEXT("instance_index"), Instance.GameInstanceIndex);
						Item->SetNumberField(TEXT("instance_id"), Instance.GameInstanceId);
						Item->SetNumberField(TEXT("connection_index"), Connection.ConnectionIndex);
						Item->SetNumberField(TEXT("connection_id"), Connection.ConnectionId);
						Item->SetStringField(TEXT("direction"), Direction);
						Item->SetStringField(TEXT("mode"), Direction);
					};
					FNetworkPacketTotals Totals;
					TMap<int64, FNetworkPacketTotals> Bins;
					TMap<uint32, FNetworkScopeCost> Costs;
					TArray<FNetworkPacketTotals> HitchTotals;
					HitchTotals.SetNum(HitchFrames.Num());
					double FirstPacket = DBL_MAX, LastPacket = -DBL_MAX;
					uint64 ConnectionContentEvents = 0, ConnectionProcessedEvents = 0;
					uint64 ConnectionValidBunches = 0, ConnectionReliableBunches = 0, ConnectionReliableBits = 0, PartialBunches = 0;
					uint32 PacketIndex = 0, ConnectionSamples = 0;
					bool bConnectionContentTruncated = false;
					if (PacketCount > 0)
					{
						Provider->EnumeratePackets(Connection.ConnectionIndex, Mode, 0, PacketCount - 1,
							[&](const TraceServices::FNetProfilerPacket& Packet)
							{
								Totals.Add(Packet);
								TotalContentEvents += Packet.EventCount;
								ConnectionContentEvents += Packet.EventCount;
								const bool bValidTime = FMath::IsFinite(Packet.TimeStamp) && Packet.TimeStamp >= 0.0;
								bool bInHitch = false;
								if (bValidTime)
								{
									FirstPacket = FMath::Min(FirstPacket, Packet.TimeStamp);
									LastPacket = FMath::Max(LastPacket, Packet.TimeStamp);
									const int64 BinIndex = FMath::FloorToInt64(Packet.TimeStamp);
									FNetworkPacketTotals* Bin = Bins.Find(BinIndex);
									if (!Bin && TimeBins.Num() + Bins.Num() < MaxNetworkTimeBins) { Bin = &Bins.Add(BinIndex); }
									if (Bin) { Bin->Add(Packet); } else { ++UnbinnedPackets; }
									// Game-frame windows are disjoint. Binary search avoids
									// comparing every packet with every retained hitch.
									int32 Low = 0, High = OrderedHitchIndices.Num();
									while (Low < High)
									{
										const int32 Middle = Low + (High - Low) / 2;
										if (HitchFrames[OrderedHitchIndices[Middle]].Start <= Packet.TimeStamp) { Low = Middle + 1; }
										else { High = Middle; }
									}
									if (Low > 0)
									{
										const int32 HitchIndex = OrderedHitchIndices[Low - 1];
										if (Packet.TimeStamp < HitchFrames[HitchIndex].End) { HitchTotals[HitchIndex].Add(Packet); bInHitch = true; }
									}
								}
								else { ++TimestampErrors; }
								// A bounded diagnostic sample, not an unbiased latency/loss dataset.
								if (bValidTime && PacketSamples.Num() < MaxNetworkPacketSamples && (ConnectionSamples < 16 || bInHitch))
								{
									TSharedRef<FJsonObject> Sample = MakeShared<FJsonObject>(); WriteIdentity(Sample);
									Sample->SetNumberField(TEXT("packet_index"), PacketIndex);
									Sample->SetNumberField(TEXT("sequence"), Packet.SequenceNumber);
									Sample->SetNumberField(TEXT("time_seconds"), Packet.TimeStamp);
									Sample->SetNumberField(TEXT("bytes"), Packet.TotalPacketSizeInBytes);
									Sample->SetNumberField(TEXT("content_bits"), Packet.ContentSizeInBits);
									Sample->SetNumberField(TEXT("content_event_count"), Packet.EventCount);
									Sample->SetStringField(TEXT("delivery_status"), Packet.DeliveryStatus == TraceServices::ENetProfilerDeliveryStatus::Dropped ? TEXT("dropped") : Packet.DeliveryStatus == TraceServices::ENetProfilerDeliveryStatus::Delivered ? TEXT("delivered") : TEXT("unknown"));
									Sample->SetStringField(TEXT("connection_state"), TraceServices::LexToString(Packet.ConnectionState));
									Sample->SetBoolField(TEXT("in_hitch_window"), bInHitch);
									PacketSamples.Add(JsonObjectValue(Sample)); ++ConnectionSamples;
								}
								++PacketIndex;
								if (Packet.EventCount == 0) { return; }
								if (ProcessedContentEvents >= MaxNetworkContentEvents) { bConnectionContentTruncated = true; return; }
								TArray<FNetworkScopeStackEntry, TInlineAllocator<32>> Stack;
								auto PopScope = [&]()
								{
									const FNetworkScopeStackEntry Entry = Stack.Pop(EAllowShrinking::No);
									const uint32 Inclusive = Entry.End >= Entry.Start ? Entry.End - Entry.Start : 0;
									if (Entry.ChildBits > Inclusive) { ++HierarchyErrors; }
									const uint32 Exclusive = static_cast<uint32>(Inclusive - FMath::Min<uint64>(Entry.ChildBits, Inclusive));
									if (Entry.bRetained)
									{
										FNetworkScopeCost& Cost = Costs.FindChecked(Entry.EventType);
										Cost.Exclusive += Exclusive; Cost.MaxExclusive = FMath::Max(Cost.MaxExclusive, Exclusive);
									}
								};
								Provider->EnumeratePacketContentEventsByIndex(Connection.ConnectionIndex, Mode, Packet.StartEventIndex, Packet.StartEventIndex + Packet.EventCount - 1,
									[&](const TraceServices::FNetProfilerContentEvent& Event)
									{
										// Finish this packet even at the budget boundary so exclusive
										// costs never include unprocessed children of a retained scope.
										++ProcessedContentEvents; ++ConnectionProcessedEvents;
										while (Stack.Num() > static_cast<int32>(Event.Level)) { PopScope(); }
										if (Stack.Num() != Event.Level || Event.EndPos < Event.StartPos) { ++HierarchyErrors; }
										const uint32 Bits = Event.EndPos >= Event.StartPos ? static_cast<uint32>(Event.EndPos - Event.StartPos) : 0;
										FNetworkScopeCost* Cost = Costs.Find(Event.EventTypeIndex);
										if (!Cost && Costs.Num() < MaxNetworkContentTypesPerConnection) { Cost = &Costs.Add(Event.EventTypeIndex); }
										if (Cost) { ++Cost->Count; Cost->Inclusive += Bits; Cost->MaxInclusive = FMath::Max(Cost->MaxInclusive, Bits); }
										else { ++UnretainedContentEvents; bConnectionContentTruncated = true; }
										if (Stack.Num() > 0)
										{
											FNetworkScopeStackEntry& Parent = Stack.Last();
											// Immediate children contribute once. Nested grandchildren
											// are subtracted by their own parent, not every ancestor.
											if (Event.StartPos >= Parent.Start && Event.EndPos <= Parent.End) { Parent.ChildBits += Bits; }
											else { ++HierarchyErrors; }
										}
										Stack.Add({ Event.EventTypeIndex, static_cast<uint32>(Event.StartPos), static_cast<uint32>(Event.EndPos), 0, Cost != nullptr });
										if (Event.BunchInfo.bIsValid)
										{
											++ConnectionValidBunches;
											if (Event.BunchInfo.bReliable) { ++ConnectionReliableBunches; ConnectionReliableBits += Bits; }
											if (Event.BunchInfo.bPartial) { ++PartialBunches; }
										}
									});
								while (Stack.Num() > 0) { PopScope(); }
							});
					}
					TotalPackets += Totals.Packets;
					ValidBunches += ConnectionValidBunches; ReliableBunches += ConnectionReliableBunches; ReliableBunchBits += ConnectionReliableBits;
					if (bConnectionContentTruncated) { ++ContentConnectionsTruncated; }
					double CaptureStart = FMath::Clamp(FMath::IsFinite(Connection.LifeTime.Begin) ? Connection.LifeTime.Begin : NetworkSpanStart, NetworkSpanStart, NetworkSpanEnd);
					double CaptureEnd = FMath::Clamp(FMath::IsFinite(Connection.LifeTime.End) ? Connection.LifeTime.End : NetworkSpanEnd, NetworkSpanStart, NetworkSpanEnd);
					// A stale lifetime timestamp must not exclude its own observed packets.
					if (FirstPacket != DBL_MAX) { CaptureStart = FMath::Min(CaptureStart, FirstPacket); CaptureEnd = FMath::Max(CaptureEnd, LastPacket); }
					CaptureEnd = FMath::Max(CaptureStart, CaptureEnd);
					if (Connections.Num() < MaxNetworkConnections)
					{
						TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>(); WriteIdentity(Item); Totals.Write(Item);
						Item->SetStringField(TEXT("instance"), Instance.InstanceName ? Instance.InstanceName : TEXT(""));
						Item->SetBoolField(TEXT("server"), Instance.bIsServer);
						Item->SetBoolField(TEXT("iris"), Instance.bIsUsingIrisReplication);
						Item->SetStringField(TEXT("connection"), Connection.Name ? Connection.Name : TEXT(""));
						Item->SetStringField(TEXT("address"), Connection.AddressString ? Connection.AddressString : TEXT(""));
						Item->SetNumberField(TEXT("capture_start_seconds"), CaptureStart); Item->SetNumberField(TEXT("capture_end_seconds"), CaptureEnd);
						Item->SetNumberField(TEXT("capture_duration_seconds"), CaptureEnd - CaptureStart);
						Item->SetNumberField(TEXT("trace_duration_seconds"), SessionDuration);
						Item->SetNumberField(TEXT("network_span_start_seconds"), NetworkSpanStart);
						Item->SetNumberField(TEXT("network_span_end_seconds"), NetworkSpanEnd);
						if (CaptureEnd > CaptureStart) { Item->SetNumberField(TEXT("mean_bytes_per_second"), static_cast<double>(Totals.Bytes) / (CaptureEnd - CaptureStart)); }
						if (FirstPacket != DBL_MAX)
						{
							Item->SetNumberField(TEXT("first_packet_seconds"), FirstPacket); Item->SetNumberField(TEXT("last_packet_seconds"), LastPacket);
							Item->SetNumberField(TEXT("observed_duration_seconds"), LastPacket - FirstPacket);
						}
						Item->SetNumberField(TEXT("content_event_count"), static_cast<double>(ConnectionContentEvents));
						Item->SetNumberField(TEXT("content_events_processed"), static_cast<double>(ConnectionProcessedEvents));
						Item->SetBoolField(TEXT("content_truncated"), bConnectionContentTruncated);
						Item->SetBoolField(TEXT("bunch_metadata_available"), ConnectionValidBunches > 0);
						Item->SetNumberField(TEXT("bunch_count"), static_cast<double>(ConnectionValidBunches));
						Item->SetNumberField(TEXT("reliable_bunch_count"), static_cast<double>(ConnectionReliableBunches));
						Item->SetNumberField(TEXT("reliable_bunch_bits"), static_cast<double>(ConnectionReliableBits));
						Item->SetNumberField(TEXT("partial_bunch_count"), static_cast<double>(PartialBunches));
						Connections.Add(JsonObjectValue(Item));
					}
					TArray<int64> BinIndices; Bins.GetKeys(BinIndices); BinIndices.Sort();
					for (int64 BinIndex : BinIndices)
					{
						++TimeBinsSeen;
						TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>(); WriteIdentity(Item); Bins.FindChecked(BinIndex).Write(Item);
						const double BinStart = static_cast<double>(BinIndex);
						const double Start = FMath::Max(BinStart, NetworkSpanStart), End = FMath::Max(Start, FMath::Min(BinStart + 1.0, NetworkSpanEnd));
						Item->SetNumberField(TEXT("start_seconds"), Start); Item->SetNumberField(TEXT("end_seconds"), End);
						Item->SetNumberField(TEXT("duration_seconds"), End - Start);
						Item->SetBoolField(TEXT("rate_available"), End > Start);
						if (End > Start) { Item->SetNumberField(TEXT("bytes_per_second"), static_cast<double>(Bins.FindChecked(BinIndex).Bytes) / (End - Start)); }
						TimeBins.Add(JsonObjectValue(Item));
					}
					TArray<uint32> CostTypes; Costs.GetKeys(CostTypes);
					CostTypes.Sort([&](uint32 A, uint32 B) { return Costs.FindChecked(A).Inclusive > Costs.FindChecked(B).Inclusive; });
					ContentRowsSeen += CostTypes.Num();
					// Keep the largest named scopes globally, rather than letting the
					// first connection consume every report row.
					for (int32 CostIndex = 0; CostIndex < FMath::Min(CostTypes.Num(), MaxNetworkContentRows); ++CostIndex)
					{
						const uint32 TypeIndex = CostTypes[CostIndex]; const FNetworkScopeCost& Cost = Costs.FindChecked(TypeIndex);
						TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>(); WriteIdentity(Item);
						Item->SetNumberField(TEXT("event_type_index"), TypeIndex);
						Item->SetStringField(TEXT("name"), TEXT("<unknown>"));
						if (TypeIndex < Provider->GetEventTypesCount())
						{
							Provider->ReadEventType(TypeIndex, [&](const TraceServices::FNetProfilerEventType& Type)
							{
								Item->SetStringField(TEXT("name"), Type.Name ? Type.Name : TEXT("<unnamed>"));
								Item->SetNumberField(TEXT("level"), Type.Level); Item->SetNumberField(TEXT("scope_level"), Type.Level);
							});
						}
						Item->SetNumberField(TEXT("event_count"), static_cast<double>(Cost.Count)); Item->SetNumberField(TEXT("instance_count"), static_cast<double>(Cost.Count));
						Item->SetNumberField(TEXT("inclusive_bits"), static_cast<double>(Cost.Inclusive)); Item->SetNumberField(TEXT("exclusive_bits"), static_cast<double>(Cost.Exclusive));
						Item->SetNumberField(TEXT("max_inclusive_bits"), Cost.MaxInclusive); Item->SetNumberField(TEXT("max_exclusive_bits"), Cost.MaxExclusive);
						ContentCosts.Add(JsonObjectValue(Item));
					}
					ContentCosts.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B) { return A->AsObject()->GetNumberField(TEXT("inclusive_bits")) > B->AsObject()->GetNumberField(TEXT("inclusive_bits")); });
					if (ContentCosts.Num() > MaxNetworkContentRows) { ContentCosts.SetNum(MaxNetworkContentRows, EAllowShrinking::No); }
					for (int32 HitchIndex = 0; HitchIndex < HitchTotals.Num(); ++HitchIndex)
					{
						if (HitchTotals[HitchIndex].Packets == 0) { continue; }
						++HitchRowsSeen;
						if (HitchActivity.Num() >= MaxNetworkHitchRows) { continue; }
						TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>(); WriteIdentity(Item); HitchTotals[HitchIndex].Write(Item);
						Item->SetNumberField(TEXT("frame_index"), static_cast<double>(HitchFrames[HitchIndex].Index));
						Item->SetNumberField(TEXT("start_seconds"), HitchFrames[HitchIndex].Start); Item->SetNumberField(TEXT("end_seconds"), HitchFrames[HitchIndex].End);
						HitchActivity.Add(JsonObjectValue(Item));
					}
				}
			});
		});
	}
	TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetBoolField(TEXT("provider_present"), Provider != nullptr);
	Summary->SetNumberField(TEXT("net_trace_version"), Version);
	Summary->SetBoolField(TEXT("packet_data_available"), TotalPackets > 0);
	Summary->SetNumberField(TEXT("session_duration_seconds"), SessionDuration);
	Summary->SetNumberField(TEXT("network_span_start_seconds"), NetworkSpanStart);
	Summary->SetNumberField(TEXT("network_span_end_seconds"), NetworkSpanEnd);
	Summary->SetNumberField(TEXT("network_span_duration_seconds"), NetworkSpanEnd - NetworkSpanStart);
	Summary->SetNumberField(TEXT("timestamped_packets"), static_cast<double>(TimedPacketCount));
	Summary->SetNumberField(TEXT("packet_timestamps_beyond_session_duration"), static_cast<double>(PacketsBeyondSessionDuration));
	Summary->SetBoolField(TEXT("network_span_extends_session_duration"), NetworkSpanEnd > SessionDuration);
	Summary->SetStringField(TEXT("network_span_semantics"), TEXT("Trace clock bounds include the global session interval when available, finite game-instance/connection lifetimes, and every finite nonnegative packet timestamp. Net-only traces can extend beyond the global analysis session duration. With no traced close/session end, the last network event is an observation bound, not proof of the capture stop time. Zero-duration spans retain counts but have no rate."));
	Summary->SetNumberField(TEXT("packet_count"), static_cast<double>(TotalPackets));
	Summary->SetNumberField(TEXT("content_event_count"), static_cast<double>(TotalContentEvents));
	Summary->SetNumberField(TEXT("content_events_processed"), static_cast<double>(ProcessedContentEvents));
	Summary->SetBoolField(TEXT("content_scan_truncated"), ProcessedContentEvents < TotalContentEvents || UnretainedContentEvents > 0);
	Summary->SetNumberField(TEXT("content_scan_limit"), static_cast<double>(MaxNetworkContentEvents));
	Summary->SetNumberField(TEXT("unretained_content_events"), static_cast<double>(UnretainedContentEvents));
	Summary->SetNumberField(TEXT("content_connections_truncated"), static_cast<double>(ContentConnectionsTruncated));
	Summary->SetNumberField(TEXT("invalid_timestamps"), static_cast<double>(TimestampErrors));
	Summary->SetNumberField(TEXT("unbinned_packets"), static_cast<double>(UnbinnedPackets));
	Summary->SetNumberField(TEXT("content_hierarchy_errors"), static_cast<double>(HierarchyErrors));
	Summary->SetBoolField(TEXT("latency_available"), false); Summary->SetBoolField(TEXT("ack_confirmation_available"), false);
	Summary->SetStringField(TEXT("latency_unavailable_reason"), TEXT("UE5.8 NetProfiler packets do not retain ACK timestamps or RTT; use captured connection/runtime counters for ping."));
	Summary->SetStringField(TEXT("delivery_status_semantics"), TEXT("UE5.8 initially marks observed incoming/outgoing packets Delivered; outgoing Delivered is not ACK confirmation. Dropped is a traced drop or an incoming sequence-gap placeholder. Unknown stays distinct. status_drop_ratio is the traced dropped share of non-Unknown records."));
	Summary->SetStringField(TEXT("packet_size_semantics"), TEXT("Traced UE packet bits rounded to bytes; transport/EOS/voice overhead is not guaranteed to be included. Gap placeholders contribute no known bytes."));
	Summary->SetStringField(TEXT("external_transport_coverage"), TEXT("NetProfiler observes traced Unreal NetDriver connections. EOS lobby/social HTTP, relay overhead, voice RTC, Steam SDK and other external socket traffic can bypass NetDriver and are not a complete machine bandwidth total."));
	Summary->SetStringField(TEXT("time_bin_semantics"), TEXT("Occupied one-second trace-clock bins clipped to network_span bounds. Rates divide by the actual clipped bin duration; a packet exactly on the final integer-second bound can have a zero-duration bin with rate_available=false. Incoming gap placeholders are timestamped at the next received packet. Drop status is attributed to the packet's original timestamp, not loss detection time."));
	Summary->SetStringField(TEXT("content_semantics"), TEXT("Named NetTrace scope costs in bits. Inclusive scopes overlap their children and must not be summed as wire bytes. Exclusive costs subtract immediate children once. Names/levels are preserved without guessing RPC, property or actor categories."));
	Summary->SetStringField(TEXT("packet_sample_policy"), TEXT("First 16 valid-timestamp packets per connection direction plus packets in retained hitch windows, bounded globally; diagnostic sample, not an unbiased population."));
	Summary->SetStringField(TEXT("hitch_correlation_semantics"), TEXT("Packet timestamps overlap game-frame hitch windows; temporal correlation alone does not establish a network cause."));
	Summary->SetBoolField(TEXT("bunch_metadata_available"), ValidBunches > 0);
	Summary->SetNumberField(TEXT("bunch_count"), static_cast<double>(ValidBunches));
	Summary->SetNumberField(TEXT("reliable_bunch_count"), static_cast<double>(ReliableBunches));
	Summary->SetNumberField(TEXT("reliable_bunch_bits"), static_cast<double>(ReliableBunchBits));
	Summary->SetBoolField(TEXT("retransmission_count_available"), false);
	Summary->SetStringField(TEXT("reliable_bunch_semantics"), TEXT("Counts traced bunch scopes/fragments with valid reliability flags; not unique application RPCs or retransmissions. May be partial when content_scan_truncated."));
	auto WriteLimit = [&](const TCHAR* Name, int64 Seen, int32 Exported, int32 Limit, bool bTruncated)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetNumberField(TEXT("rows_seen"), static_cast<double>(Seen)); Item->SetNumberField(TEXT("rows_exported"), Exported);
		Item->SetNumberField(TEXT("limit"), Limit); Item->SetBoolField(TEXT("truncated"), bTruncated);
		Summary->SetObjectField(Name, Item);
	};
	WriteLimit(TEXT("connections"), ConnectionRowsSeen, Connections.Num(), MaxNetworkConnections, ConnectionRowsSeen > static_cast<uint64>(Connections.Num()));
	// Once the sparse-bin budget is full, unseen seconds are not tracked in memory.
	WriteLimit(TEXT("time_bins"), TimeBinsSeen, TimeBins.Num(), MaxNetworkTimeBins, UnbinnedPackets > 0);
	WriteLimit(TEXT("content_costs"), ContentRowsSeen, ContentCosts.Num(), MaxNetworkContentRows, ContentRowsSeen > static_cast<uint64>(ContentCosts.Num()) || UnretainedContentEvents > 0);
	WriteLimit(TEXT("packet_samples"), TotalPackets, PacketSamples.Num(), MaxNetworkPacketSamples, TotalPackets > static_cast<uint64>(PacketSamples.Num()));
	WriteLimit(TEXT("hitch_activity"), HitchRowsSeen, HitchActivity.Num(), MaxNetworkHitchRows, HitchRowsSeen > static_cast<uint64>(HitchActivity.Num()));
	Root->SetArrayField(TEXT("network_connections"), Connections); Root->SetArrayField(TEXT("network_time_bins"), TimeBins);
	Root->SetArrayField(TEXT("network_content_costs"), ContentCosts); Root->SetArrayField(TEXT("network_packet_samples"), PacketSamples);
	Root->SetArrayField(TEXT("network_hitch_activity"), HitchActivity); Root->SetObjectField(TEXT("network_summary"), Summary);
	TSharedRef<FJsonObject> NetworkCoverage = MakeCoverageEntry(TotalPackets > 0, TotalPackets);
	NetworkCoverage->SetBoolField(TEXT("provider_present"), Provider != nullptr); NetworkCoverage->SetNumberField(TEXT("net_trace_version"), Version);
	NetworkCoverage->SetStringField(TEXT("status"), TotalPackets > 0 ? TEXT("captured_packets") : Version > 0 ? TEXT("no_packets_observed") : TEXT("not_captured"));
	Coverage->SetObjectField(TEXT("network"), NetworkCoverage);
	Coverage->SetObjectField(TEXT("network_content"), MakeCoverageEntry(ProcessedContentEvents > 0, ProcessedContentEvents));
	Coverage->SetObjectField(TEXT("network_bunches"), MakeCoverageEntry(ValidBunches > 0, ValidBunches));
}

void ExtractTimingScopeTotals(const TraceServices::ITimingProfilerProvider* Provider, const FAnalysisWindow& Window,
	const TSharedRef<FJsonObject>& Root, const TSharedRef<FJsonObject>& Coverage)
{
	TArray<TSharedPtr<FJsonValue>> Totals;
	bool bUnfocusedIncluded = false;
	const TArray<TPair<double, double>> Intervals = Window.AggregationIntervals(bUnfocusedIncluded);
	const double AnalysedSeconds = bUnfocusedIncluded ? Window.End - Window.Start : Window.FocusedSeconds();
	TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("start_seconds"), Window.Start); Summary->SetNumberField(TEXT("end_seconds"), Window.End);
	Summary->SetNumberField(TEXT("duration_seconds"), AnalysedSeconds);
	Summary->SetStringField(TEXT("window_source"), Window.bFromBookmarks ? TEXT("perfsentinel_bookmarks") : TEXT("whole_session"));
	Summary->SetBoolField(TEXT("unfocused_time_included"), bUnfocusedIncluded);
	Summary->SetNumberField(TEXT("rows_per_kind_limit"), MaxTimingScopeRowsPerKind);
	Summary->SetStringField(TEXT("semantics"), TEXT("Instrumented timer aggregation over the analysed window (capture start to stop, minus unfocused stretches) across all CPU threads or GPU queues. Inclusive scopes overlap their children; exclusive scopes subtract children. Concurrent threads/queues may sum beyond wall time. Not total CPU utilization, a causal critical path, or a complete cost of uninstrumented SDK work. thread_scope_totals splits exclusive time by thread."));
	uint32 GpuQueueCount = 0;
	if (Provider)
	{
		Provider->EnumerateGpuQueues([&](const TraceServices::FGpuQueueInfo& Queue) { (void)Queue; ++GpuQueueCount; });
		for (int32 KindIndex = 0; KindIndex < 2; ++KindIndex)
		{
			const bool bCpu = KindIndex == 0;
			TraceServices::FCreateAggregationParams Params;
			if (bCpu) { Params.CpuThreadFilter = [](uint32 ThreadId) { (void)ThreadId; return true; }; }
			else if (GpuQueueCount > 0) { Params.GpuQueueFilter = [](uint32 QueueId) { (void)QueueId; return true; }; }
			else
			{
				uint32 TimelineIndex = ~0u;
				Params.bIncludeOldGpu1 = Provider->GetGpuTimelineIndex(TimelineIndex);
				Params.bIncludeOldGpu2 = Provider->GetGpu2TimelineIndex(TimelineIndex);
			}
			TMap<uint32, FScopeAccumulator> Scopes;
			AccumulateAggregation(*Provider, Params, Intervals, Scopes);
			const TArray<FScopeAccumulator> Sorted = SortedScopes(Scopes, false);
			Summary->SetNumberField(bCpu ? TEXT("cpu_rows_returned") : TEXT("gpu_rows_returned"), static_cast<double>(Sorted.Num()));
			Summary->SetBoolField(bCpu ? TEXT("cpu_truncated") : TEXT("gpu_truncated"), Sorted.Num() > MaxTimingScopeRowsPerKind);
			for (int32 RowIndex = 0; RowIndex < FMath::Min(MaxTimingScopeRowsPerKind, Sorted.Num()); ++RowIndex)
			{
				const FScopeAccumulator& Row = Sorted[RowIndex];
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("kind"), bCpu ? TEXT("cpu") : TEXT("gpu"));
				WriteScopeRow(Row, Item);
				Item->SetStringField(TEXT("source_file"), Row.Timer->File ? Row.Timer->File : TEXT(""));
				Item->SetNumberField(TEXT("line"), Row.Timer->Line);
				Totals.Add(JsonObjectValue(Item));
			}
		}
	}
	Summary->SetNumberField(TEXT("gpu_queue_count"), GpuQueueCount);
	Root->SetArrayField(TEXT("timing_scope_totals"), Totals); Root->SetObjectField(TEXT("timing_scope_summary"), Summary);
	Coverage->SetObjectField(TEXT("timing_aggregate"), MakeCoverageEntry(Totals.Num() > 0, Totals.Num()));
}

double ValidTaskDuration(double Start, double End)
{
	return Start == TraceServices::FTaskInfo::InvalidTimestamp || End == TraceServices::FTaskInfo::InvalidTimestamp
		? -1.0
		: FMath::Max(0.0, End - Start);
}
}

UPerfSentinelAnalyzeCommandlet::UPerfSentinelAnalyzeCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	// Trace providers and unrelated project startup modules can report recoverable
	// errors while extraction still succeeds. Keep the process code tied to Main.
	ShowErrorCount = false;
}

int32 UPerfSentinelAnalyzeCommandlet::Main(const FString& Params)
{
	FString TracePath;
	FString OutputPath;
	FParse::Value(*Params, TEXT("Trace="), TracePath);
	FParse::Value(*Params, TEXT("Out="), OutputPath);
	double HitchThresholdMs = 50.0;
	FParse::Value(*Params, TEXT("HitchThresholdMs="), HitchThresholdMs);

	TracePath.TrimQuotesInline();
	OutputPath.TrimQuotesInline();
	if (TracePath.IsEmpty() || OutputPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("PerfSentinelAnalyze requires explicit -Trace=<existing.utrace> -Out=<native_evidence.json>."));
		return 2;
	}
	HitchThresholdMs = FMath::Clamp(HitchThresholdMs, 1.0, 10000.0);
	TracePath = FPaths::ConvertRelativePathToFull(TracePath);
	OutputPath = FPaths::ConvertRelativePathToFull(OutputPath);
	FPaths::NormalizeFilename(TracePath);
	FPaths::NormalizeFilename(OutputPath);

	if (!FPaths::FileExists(TracePath) || OutputPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("PerfSentinelAnalyze requires -Trace=<existing.utrace> -Out=<native_evidence.json>."));
		return 2;
	}

	ITraceServicesModule& TraceServicesModule = FModuleManager::LoadModuleChecked<ITraceServicesModule>(TEXT("TraceServices"));
	TSharedPtr<TraceServices::IAnalysisService> AnalysisService = TraceServicesModule.GetAnalysisService();
	if (!AnalysisService)
	{
		AnalysisService = TraceServicesModule.CreateAnalysisService();
	}
	if (!AnalysisService)
	{
		UE_LOG(LogTemp, Error, TEXT("PerfSentinelAnalyze could not create TraceServices analysis service."));
		return 3;
	}

	UE_LOG(LogTemp, Display, TEXT("PerfSentinelAnalyze: analyzing %s"), *TracePath);
	const TSharedPtr<const TraceServices::IAnalysisSession> Session = AnalysisService->Analyze(*TracePath);
	if (!Session || !Session->IsAnalysisComplete())
	{
		UE_LOG(LogTemp, Error, TEXT("PerfSentinelAnalyze failed to complete trace analysis."));
		return 4;
	}

	TraceServices::FAnalysisSessionReadScope ReadScope(*Session);
	const double SessionDuration = Session->GetDurationSeconds();
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schema_version"), 3);
	const FEngineVersion& EngineVersion = FEngineVersion::Current();
	Root->SetStringField(TEXT("extractor"), FString::Printf(TEXT("PerfSentinelTraceServices-UE%d.%d"),
		EngineVersion.GetMajor(), EngineVersion.GetMinor()));
	Root->SetStringField(TEXT("trace"), TracePath);
	Root->SetNumberField(TEXT("duration_seconds"), SessionDuration);
	Root->SetNumberField(TEXT("hitch_threshold_ms"), HitchThresholdMs);
	const TraceServices::IFrameProvider* FrameProvider = Session->ReadProvider<TraceServices::IFrameProvider>(TraceServices::GetFrameProviderName());
	double TraceStartSeconds = 0.0;
	if (FrameProvider && FrameProvider->GetFrameCount(TraceFrameType_Game) > 0)
	{
		FrameProvider->EnumerateFrames(TraceFrameType_Game, static_cast<uint64>(0), static_cast<uint64>(1), [&TraceStartSeconds](const TraceServices::FFrame& Frame)
		{
			if (FMath::IsFinite(Frame.StartTime)) { TraceStartSeconds = Frame.StartTime; }
		});
	}
	const FAnalysisWindow Window = ComputeAnalysisWindow(*Session, SessionDuration, TraceStartSeconds);
	Window.Write(Root);
	// Every rate and ratio divides by this, never by duration_seconds (process uptime at the end of the trace).
	Root->SetNumberField(TEXT("trace_start_seconds"), Window.TraceStart);
	int32 HitchFramesOutsideWindow = 0;

	TSharedRef<FJsonObject> Coverage = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> GameFrames;
	TArray<TSharedPtr<FJsonValue>> RenderFrames;
	TArray<FHitchFrame> HitchFrames;

	if (FrameProvider)
	{
		auto ExtractFrames = [&](ETraceFrameType Type, TArray<TSharedPtr<FJsonValue>>& Destination, bool bCollectHitches)
		{
			const uint64 Count = FrameProvider->GetFrameCount(Type);
			FrameProvider->EnumerateFrames(Type, 0, Count, [&](const TraceServices::FFrame& Frame)
			{
				if (!FMath::IsFinite(Frame.StartTime))
				{
					return;
				}
				// A trace can stop while its last frame is open. TraceServices exposes
				// that end as infinity, which is not legal JSON; clamp it to the session.
				const double EndTime = FMath::IsFinite(Frame.EndTime)
					? FMath::Min(Frame.EndTime, SessionDuration)
					: SessionDuration;
				if (EndTime < Frame.StartTime)
				{
					return;
				}
				const double DurationMs = (EndTime - Frame.StartTime) * 1000.0;
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetNumberField(TEXT("index"), static_cast<double>(Frame.Index));
				Item->SetNumberField(TEXT("start_seconds"), Frame.StartTime);
				Item->SetNumberField(TEXT("end_seconds"), EndTime);
				Item->SetNumberField(TEXT("duration_ms"), DurationMs);
				Destination.Add(JsonObjectValue(Item));
				if (bCollectHitches && DurationMs >= HitchThresholdMs)
				{
					// Retain scopes only for slow frames the analysis keeps: inside the capture and focused.
					if (Window.IncludesFrame(Frame.StartTime, EndTime)) { HitchFrames.Add({ Frame.Index, Frame.StartTime, EndTime, DurationMs }); }
					else { ++HitchFramesOutsideWindow; }
				}
			});
		};
		ExtractFrames(TraceFrameType_Game, GameFrames, true);
		ExtractFrames(TraceFrameType_Rendering, RenderFrames, false);
	}
	HitchFrames.Sort([](const FHitchFrame& A, const FHitchFrame& B) { return A.DurationMs > B.DurationMs; });
	const int32 HitchWindowsSeen = HitchFrames.Num();
	if (HitchFrames.Num() > MaxHitchWindows)
	{
		HitchFrames.SetNum(MaxHitchWindows, EAllowShrinking::No);
	}
	Root->SetArrayField(TEXT("game_frames"), GameFrames);
	Root->SetArrayField(TEXT("render_frames"), RenderFrames);
	Coverage->SetObjectField(TEXT("frames"), MakeCoverageEntry(FrameProvider != nullptr, GameFrames.Num()));

	Root->SetNumberField(TEXT("hitch_frames_outside_window"), HitchFramesOutsideWindow);

	const TraceServices::IThreadProvider* ThreadProvider = Session->ReadProvider<TraceServices::IThreadProvider>(TraceServices::GetThreadProviderName());
	const TraceServices::ITimingProfilerProvider* TimingProvider = TraceServices::ReadTimingProfilerProvider(*Session);
	ExtractTimingScopeTotals(TimingProvider, Window, Root, Coverage);
	ExtractThreadScopeTotals(TimingProvider, ThreadProvider, Window, Root);

	// Scopes inside the retained slow frames. Windows are walked one at a time with their own event budget, so a few
	// long frames or many threads cannot starve later windows or the GPU queues (previously each thread walked every
	// window first and the GPU queues went last).
	struct FTimelineRef { uint32 TimelineIndex = 0; int64 ThreadId = -1; FString Name; bool bGpu = false; };
	TArray<FTimelineRef> Timelines;
	TArray<TSharedPtr<FJsonValue>> Threads;
	if (ThreadProvider)
	{
		ThreadProvider->EnumerateThreads([&](const TraceServices::FThreadInfo& Thread)
		{
			TSharedRef<FJsonObject> ThreadItem = MakeShared<FJsonObject>();
			ThreadItem->SetNumberField(TEXT("id"), Thread.Id);
			ThreadItem->SetStringField(TEXT("name"), Thread.Name ? Thread.Name : TEXT(""));
			ThreadItem->SetStringField(TEXT("group"), Thread.GroupName ? Thread.GroupName : TEXT(""));
			Threads.Add(JsonObjectValue(ThreadItem));
			uint32 TimelineIndex = ~0u;
			if (TimingProvider && TimingProvider->GetCpuThreadTimelineIndex(Thread.Id, TimelineIndex))
			{
				Timelines.Add({ TimelineIndex, static_cast<int64>(Thread.Id), Thread.Name ? FString(Thread.Name) : FString(), false });
			}
		});
	}
	if (TimingProvider)
	{
		bool bAnyQueue = false;
		TimingProvider->EnumerateGpuQueues([&](const TraceServices::FGpuQueueInfo& Queue)
		{
			uint32 TimelineIndex = ~0u;
			if (TimingProvider->GetGpuQueueTimelineIndex(Queue.Id, TimelineIndex)) { Timelines.Add({ TimelineIndex, -1, FString(Queue.GetDisplayName()), true }); bAnyQueue = true; }
		});
		if (!bAnyQueue)
		{
			uint32 TimelineIndex = ~0u;
			if (TimingProvider->GetGpuTimelineIndex(TimelineIndex)) { Timelines.Add({ TimelineIndex, -1, TEXT("GPU"), true }); }
			if (TimingProvider->GetGpu2TimelineIndex(TimelineIndex)) { Timelines.Add({ TimelineIndex, -1, TEXT("GPU2"), true }); }
		}
	}

	TArray<TSharedPtr<FJsonValue>> TimingEvents;
	TArray<TSharedPtr<FJsonValue>> HitchWindowRows;
	int32 TimingEventCount = 0;
	int32 WindowsTruncated = 0;
	const int32 PerWindowBudget = HitchFrames.Num() > 0 ? FMath::Max(2000, MaxTimingEvents / HitchFrames.Num()) : 0;
	for (const FHitchFrame& Hitch : HitchFrames)
	{
		if (!TimingProvider || TimingEventCount >= MaxTimingEvents) { break; }
		const int32 WindowBudget = FMath::Min(PerWindowBudget, MaxTimingEvents - TimingEventCount);
		int32 WindowEvents = 0;
		bool bWindowTruncated = false;
		for (const FTimelineRef& Ref : Timelines)
		{
			if (bWindowTruncated) { break; }
			TimingProvider->ReadTimeline(Ref.TimelineIndex, [&](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
			{
				Timeline.EnumerateEvents(Hitch.Start, Hitch.End,
					[&](double Start, double End, uint32 Depth, const TraceServices::FTimingProfilerEvent& Event)
					{
						const double ClippedStart = FMath::Max(Start, Hitch.Start);
						const double ScopeEnd = FMath::IsFinite(End) ? End : Window.End;
						const double ClippedEnd = FMath::Min(ScopeEnd, Hitch.End);
						if (!FMath::IsFinite(Start) || ClippedEnd <= ClippedStart) { return TraceServices::EEventEnumerate::Continue; }
						// Sub-50 us CPU scopes are skipped for volume; hitch_scope_totals carries exact self time per timer.
						if (!Ref.bGpu && (ClippedEnd - ClippedStart) < 0.00005) { return TraceServices::EEventEnumerate::Continue; }
						if (WindowEvents >= WindowBudget) { bWindowTruncated = true; return TraceServices::EEventEnumerate::Stop; }
						TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
						Item->SetNumberField(TEXT("frame_index"), static_cast<double>(Hitch.Index));
						Item->SetNumberField(TEXT("thread_id"), static_cast<double>(Ref.ThreadId));
						if (Ref.bGpu) { Item->SetNumberField(TEXT("timeline_index"), Ref.TimelineIndex); }
						Item->SetStringField(TEXT("thread"), Ref.Name);
						Item->SetNumberField(TEXT("timer_id"), Event.TimerIndex);
						const TraceServices::FTimingProfilerTimer* Timer = TimerReaderOf(*TimingProvider).GetTimer(Event.TimerIndex);
						Item->SetStringField(TEXT("timer"), Timer && Timer->Name ? Timer->Name : TEXT("<unknown>"));
						Item->SetNumberField(TEXT("start_seconds"), ClippedStart);
						Item->SetNumberField(TEXT("end_seconds"), ClippedEnd);
						Item->SetNumberField(TEXT("duration_ms"), (ClippedEnd - ClippedStart) * 1000.0);
						Item->SetNumberField(TEXT("original_scope_start_seconds"), Start);
						Item->SetNumberField(TEXT("original_scope_end_seconds"), ScopeEnd);
						Item->SetNumberField(TEXT("original_scope_duration_ms"), FMath::Max(0.0, ScopeEnd - Start) * 1000.0);
						Item->SetBoolField(TEXT("scope_open_at_trace_end"), !FMath::IsFinite(End));
						Item->SetNumberField(TEXT("depth"), Depth);
						TimingEvents.Add(JsonObjectValue(Item));
						++WindowEvents;
						++TimingEventCount;
						return TraceServices::EEventEnumerate::Continue;
					});
			});
		}
		WindowsTruncated += bWindowTruncated ? 1 : 0;
		TSharedRef<FJsonObject> WindowItem = MakeShared<FJsonObject>();
		WindowItem->SetNumberField(TEXT("frame_index"), static_cast<double>(Hitch.Index));
		WindowItem->SetNumberField(TEXT("start_seconds"), Hitch.Start);
		WindowItem->SetNumberField(TEXT("end_seconds"), Hitch.End);
		WindowItem->SetNumberField(TEXT("duration_ms"), Hitch.DurationMs);
		WindowItem->SetNumberField(TEXT("events_kept"), WindowEvents);
		WindowItem->SetBoolField(TEXT("events_truncated"), bWindowTruncated);
		HitchWindowRows.Add(JsonObjectValue(WindowItem));
	}
	ExtractHitchScopeTotals(TimingProvider, ThreadProvider, HitchFrames, Root);
	Root->SetArrayField(TEXT("threads"), Threads);
	Root->SetArrayField(TEXT("timing_events"), TimingEvents);
	Root->SetArrayField(TEXT("hitch_windows"), HitchWindowRows);
	Root->SetStringField(TEXT("timing_events_scope"), TEXT("retained_hitch_windows_only"));
	Root->SetBoolField(TEXT("timing_events_truncated"), TimingEventCount >= MaxTimingEvents || WindowsTruncated > 0);
	Root->SetNumberField(TEXT("timing_events_limit"), MaxTimingEvents);
	Root->SetNumberField(TEXT("timing_events_per_window_limit"), PerWindowBudget);
	Root->SetNumberField(TEXT("hitch_windows_events_truncated"), WindowsTruncated);
	Root->SetNumberField(TEXT("hitch_windows_exported"), HitchFrames.Num());
	Root->SetNumberField(TEXT("hitch_windows_seen"), HitchWindowsSeen);
	Root->SetBoolField(TEXT("hitch_windows_truncated"), HitchWindowsSeen > HitchFrames.Num());
	Root->SetNumberField(TEXT("hitch_windows_limit"), MaxHitchWindows);
	// Provider presence and exported rows are separate facts: a clean capture with no slow frames exports no scopes.
	TSharedRef<FJsonObject> TimingCoverage = MakeCoverageEntry(TimingProvider != nullptr, TimingEventCount);
	TimingCoverage->SetBoolField(TEXT("count_is_exported_subset"), true);
	TimingCoverage->SetNumberField(TEXT("timelines"), Timelines.Num());
	Coverage->SetObjectField(TEXT("timing"), TimingCoverage);

	const TraceServices::ICounterProvider* CounterProvider = Session->ReadProvider<TraceServices::ICounterProvider>(TraceServices::GetCounterProviderName());
	TArray<TSharedPtr<FJsonValue>> CounterSummaries;
	if (CounterProvider)
	{
		CounterProvider->EnumerateCounters([&](uint32 CounterId, const TraceServices::ICounter& Counter)
		{
			double First = 0.0, Last = 0.0, Minimum = DBL_MAX, Maximum = -DBL_MAX, Sum = 0.0;
			double FirstTime = 0.0, LastTime = 0.0;
			int64 Count = 0;
			auto AddValue = [&](double Time, double Value)
			{
				if (Count == 0) { First = Value; FirstTime = Time; }
				Last = Value; LastTime = Time; Minimum = FMath::Min(Minimum, Value); Maximum = FMath::Max(Maximum, Value); Sum += Value; ++Count;
			};
			// Counters cover the capture window only; per-frame counters (draw calls, primitives) also get a mean.
			if (Counter.IsFloatingPoint())
			{
				Counter.EnumerateFloatValues(Window.Start, Window.End, false, [&](double Time, double Value) { AddValue(Time, Value); });
			}
			else
			{
				Counter.EnumerateValues(Window.Start, Window.End, false, [&](double Time, int64 Value) { AddValue(Time, static_cast<double>(Value)); });
			}
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetNumberField(TEXT("id"), CounterId);
			Item->SetStringField(TEXT("name"), Counter.GetName() ? Counter.GetName() : TEXT(""));
			Item->SetStringField(TEXT("group"), Counter.GetGroup() ? Counter.GetGroup() : TEXT(""));
			Item->SetNumberField(TEXT("sample_count"), static_cast<double>(Count));
			// Per-frame counters (draw calls, primitives) restart every frame: read mean and max, not first/last/delta.
			Item->SetBoolField(TEXT("reset_every_frame"), Counter.IsResetEveryFrame());
			if (Count > 0)
			{
				Item->SetNumberField(TEXT("first"), First); Item->SetNumberField(TEXT("last"), Last);
				Item->SetNumberField(TEXT("min"), Minimum); Item->SetNumberField(TEXT("max"), Maximum);
				Item->SetNumberField(TEXT("delta"), Last - First);
				Item->SetNumberField(TEXT("mean"), Sum / static_cast<double>(Count));
				Item->SetNumberField(TEXT("first_seconds"), FirstTime); Item->SetNumberField(TEXT("last_seconds"), LastTime);
			}
			CounterSummaries.Add(JsonObjectValue(Item));
		});
	}
	Root->SetArrayField(TEXT("counters"), CounterSummaries);
	Coverage->SetObjectField(TEXT("counters"), MakeCoverageEntry(CounterProvider != nullptr, CounterSummaries.Num()));

	const TraceServices::IBookmarkProvider* BookmarkProvider = Session->ReadProvider<TraceServices::IBookmarkProvider>(TraceServices::GetBookmarkProviderName());
	TArray<TSharedPtr<FJsonValue>> Bookmarks;
	if (BookmarkProvider)
	{
		BookmarkProvider->EnumerateBookmarks(0.0, SessionDuration, [&](const TraceServices::FBookmark& Bookmark)
		{
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetNumberField(TEXT("time_seconds"), Bookmark.Time);
			Item->SetStringField(TEXT("text"), Bookmark.Text ? Bookmark.Text : TEXT(""));
			Item->SetNumberField(TEXT("callstack_id"), Bookmark.CallstackId);
			Bookmarks.Add(JsonObjectValue(Item));
		});
	}
	Root->SetArrayField(TEXT("bookmarks"), Bookmarks);
	Coverage->SetObjectField(TEXT("bookmarks"), MakeCoverageEntry(BookmarkProvider != nullptr, Bookmarks.Num()));

	// Rows seen versus kept for every capped export below, so a cap is never presented as the complete population.
	TSharedRef<FJsonObject> ExportLimits = MakeShared<FJsonObject>();
	auto WriteLimit = [&ExportLimits](const TCHAR* Name, int64 Seen, int64 Kept, int64 Limit)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetNumberField(TEXT("rows_seen"), static_cast<double>(Seen));
		Item->SetNumberField(TEXT("rows_kept"), static_cast<double>(Kept));
		Item->SetNumberField(TEXT("limit"), static_cast<double>(Limit));
		Item->SetBoolField(TEXT("truncated"), Seen > Kept);
		ExportLimits->SetObjectField(Name, Item);
	};

	const TraceServices::ITasksProvider* TasksProvider = TraceServices::ReadTasksProvider(*Session);
	TArray<TSharedPtr<FJsonValue>> Tasks;
	int64 TasksSeen = 0;
	if (TasksProvider)
	{
		TasksProvider->EnumerateTasks(Window.Start, Window.End, TraceServices::ETaskEnumerationOption::Alive,
			[&](const TraceServices::FTaskInfo& Task)
			{
				++TasksSeen;
				if (Tasks.Num() >= MaxTasks) { return TraceServices::ETaskEnumerationResult::Continue; }
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				// Missing timestamps (FTaskInfo::InvalidTimestamp) and durations are left out, never written as values.
				auto SetTime = [&Item](const TCHAR* Field, double Time)
				{
					if (Time != TraceServices::FTaskInfo::InvalidTimestamp && FMath::IsFinite(Time)) { Item->SetNumberField(Field, Time); }
				};
				auto SetDuration = [&Item](const TCHAR* Field, double Seconds)
				{
					if (Seconds >= 0.0) { Item->SetNumberField(Field, Seconds * 1000.0); }
				};
				Item->SetStringField(TEXT("id"), LexToString(Task.Id));
				Item->SetStringField(TEXT("name"), Task.DebugName ? Task.DebugName : TEXT(""));
				SetTime(TEXT("created_seconds"), Task.CreatedTimestamp);
				SetTime(TEXT("launched_seconds"), Task.LaunchedTimestamp);
				SetTime(TEXT("scheduled_seconds"), Task.ScheduledTimestamp);
				SetTime(TEXT("started_seconds"), Task.StartedTimestamp);
				SetTime(TEXT("finished_seconds"), Task.FinishedTimestamp);
				SetTime(TEXT("completed_seconds"), Task.CompletedTimestamp);
				SetDuration(TEXT("queue_delay_ms"), ValidTaskDuration(Task.ScheduledTimestamp, Task.StartedTimestamp));
				SetDuration(TEXT("execution_ms"), ValidTaskDuration(Task.StartedTimestamp, Task.FinishedTimestamp));
				Item->SetNumberField(TEXT("prerequisite_count"), Task.Prerequisites.Num());
				Item->SetNumberField(TEXT("subsequent_count"), Task.Subsequents.Num());
				Item->SetNumberField(TEXT("parent_count"), Task.ParentTasks.Num());
				Item->SetNumberField(TEXT("nested_count"), Task.NestedTasks.Num());
				Tasks.Add(JsonObjectValue(Item));
				return TraceServices::ETaskEnumerationResult::Continue;
			});
	}
	Root->SetArrayField(TEXT("tasks"), Tasks);
	WriteLimit(TEXT("tasks"), TasksSeen, Tasks.Num(), MaxTasks);
	Coverage->SetObjectField(TEXT("tasks"), MakeCoverageEntry(TasksProvider != nullptr, TasksSeen));

	const TraceServices::IFileActivityProvider* FileProvider = TraceServices::ReadFileActivityProvider(*Session);
	TArray<TSharedPtr<FJsonValue>> FileActivity;
	int64 FileOperationsOpenAtEnd = 0;
	if (FileProvider)
	{
		FileProvider->EnumerateFileActivity([&](const TraceServices::FFileInfo& File, const TraceServices::IFileActivityProvider::Timeline& Timeline)
		{
			int64 Count = 0, Failures = 0, OpenAtEnd = 0;
			uint64 Bytes = 0;
			double TotalMs = 0.0, MaxMs = 0.0;
			Timeline.EnumerateEvents(Window.Start, Window.End, [&](double Start, double End, uint32 Depth, TraceServices::FFileActivity* const& Activity)
			{
				(void)Depth;
				if (Activity)
				{
					// A read still in flight when the trace stopped has an infinite end; measure it to the window end and flag it.
					const bool bOpen = !FMath::IsFinite(End);
					const double EffectiveEnd = bOpen ? Window.End : FMath::Min(End, Window.End);
					++Count; Failures += Activity->Failed ? 1 : 0; Bytes += Activity->ActualSize; OpenAtEnd += bOpen ? 1 : 0;
					const double DurationMs = FMath::Max(0.0, EffectiveEnd - FMath::Max(Start, Window.Start)) * 1000.0;
					TotalMs += DurationMs; MaxMs = FMath::Max(MaxMs, DurationMs);
				}
				return TraceServices::EEventEnumerate::Continue;
			});
			if (Count > 0)
			{
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("path"), File.Path ? File.Path : TEXT(""));
				Item->SetNumberField(TEXT("operation_count"), static_cast<double>(Count));
				Item->SetNumberField(TEXT("failure_count"), static_cast<double>(Failures));
				Item->SetNumberField(TEXT("actual_bytes"), static_cast<double>(Bytes));
				Item->SetNumberField(TEXT("total_ms"), TotalMs);
				Item->SetNumberField(TEXT("max_ms"), MaxMs);
				if (OpenAtEnd > 0) { Item->SetNumberField(TEXT("open_at_trace_end"), static_cast<double>(OpenAtEnd)); }
				FileOperationsOpenAtEnd += OpenAtEnd;
				FileActivity.Add(JsonObjectValue(Item));
			}
			return true;
		});
	}
	ExportLimits->SetNumberField(TEXT("file_operations_open_at_trace_end"), static_cast<double>(FileOperationsOpenAtEnd));
	Root->SetArrayField(TEXT("file_activity"), FileActivity);
	Coverage->SetObjectField(TEXT("file_activity"), MakeCoverageEntry(FileProvider != nullptr, FileActivity.Num()));

	const TraceServices::ILoadTimeProfilerProvider* LoadTimeProvider = TraceServices::ReadLoadTimeProfilerProvider(*Session);
	TArray<TSharedPtr<FJsonValue>> PackageLoads;
	TArray<TSharedPtr<FJsonValue>> ExportLoads;
	TArray<TSharedPtr<FJsonValue>> LoadRequests;
	int64 PackageRowsSeen = 0, ExportRowsSeen = 0, RequestRowsSeen = 0;
	if (LoadTimeProvider)
	{
		// Rows are ranked over the whole capture window before the cap, so "top" is the top of everything loaded in the capture.
		TUniquePtr<TraceServices::ITable<TraceServices::FPackagesTableRow>> PackageTable(LoadTimeProvider->CreatePackageDetailsTable(Window.Start, Window.End));
		if (PackageTable)
		{
			TArray<const TraceServices::FPackagesTableRow*> Rows;
			TUniquePtr<TraceServices::ITableReader<TraceServices::FPackagesTableRow>> Reader(PackageTable->CreateReader());
			for (; Reader && Reader->IsValid(); Reader->NextRow())
			{
				const TraceServices::FPackagesTableRow* Row = Reader->GetCurrentRow();
				if (Row && Row->PackageInfo) { Rows.Add(Row); }
			}
			PackageRowsSeen = Rows.Num();
			Rows.Sort([](const TraceServices::FPackagesTableRow& A, const TraceServices::FPackagesTableRow& B)
			{
				return A.MainThreadTime + A.AsyncLoadingThreadTime > B.MainThreadTime + B.AsyncLoadingThreadTime;
			});
			for (int32 Index = 0; Index < FMath::Min(MaxLoadRows, Rows.Num()); ++Index)
			{
				const TraceServices::FPackagesTableRow* Row = Rows[Index];
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetNumberField(TEXT("id"), Row->PackageInfo->Id);
				Item->SetStringField(TEXT("package"), Row->PackageInfo->Name ? Row->PackageInfo->Name : TEXT(""));
				Item->SetStringField(TEXT("request_id"), LexToString(Row->PackageInfo->RequestId));
				Item->SetNumberField(TEXT("serialized_bytes"), static_cast<double>(Row->TotalSerializedSize));
				Item->SetNumberField(TEXT("header_bytes"), static_cast<double>(Row->SerializedHeaderSize));
				Item->SetNumberField(TEXT("export_count"), static_cast<double>(Row->SerializedExportsCount));
				Item->SetNumberField(TEXT("export_bytes"), static_cast<double>(Row->SerializedExportsSize));
				Item->SetNumberField(TEXT("main_thread_ms"), Row->MainThreadTime * 1000.0);
				Item->SetNumberField(TEXT("async_loading_thread_ms"), Row->AsyncLoadingThreadTime * 1000.0);
				Item->SetNumberField(TEXT("total_ms"), (Row->MainThreadTime + Row->AsyncLoadingThreadTime) * 1000.0);
				PackageLoads.Add(JsonObjectValue(Item));
			}
		}

		TUniquePtr<TraceServices::ITable<TraceServices::FExportsTableRow>> ExportTable(LoadTimeProvider->CreateExportDetailsTable(Window.Start, Window.End));
		if (ExportTable)
		{
			TArray<const TraceServices::FExportsTableRow*> Rows;
			TUniquePtr<TraceServices::ITableReader<TraceServices::FExportsTableRow>> Reader(ExportTable->CreateReader());
			for (; Reader && Reader->IsValid(); Reader->NextRow())
			{
				const TraceServices::FExportsTableRow* Row = Reader->GetCurrentRow();
				if (Row && Row->ExportInfo) { Rows.Add(Row); }
			}
			ExportRowsSeen = Rows.Num();
			Rows.Sort([](const TraceServices::FExportsTableRow& A, const TraceServices::FExportsTableRow& B)
			{
				return A.MainThreadTime + A.AsyncLoadingThreadTime > B.MainThreadTime + B.AsyncLoadingThreadTime;
			});
			for (int32 Index = 0; Index < FMath::Min(MaxLoadRows, Rows.Num()); ++Index)
			{
				const TraceServices::FExportsTableRow* Row = Rows[Index];
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetNumberField(TEXT("id"), Row->ExportInfo->Id);
				Item->SetStringField(TEXT("package"), Row->ExportInfo->Package && Row->ExportInfo->Package->Name ? Row->ExportInfo->Package->Name : TEXT(""));
				Item->SetStringField(TEXT("class"), Row->ExportInfo->Class && Row->ExportInfo->Class->Name ? Row->ExportInfo->Class->Name : TEXT(""));
				Item->SetStringField(TEXT("event"), TraceServices::GetLoadTimeProfilerObjectEventTypeString(Row->EventType));
				Item->SetNumberField(TEXT("serialized_bytes"), static_cast<double>(Row->SerializedSize));
				Item->SetNumberField(TEXT("main_thread_ms"), Row->MainThreadTime * 1000.0);
				Item->SetNumberField(TEXT("async_loading_thread_ms"), Row->AsyncLoadingThreadTime * 1000.0);
				Item->SetNumberField(TEXT("total_ms"), (Row->MainThreadTime + Row->AsyncLoadingThreadTime) * 1000.0);
				ExportLoads.Add(JsonObjectValue(Item));
			}
		}

		TUniquePtr<TraceServices::ITable<TraceServices::FRequestsTableRow>> RequestTable(LoadTimeProvider->CreateRequestsTable(Window.Start, Window.End));
		if (RequestTable)
		{
			TArray<const TraceServices::FRequestsTableRow*> Rows;
			TUniquePtr<TraceServices::ITableReader<TraceServices::FRequestsTableRow>> Reader(RequestTable->CreateReader());
			for (; Reader && Reader->IsValid(); Reader->NextRow())
			{
				if (const TraceServices::FRequestsTableRow* Row = Reader->GetCurrentRow()) { Rows.Add(Row); }
			}
			RequestRowsSeen = Rows.Num();
			Rows.Sort([](const TraceServices::FRequestsTableRow& A, const TraceServices::FRequestsTableRow& B) { return A.Duration > B.Duration; });
			for (int32 Index = 0; Index < FMath::Min(MaxLoadRows, Rows.Num()); ++Index)
			{
				const TraceServices::FRequestsTableRow* Row = Rows[Index];
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("id"), LexToString(Row->Id));
				Item->SetStringField(TEXT("name"), Row->Name ? Row->Name : TEXT(""));
				Item->SetNumberField(TEXT("start_seconds"), Row->StartTime);
				Item->SetNumberField(TEXT("duration_ms"), Row->Duration * 1000.0);
				Item->SetNumberField(TEXT("package_count"), Row->Packages.Num());
				LoadRequests.Add(JsonObjectValue(Item));
			}
		}
	}
	Root->SetArrayField(TEXT("package_loads"), PackageLoads);
	Root->SetArrayField(TEXT("export_loads"), ExportLoads);
	Root->SetArrayField(TEXT("load_requests"), LoadRequests);
	WriteLimit(TEXT("package_loads"), PackageRowsSeen, PackageLoads.Num(), MaxLoadRows);
	WriteLimit(TEXT("export_loads"), ExportRowsSeen, ExportLoads.Num(), MaxLoadRows);
	WriteLimit(TEXT("load_requests"), RequestRowsSeen, LoadRequests.Num(), MaxLoadRows);
	Coverage->SetObjectField(TEXT("load_time"), MakeCoverageEntry(LoadTimeProvider != nullptr, PackageRowsSeen + RequestRowsSeen));

	const TraceServices::ILogProvider* LogProvider = Session->ReadProvider<TraceServices::ILogProvider>(TraceServices::GetLogProviderName());
	TArray<TSharedPtr<FJsonValue>> Logs;
	int64 WarningLogsSeen = 0;
	if (LogProvider)
	{
		// Warnings and errors inside the capture window; the count of all messages tells presence apart from a quiet capture.
		LogProvider->EnumerateMessages(Window.Start, Window.End, [&](const TraceServices::FLogMessageInfo& Message)
		{
			if (Message.Verbosity > ELogVerbosity::Warning) { return; }
			++WarningLogsSeen;
			if (Logs.Num() >= MaxLogMessages) { return; }
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetNumberField(TEXT("time_seconds"), Message.Time);
			Item->SetStringField(TEXT("category"), Message.Category && Message.Category->Name ? Message.Category->Name : TEXT(""));
			Item->SetStringField(TEXT("message"), Message.Message ? Message.Message : TEXT(""));
			Item->SetNumberField(TEXT("verbosity"), static_cast<uint8>(Message.Verbosity));
			Logs.Add(JsonObjectValue(Item));
		});
		Root->SetNumberField(TEXT("log_messages_total"), static_cast<double>(LogProvider->GetMessageCount()));
	}
	Root->SetArrayField(TEXT("logs"), Logs);
	WriteLimit(TEXT("warning_logs"), WarningLogsSeen, Logs.Num(), MaxLogMessages);
	TSharedRef<FJsonObject> LogCoverage = MakeCoverageEntry(LogProvider != nullptr, LogProvider ? static_cast<int64>(LogProvider->GetMessageCount()) : 0);
	LogCoverage->SetNumberField(TEXT("warnings_and_errors_in_window"), static_cast<double>(WarningLogsSeen));
	Coverage->SetObjectField(TEXT("logs"), LogCoverage);

	const TraceServices::IContextSwitchesProvider* ContextProvider = TraceServices::ReadContextSwitchesProvider(*Session);
	TArray<TSharedPtr<FJsonValue>> ContextSwitches;
	bool bHasContextData = false;
	if (ContextProvider)
	{
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 8, 0)
		TraceServices::FProviderReadScopeLock ContextReadScope(*ContextProvider);
#endif
		// UE 5.6 checks the analysis session read scope already held by this commandlet.
		bHasContextData = ContextProvider->HasData();
		if (bHasContextData && ThreadProvider)
		{
			// Over the capture window: the ratio divides by the window, never by process uptime.
			const double WindowSeconds = FMath::Max(0.0, Window.End - Window.Start);
			ThreadProvider->EnumerateThreads([&](const TraceServices::FThreadInfo& Thread)
			{
				int64 Count = 0;
				double RunningMs = 0.0;
				ContextProvider->EnumerateContextSwitches(Thread.Id, Window.Start, Window.End, [&](const TraceServices::FContextSwitch& Context)
				{
					const double End = FMath::IsFinite(Context.End) ? FMath::Min(Context.End, Window.End) : Window.End;
					++Count; RunningMs += FMath::Max(0.0, End - FMath::Max(Context.Start, Window.Start)) * 1000.0;
					return TraceServices::EContextSwitchEnumerationResult::Continue;
				});
				if (Count > 0)
				{
					TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
					Item->SetNumberField(TEXT("thread_id"), Thread.Id);
					Item->SetStringField(TEXT("thread"), Thread.Name ? Thread.Name : TEXT(""));
					Item->SetNumberField(TEXT("switch_count"), static_cast<double>(Count));
					Item->SetNumberField(TEXT("running_ms"), RunningMs);
					Item->SetNumberField(TEXT("scheduled_ratio"), WindowSeconds > 0.0 ? RunningMs / (WindowSeconds * 1000.0) : 0.0);
					ContextSwitches.Add(JsonObjectValue(Item));
				}
			});
		}
	}
	Root->SetArrayField(TEXT("context_switches"), ContextSwitches);
	Coverage->SetObjectField(TEXT("context_switches"), MakeCoverageEntry(bHasContextData, ContextSwitches.Num()));
	Root->SetObjectField(TEXT("export_limits"), ExportLimits);

	TArray<TSharedPtr<FJsonValue>> StackFrames;
	TArray<TSharedPtr<FJsonValue>> StackSampleEvents;
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 8, 0)
	const TraceServices::IStackSamplesProvider* StackSamplesProvider = TraceServices::ReadStackSamplesProvider(*Session);
	TMap<uint32, TSharedPtr<FJsonObject>> StackFrameByTimer;
	if (StackSamplesProvider)
	{
		TraceServices::FProviderReadScopeLock StackSamplesReadScope(*StackSamplesProvider);
		StackSamplesProvider->EnumerateStackFrames([&](const TraceServices::FStackSampleFrame& Frame)
		{
			if (StackFrames.Num() >= MaxStackFrames)
			{
				return;
			}
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetNumberField(TEXT("timer_id"), Frame.TimerId);
			Item->SetStringField(TEXT("address"), FString::Printf(TEXT("0x%llx"), Frame.Address));
			Item->SetStringField(TEXT("module"), Frame.Symbol && Frame.Symbol->Module ? Frame.Symbol->Module : TEXT(""));
			Item->SetStringField(TEXT("symbol"), Frame.Symbol && Frame.Symbol->Name ? Frame.Symbol->Name : TEXT(""));
			Item->SetStringField(TEXT("file"), Frame.Symbol && Frame.Symbol->File ? Frame.Symbol->File : TEXT(""));
			Item->SetNumberField(TEXT("line"), Frame.Symbol ? Frame.Symbol->Line : 0);
			StackFrameByTimer.Add(Frame.TimerId, Item);
			StackFrames.Add(JsonObjectValue(Item));
		});

		StackSamplesProvider->EnumerateThreads([&](const TraceServices::FStackSampleThread& Thread)
		{
			if (!Thread.Timeline || StackSampleEvents.Num() >= MaxStackSampleEvents)
			{
				return;
			}
			for (const FHitchFrame& Hitch : HitchFrames)
			{
				Thread.Timeline->EnumerateEvents(Hitch.Start, Hitch.End,
					[&](double Start, double End, uint32 Depth, const TraceServices::FTimingProfilerEvent& Event)
					{
						if (StackSampleEvents.Num() >= MaxStackSampleEvents)
						{
							return TraceServices::EEventEnumerate::Stop;
						}
						const TSharedPtr<FJsonObject>* FrameInfo = StackFrameByTimer.Find(Event.TimerIndex);
						TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
						Item->SetNumberField(TEXT("frame_index"), static_cast<double>(Hitch.Index));
						Item->SetNumberField(TEXT("system_thread_id"), Thread.SystemThreadId);
						Item->SetStringField(TEXT("thread"), Thread.Name ? Thread.Name : TEXT(""));
						Item->SetNumberField(TEXT("timer_id"), Event.TimerIndex);
						Item->SetStringField(TEXT("symbol"), FrameInfo && FrameInfo->IsValid() ? (*FrameInfo)->GetStringField(TEXT("symbol")) : TEXT(""));
						Item->SetStringField(TEXT("module"), FrameInfo && FrameInfo->IsValid() ? (*FrameInfo)->GetStringField(TEXT("module")) : TEXT(""));
						Item->SetNumberField(TEXT("start_seconds"), Start);
						Item->SetNumberField(TEXT("end_seconds"), End);
						Item->SetNumberField(TEXT("duration_ms"), FMath::Max(0.0, End - Start) * 1000.0);
						Item->SetNumberField(TEXT("depth"), Depth);
						StackSampleEvents.Add(JsonObjectValue(Item));
						return TraceServices::EEventEnumerate::Continue;
					});
			}
		});
	}
	Coverage->SetObjectField(TEXT("stack_samples"), MakeCoverageEntry(StackSamplesProvider != nullptr, StackSampleEvents.Num()));
#else
	// Before UE 5.8 the stack samples provider only holds raw per-thread addresses, with no frames or timelines.
	Coverage->SetObjectField(TEXT("stack_samples"), MakeUnsupportedCoverageEntry());
#endif
	Root->SetArrayField(TEXT("stack_frames"), StackFrames);
	Root->SetArrayField(TEXT("stack_sample_events"), StackSampleEvents);

	const TraceServices::IMemoryProvider* MemoryProvider = TraceServices::ReadMemoryProvider(*Session);
	TArray<TSharedPtr<FJsonValue>> MemoryTags;
	bool bMemoryInitialized = false;
	if (MemoryProvider)
	{
		TraceServices::FProviderReadScopeLock MemoryReadScope(*MemoryProvider);
		bMemoryInitialized = MemoryProvider->IsInitialized();
		if (bMemoryInitialized)
		{
			TArray<TraceServices::FMemoryTrackerInfo> Trackers;
			MemoryProvider->EnumerateTrackers([&](const TraceServices::FMemoryTrackerInfo& Tracker) { Trackers.Add(Tracker); });
			MemoryProvider->EnumerateTags([&](const TraceServices::FMemoryTagInfo& Tag)
			{
				for (const TraceServices::FMemoryTrackerInfo& Tracker : Trackers)
				{
					if ((Tag.Trackers & (1ull << Tracker.Id)) == 0 || MemoryProvider->GetTagSampleCount(Tracker.Id, Tag.Id) == 0) { continue; }
					int64 First = 0, Last = 0, Peak = MIN_int64, Count = 0;
					MemoryProvider->EnumerateTagSamples(Tracker.Id, Tag.Id, 0.0, SessionDuration, true,
						[&](double Time, double Duration, const TraceServices::FMemoryTagSample& Sample)
						{
							(void)Time; (void)Duration;
							if (Count == 0) { First = Sample.Value; }
							Last = Sample.Value; Peak = FMath::Max(Peak, Sample.Value); ++Count;
						});
					TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
					Item->SetStringField(TEXT("tracker"), Tracker.Name);
					Item->SetStringField(TEXT("tag"), Tag.Name);
					Item->SetStringField(TEXT("tag_id"), LexToString(Tag.Id));
					Item->SetNumberField(TEXT("sample_count"), static_cast<double>(Count));
					Item->SetNumberField(TEXT("first_bytes"), static_cast<double>(First));
					Item->SetNumberField(TEXT("last_bytes"), static_cast<double>(Last));
					Item->SetNumberField(TEXT("peak_bytes"), static_cast<double>(Peak));
					Item->SetNumberField(TEXT("growth_bytes"), static_cast<double>(Last - First));
					MemoryTags.Add(JsonObjectValue(Item));
				}
			});
		}
	}
	Root->SetArrayField(TEXT("memory_tags"), MemoryTags);
	Coverage->SetObjectField(TEXT("memory_tags"), MakeCoverageEntry(bMemoryInitialized, MemoryTags.Num()));

	const TraceServices::IAllocationsProvider* AllocationsProvider = TraceServices::ReadAllocationsProvider(*Session);
	TSharedRef<FJsonObject> AllocationSummary = MakeShared<FJsonObject>();
	bool bAllocationsInitialized = false;
	if (AllocationsProvider)
	{
		TraceServices::FProviderReadScopeLock AllocationsReadScope(*AllocationsProvider);
		bAllocationsInitialized = AllocationsProvider->IsInitialized();
		if (bAllocationsInitialized && AllocationsProvider->GetTimelineNumPoints() > 0)
		{
			const int32 LastIndex = AllocationsProvider->GetTimelineNumPoints() - 1;
			uint64 FirstBytes = 0, LastBytes = 0, PeakBytes = 0;
			int64 Count = 0;
			auto AddPoint = [&](double Time, double Duration, uint64 Value)
			{
				(void)Time; (void)Duration;
				if (Count == 0) { FirstBytes = Value; } LastBytes = Value; PeakBytes = FMath::Max(PeakBytes, Value); ++Count;
			};
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 7, 0)
			AllocationsProvider->EnumerateTimeline(TraceServices::IAllocationsProvider::ETimelineU64::MaxTotalAllocatedMemory, 0, LastIndex, AddPoint);
#else
			AllocationsProvider->EnumerateMaxTotalAllocatedMemoryTimeline(0, LastIndex, AddPoint);
#endif
			AllocationSummary->SetNumberField(TEXT("first_bytes"), static_cast<double>(FirstBytes));
			AllocationSummary->SetNumberField(TEXT("last_bytes"), static_cast<double>(LastBytes));
			AllocationSummary->SetNumberField(TEXT("peak_bytes"), static_cast<double>(PeakBytes));
			AllocationSummary->SetNumberField(TEXT("growth_bytes"), static_cast<double>(LastBytes) - static_cast<double>(FirstBytes));
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 8, 0)
			AllocationSummary->SetBoolField(TEXT("has_allocation_events"), AllocationsProvider->HasAllocationEvents());
#endif
		}
	}
	Root->SetObjectField(TEXT("allocation_summary"), AllocationSummary);
	Coverage->SetObjectField(TEXT("allocations"), MakeCoverageEntry(bAllocationsInitialized));

	TArray<TSharedPtr<FJsonValue>> ObjectSnapshots;
	TArray<TSharedPtr<FJsonValue>> ObjectClasses;
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 8, 0)
	const TraceServices::IObjectProvider* ObjectProvider = TraceServices::ReadObjectProvider(*Session);
	if (ObjectProvider)
	{
		TraceServices::FProviderReadScopeLock ObjectReadScope(*ObjectProvider);
		const TraceServices::IObjectSnapshot* LastSnapshot = nullptr;
		ObjectProvider->EnumerateSnapshots([&](const TraceServices::IObjectSnapshot& Snapshot)
		{
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetNumberField(TEXT("id"), Snapshot.GetId());
			Item->SetNumberField(TEXT("start_seconds"), Snapshot.GetStartTime());
			Item->SetNumberField(TEXT("end_seconds"), Snapshot.GetEndTime());
			Item->SetNumberField(TEXT("object_count"), Snapshot.GetObjectCount());
			Item->SetNumberField(TEXT("object_array_size"), Snapshot.GetObjectArrayNum());
			Item->SetNumberField(TEXT("reference_count"), Snapshot.GetNumReferences());
			Item->SetNumberField(TEXT("traced_object_count"), Snapshot.GetTracedObjectArrayNum());
			Item->SetBoolField(TEXT("has_total_memory_sizes"), Snapshot.HasTotalMemorySizes());
			ObjectSnapshots.Add(JsonObjectValue(Item));
			LastSnapshot = &Snapshot;
			return true;
		});

		if (LastSnapshot)
		{
			TMap<FString, FObjectClassSummary> ClassSummaries;
			ObjectProvider->EnumerateObjects(LastSnapshot->GetId(), [&](const TraceServices::FObjectInfo& Object)
			{
				const TraceServices::FObjectInfo* ClassObject = LastSnapshot->GetObject(Object.ClassId);
				const FString ClassName = ClassObject && ClassObject->Name ? ClassObject->Name : TEXT("<unknown>");
				FObjectClassSummary& Summary = ClassSummaries.FindOrAdd(ClassName);
				++Summary.Count;
				Summary.SystemBytes += Object.SystemMemoryBytes;
				Summary.VideoBytes += Object.VideoMemoryBytes;
				return ClassSummaries.Num() <= MaxObjectClasses;
			});
			for (const TPair<FString, FObjectClassSummary>& Pair : ClassSummaries)
			{
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("class"), Pair.Key);
				Item->SetNumberField(TEXT("count"), static_cast<double>(Pair.Value.Count));
				Item->SetNumberField(TEXT("system_bytes"), static_cast<double>(Pair.Value.SystemBytes));
				Item->SetNumberField(TEXT("video_bytes"), static_cast<double>(Pair.Value.VideoBytes));
				Item->SetNumberField(TEXT("total_bytes"), static_cast<double>(Pair.Value.SystemBytes + Pair.Value.VideoBytes));
				ObjectClasses.Add(JsonObjectValue(Item));
			}
			ObjectClasses.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
			{
				const TSharedPtr<FJsonObject> Left = A->AsObject();
				const TSharedPtr<FJsonObject> Right = B->AsObject();
				const double LeftBytes = Left->GetNumberField(TEXT("total_bytes"));
				const double RightBytes = Right->GetNumberField(TEXT("total_bytes"));
				return LeftBytes == RightBytes
					? Left->GetNumberField(TEXT("count")) > Right->GetNumberField(TEXT("count"))
					: LeftBytes > RightBytes;
			});
		}
	}
	Coverage->SetObjectField(TEXT("objects"), MakeCoverageEntry(ObjectProvider != nullptr, ObjectSnapshots.Num()));
#else
	// TraceServices has no object provider before UE 5.8.
	Coverage->SetObjectField(TEXT("objects"), MakeUnsupportedCoverageEntry());
#endif
	Root->SetArrayField(TEXT("object_snapshots"), ObjectSnapshots);
	Root->SetArrayField(TEXT("object_classes"), ObjectClasses);

	ExtractNetworkEvidence(*Session, SessionDuration, Window.TraceStart, HitchFrames, Root, Coverage);

	const TraceServices::IScreenshotProvider* ScreenshotProvider = Session->ReadProvider<TraceServices::IScreenshotProvider>(TraceServices::GetScreenshotProviderName());
	Coverage->SetObjectField(TEXT("screenshots"), MakeCoverageEntry(ScreenshotProvider != nullptr));
	Root->SetObjectField(TEXT("coverage"), Coverage);
	const int32 NonFiniteReplaced = ReplaceNonFiniteNumbers(MakeShared<FJsonValueObject>(Root));
	Root->SetNumberField(TEXT("non_finite_values_replaced_with_null"), NonFiniteReplaced);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.CreateDirectoryTree(*FPaths::GetPath(OutputPath)))
	{
		UE_LOG(LogTemp, Error, TEXT("PerfSentinelAnalyze could not create output directory: %s"), *FPaths::GetPath(OutputPath));
		return 5;
	}

	FString Json;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	if (!FFileHelper::SaveStringToFile(Json, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Error, TEXT("PerfSentinelAnalyze failed to write %s"), *OutputPath);
		return 6;
	}

	UE_LOG(LogTemp, Display, TEXT("PerfSentinelAnalyze wrote native evidence: %s"), *OutputPath);
	return 0;
}
