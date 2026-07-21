// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PerfSentinelAnalyzeCommandlet.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Common/ProviderLock.h"
#include "HAL/PlatformFileManager.h"
#include "TraceServices/ITraceServicesModule.h"
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
#if !UE_VERSION_OLDER_THAN(5, 8, 0)
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

struct FHitchFrame
{
	uint64 Index = 0;
	double Start = 0.0;
	double End = 0.0;
	double DurationMs = 0.0;
};

struct FObjectClassSummary
{
	int64 Count = 0;
	uint64 SystemBytes = 0;
	uint64 VideoBytes = 0;
};

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
#if UE_VERSION_OLDER_THAN(5, 8, 0)
	// This commandlet's trace extraction targets the UE 5.8 TraceServices API
	// (timeline readers, stack-sample / object providers, allocation timelines).
	// Those APIs are unavailable on earlier engines, so report unsupported here
	// rather than failing to compile. The runtime PerfSentinel module is unaffected.
	(void)Params;
	UE_LOG(LogTemp, Warning,
		TEXT("PerfSentinelAnalyze requires UE 5.8+ TraceServices; skipping extraction on this engine version."));
	return 0;
#else
	FString TracePath;
	FString OutputPath;
	FParse::Value(*Params, TEXT("Trace="), TracePath);
	FParse::Value(*Params, TEXT("Out="), OutputPath);
	double HitchThresholdMs = 50.0;
	FParse::Value(*Params, TEXT("HitchThresholdMs="), HitchThresholdMs);

	TracePath.TrimQuotesInline();
	OutputPath.TrimQuotesInline();
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
	Root->SetNumberField(TEXT("schema_version"), 2);
	Root->SetStringField(TEXT("extractor"), TEXT("PerfSentinelTraceServices-UE5.8"));
	Root->SetStringField(TEXT("trace"), TracePath);
	Root->SetNumberField(TEXT("duration_seconds"), SessionDuration);
	Root->SetNumberField(TEXT("hitch_threshold_ms"), HitchThresholdMs);

	TSharedRef<FJsonObject> Coverage = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> GameFrames;
	TArray<TSharedPtr<FJsonValue>> RenderFrames;
	TArray<FHitchFrame> HitchFrames;

	const TraceServices::IFrameProvider* FrameProvider = Session->ReadProvider<TraceServices::IFrameProvider>(TraceServices::GetFrameProviderName());
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
					HitchFrames.Add({ Frame.Index, Frame.StartTime, EndTime, DurationMs });
				}
			});
		};
		ExtractFrames(TraceFrameType_Game, GameFrames, true);
		ExtractFrames(TraceFrameType_Rendering, RenderFrames, false);
	}
	HitchFrames.Sort([](const FHitchFrame& A, const FHitchFrame& B) { return A.DurationMs > B.DurationMs; });
	if (HitchFrames.Num() > MaxHitchWindows)
	{
		HitchFrames.SetNum(MaxHitchWindows, EAllowShrinking::No);
	}
	Root->SetArrayField(TEXT("game_frames"), GameFrames);
	Root->SetArrayField(TEXT("render_frames"), RenderFrames);
	Coverage->SetObjectField(TEXT("frames"), MakeCoverageEntry(FrameProvider != nullptr, GameFrames.Num()));

	const TraceServices::IThreadProvider* ThreadProvider = Session->ReadProvider<TraceServices::IThreadProvider>(TraceServices::GetThreadProviderName());
	const TraceServices::ITimingProfilerProvider* TimingProvider = TraceServices::ReadTimingProfilerProvider(*Session);
	TMap<uint32, FString> TimerNames;
	if (TimingProvider)
	{
		const TraceServices::ITimingProfilerTimerReader& Reader = TimingProvider->GetTimerReader();
		for (uint32 TimerId = 0; TimerId < Reader.GetTimerCount(); ++TimerId)
		{
			if (const TraceServices::FTimingProfilerTimer* Timer = Reader.GetTimer(TimerId))
			{
				TimerNames.Add(TimerId, Timer->Name ? Timer->Name : TEXT("<unnamed>"));
			}
		}
	}

	TArray<TSharedPtr<FJsonValue>> Threads;
	TArray<TSharedPtr<FJsonValue>> TimingEvents;
	int32 TimingEventCount = 0;
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
			if (!TimingProvider || !TimingProvider->GetCpuThreadTimelineIndex(Thread.Id, TimelineIndex))
			{
				return;
			}
			TimingProvider->ReadTimeline(TimelineIndex, [&](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
			{
				for (const FHitchFrame& Hitch : HitchFrames)
				{
					Timeline.EnumerateEvents(Hitch.Start, Hitch.End,
						[&](double Start, double End, uint32 Depth, const TraceServices::FTimingProfilerEvent& Event)
						{
							if (TimingEventCount >= MaxTimingEvents || (End - Start) < 0.00005)
							{
								return TimingEventCount >= MaxTimingEvents ? TraceServices::EEventEnumerate::Stop : TraceServices::EEventEnumerate::Continue;
							}
							TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
							Item->SetNumberField(TEXT("frame_index"), static_cast<double>(Hitch.Index));
							Item->SetNumberField(TEXT("thread_id"), Thread.Id);
							Item->SetStringField(TEXT("thread"), Thread.Name ? Thread.Name : TEXT(""));
							Item->SetNumberField(TEXT("timer_id"), Event.TimerIndex);
							Item->SetStringField(TEXT("timer"), TimerNames.FindRef(Event.TimerIndex));
							Item->SetNumberField(TEXT("start_seconds"), Start);
							Item->SetNumberField(TEXT("end_seconds"), End);
							Item->SetNumberField(TEXT("duration_ms"), (End - Start) * 1000.0);
							Item->SetNumberField(TEXT("depth"), Depth);
							TimingEvents.Add(JsonObjectValue(Item));
							++TimingEventCount;
							return TraceServices::EEventEnumerate::Continue;
						});
				}
			});
		});
	}

	if (TimingProvider && TimingEventCount < MaxTimingEvents)
	{
		uint32 GpuTimelineIndex = ~0u;
		if (TimingProvider->GetGpuTimelineIndex(GpuTimelineIndex))
		{
			TimingProvider->ReadTimeline(GpuTimelineIndex, [&](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
			{
				for (const FHitchFrame& Hitch : HitchFrames)
				{
					Timeline.EnumerateEvents(Hitch.Start, Hitch.End,
						[&](double Start, double End, uint32 Depth, const TraceServices::FTimingProfilerEvent& Event)
						{
							if (TimingEventCount >= MaxTimingEvents) { return TraceServices::EEventEnumerate::Stop; }
							TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
							Item->SetNumberField(TEXT("frame_index"), static_cast<double>(Hitch.Index));
							Item->SetNumberField(TEXT("thread_id"), -1);
							Item->SetStringField(TEXT("thread"), TEXT("GPU"));
							Item->SetNumberField(TEXT("timer_id"), Event.TimerIndex);
							Item->SetStringField(TEXT("timer"), TimerNames.FindRef(Event.TimerIndex));
							Item->SetNumberField(TEXT("start_seconds"), Start);
							Item->SetNumberField(TEXT("end_seconds"), End);
							Item->SetNumberField(TEXT("duration_ms"), (End - Start) * 1000.0);
							Item->SetNumberField(TEXT("depth"), Depth);
							TimingEvents.Add(JsonObjectValue(Item));
							++TimingEventCount;
							return TraceServices::EEventEnumerate::Continue;
						});
				}
			});
		}
	}
	Root->SetArrayField(TEXT("threads"), Threads);
	Root->SetArrayField(TEXT("timing_events"), TimingEvents);
	Coverage->SetObjectField(TEXT("timing"), MakeCoverageEntry(TimingProvider != nullptr, TimingEventCount));

	const TraceServices::ICounterProvider* CounterProvider = Session->ReadProvider<TraceServices::ICounterProvider>(TraceServices::GetCounterProviderName());
	TArray<TSharedPtr<FJsonValue>> CounterSummaries;
	if (CounterProvider)
	{
		CounterProvider->EnumerateCounters([&](uint32 CounterId, const TraceServices::ICounter& Counter)
		{
			double First = 0.0, Last = 0.0, Minimum = DBL_MAX, Maximum = -DBL_MAX;
			double FirstTime = 0.0, LastTime = 0.0;
			int64 Count = 0;
			auto AddValue = [&](double Time, double Value)
			{
				if (Count == 0) { First = Value; FirstTime = Time; }
				Last = Value; LastTime = Time; Minimum = FMath::Min(Minimum, Value); Maximum = FMath::Max(Maximum, Value); ++Count;
			};
			if (Counter.IsFloatingPoint())
			{
				Counter.EnumerateFloatValues(0.0, SessionDuration, true, [&](double Time, double Value) { AddValue(Time, Value); });
			}
			else
			{
				Counter.EnumerateValues(0.0, SessionDuration, true, [&](double Time, int64 Value) { AddValue(Time, static_cast<double>(Value)); });
			}
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetNumberField(TEXT("id"), CounterId);
			Item->SetStringField(TEXT("name"), Counter.GetName() ? Counter.GetName() : TEXT(""));
			Item->SetStringField(TEXT("group"), Counter.GetGroup() ? Counter.GetGroup() : TEXT(""));
			Item->SetNumberField(TEXT("sample_count"), static_cast<double>(Count));
			if (Count > 0)
			{
				Item->SetNumberField(TEXT("first"), First); Item->SetNumberField(TEXT("last"), Last);
				Item->SetNumberField(TEXT("min"), Minimum); Item->SetNumberField(TEXT("max"), Maximum);
				Item->SetNumberField(TEXT("delta"), Last - First);
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

	const TraceServices::ITasksProvider* TasksProvider = TraceServices::ReadTasksProvider(*Session);
	TArray<TSharedPtr<FJsonValue>> Tasks;
	if (TasksProvider)
	{
		TasksProvider->EnumerateTasks(0.0, SessionDuration, TraceServices::ETaskEnumerationOption::Alive,
			[&](const TraceServices::FTaskInfo& Task)
			{
				if (Tasks.Num() >= MaxTasks) { return TraceServices::ETaskEnumerationResult::Stop; }
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("id"), LexToString(Task.Id));
				Item->SetStringField(TEXT("name"), Task.DebugName ? Task.DebugName : TEXT(""));
				Item->SetNumberField(TEXT("created_seconds"), Task.CreatedTimestamp);
				Item->SetNumberField(TEXT("launched_seconds"), Task.LaunchedTimestamp);
				Item->SetNumberField(TEXT("scheduled_seconds"), Task.ScheduledTimestamp);
				Item->SetNumberField(TEXT("started_seconds"), Task.StartedTimestamp);
				Item->SetNumberField(TEXT("finished_seconds"), Task.FinishedTimestamp);
				Item->SetNumberField(TEXT("completed_seconds"), Task.CompletedTimestamp);
				Item->SetNumberField(TEXT("queue_delay_ms"), ValidTaskDuration(Task.ScheduledTimestamp, Task.StartedTimestamp) * 1000.0);
				Item->SetNumberField(TEXT("execution_ms"), ValidTaskDuration(Task.StartedTimestamp, Task.FinishedTimestamp) * 1000.0);
				Item->SetNumberField(TEXT("prerequisite_count"), Task.Prerequisites.Num());
				Item->SetNumberField(TEXT("subsequent_count"), Task.Subsequents.Num());
				Item->SetNumberField(TEXT("parent_count"), Task.ParentTasks.Num());
				Item->SetNumberField(TEXT("nested_count"), Task.NestedTasks.Num());
				Tasks.Add(JsonObjectValue(Item));
				return TraceServices::ETaskEnumerationResult::Continue;
			});
	}
	Root->SetArrayField(TEXT("tasks"), Tasks);
	Coverage->SetObjectField(TEXT("tasks"), MakeCoverageEntry(TasksProvider != nullptr, Tasks.Num()));

	const TraceServices::IFileActivityProvider* FileProvider = TraceServices::ReadFileActivityProvider(*Session);
	TArray<TSharedPtr<FJsonValue>> FileActivity;
	if (FileProvider)
	{
		FileProvider->EnumerateFileActivity([&](const TraceServices::FFileInfo& File, const TraceServices::IFileActivityProvider::Timeline& Timeline)
		{
			int64 Count = 0, Failures = 0;
			uint64 Bytes = 0;
			double TotalMs = 0.0, MaxMs = 0.0;
			Timeline.EnumerateEvents(0.0, SessionDuration, [&](double Start, double End, uint32 Depth, TraceServices::FFileActivity* const& Activity)
			{
				(void)Depth;
				if (Activity)
				{
					++Count; Failures += Activity->Failed ? 1 : 0; Bytes += Activity->ActualSize;
					const double DurationMs = FMath::Max(0.0, End - Start) * 1000.0;
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
				FileActivity.Add(JsonObjectValue(Item));
			}
			return true;
		});
	}
	Root->SetArrayField(TEXT("file_activity"), FileActivity);
	Coverage->SetObjectField(TEXT("file_activity"), MakeCoverageEntry(FileProvider != nullptr, FileActivity.Num()));

	const TraceServices::ILoadTimeProfilerProvider* LoadTimeProvider = TraceServices::ReadLoadTimeProfilerProvider(*Session);
	TArray<TSharedPtr<FJsonValue>> PackageLoads;
	TArray<TSharedPtr<FJsonValue>> ExportLoads;
	TArray<TSharedPtr<FJsonValue>> LoadRequests;
	if (LoadTimeProvider)
	{
		TUniquePtr<TraceServices::ITable<TraceServices::FPackagesTableRow>> PackageTable(LoadTimeProvider->CreatePackageDetailsTable(0.0, SessionDuration));
		if (PackageTable)
		{
			TUniquePtr<TraceServices::ITableReader<TraceServices::FPackagesTableRow>> Reader(PackageTable->CreateReader());
			while (Reader && Reader->IsValid() && PackageLoads.Num() < MaxLoadRows)
			{
				const TraceServices::FPackagesTableRow* Row = Reader->GetCurrentRow();
				if (Row && Row->PackageInfo)
				{
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
				Reader->NextRow();
			}
		}

		TUniquePtr<TraceServices::ITable<TraceServices::FExportsTableRow>> ExportTable(LoadTimeProvider->CreateExportDetailsTable(0.0, SessionDuration));
		if (ExportTable)
		{
			TUniquePtr<TraceServices::ITableReader<TraceServices::FExportsTableRow>> Reader(ExportTable->CreateReader());
			while (Reader && Reader->IsValid() && ExportLoads.Num() < MaxLoadRows)
			{
				const TraceServices::FExportsTableRow* Row = Reader->GetCurrentRow();
				if (Row && Row->ExportInfo)
				{
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
				Reader->NextRow();
			}
		}

		TUniquePtr<TraceServices::ITable<TraceServices::FRequestsTableRow>> RequestTable(LoadTimeProvider->CreateRequestsTable(0.0, SessionDuration));
		if (RequestTable)
		{
			TUniquePtr<TraceServices::ITableReader<TraceServices::FRequestsTableRow>> Reader(RequestTable->CreateReader());
			while (Reader && Reader->IsValid() && LoadRequests.Num() < MaxLoadRows)
			{
				const TraceServices::FRequestsTableRow* Row = Reader->GetCurrentRow();
				if (Row)
				{
					TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
					Item->SetStringField(TEXT("id"), LexToString(Row->Id));
					Item->SetStringField(TEXT("name"), Row->Name ? Row->Name : TEXT(""));
					Item->SetNumberField(TEXT("start_seconds"), Row->StartTime);
					Item->SetNumberField(TEXT("duration_ms"), Row->Duration * 1000.0);
					Item->SetNumberField(TEXT("package_count"), Row->Packages.Num());
					LoadRequests.Add(JsonObjectValue(Item));
				}
				Reader->NextRow();
			}
		}
	}
	PackageLoads.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B) { return A->AsObject()->GetNumberField(TEXT("total_ms")) > B->AsObject()->GetNumberField(TEXT("total_ms")); });
	ExportLoads.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B) { return A->AsObject()->GetNumberField(TEXT("total_ms")) > B->AsObject()->GetNumberField(TEXT("total_ms")); });
	LoadRequests.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B) { return A->AsObject()->GetNumberField(TEXT("duration_ms")) > B->AsObject()->GetNumberField(TEXT("duration_ms")); });
	Root->SetArrayField(TEXT("package_loads"), PackageLoads);
	Root->SetArrayField(TEXT("export_loads"), ExportLoads);
	Root->SetArrayField(TEXT("load_requests"), LoadRequests);
	Coverage->SetObjectField(TEXT("load_time"), MakeCoverageEntry(LoadTimeProvider != nullptr, PackageLoads.Num()));

	const TraceServices::ILogProvider* LogProvider = Session->ReadProvider<TraceServices::ILogProvider>(TraceServices::GetLogProviderName());
	TArray<TSharedPtr<FJsonValue>> Logs;
	if (LogProvider)
	{
		LogProvider->EnumerateMessages(0.0, SessionDuration, [&](const TraceServices::FLogMessageInfo& Message)
		{
			if (Logs.Num() >= MaxLogMessages || Message.Verbosity > ELogVerbosity::Warning) { return; }
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetNumberField(TEXT("time_seconds"), Message.Time);
			Item->SetStringField(TEXT("category"), Message.Category && Message.Category->Name ? Message.Category->Name : TEXT(""));
			Item->SetStringField(TEXT("message"), Message.Message ? Message.Message : TEXT(""));
			Item->SetNumberField(TEXT("verbosity"), static_cast<uint8>(Message.Verbosity));
			Logs.Add(JsonObjectValue(Item));
		});
	}
	Root->SetArrayField(TEXT("logs"), Logs);
	Coverage->SetObjectField(TEXT("logs"), MakeCoverageEntry(LogProvider != nullptr, Logs.Num()));

	const TraceServices::IContextSwitchesProvider* ContextProvider = TraceServices::ReadContextSwitchesProvider(*Session);
	TArray<TSharedPtr<FJsonValue>> ContextSwitches;
	bool bHasContextData = false;
	if (ContextProvider)
	{
		TraceServices::FProviderReadScopeLock ContextReadScope(*ContextProvider);
		bHasContextData = ContextProvider->HasData();
		if (bHasContextData && ThreadProvider)
		{
			ThreadProvider->EnumerateThreads([&](const TraceServices::FThreadInfo& Thread)
			{
				int64 Count = 0;
				double RunningMs = 0.0;
				ContextProvider->EnumerateContextSwitches(Thread.Id, 0.0, SessionDuration, [&](const TraceServices::FContextSwitch& Context)
				{
					++Count; RunningMs += FMath::Max(0.0, Context.End - Context.Start) * 1000.0;
					return TraceServices::EContextSwitchEnumerationResult::Continue;
				});
				if (Count > 0)
				{
					TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
					Item->SetNumberField(TEXT("thread_id"), Thread.Id);
					Item->SetStringField(TEXT("thread"), Thread.Name ? Thread.Name : TEXT(""));
					Item->SetNumberField(TEXT("switch_count"), static_cast<double>(Count));
					Item->SetNumberField(TEXT("running_ms"), RunningMs);
					Item->SetNumberField(TEXT("scheduled_ratio"), SessionDuration > 0.0 ? RunningMs / (SessionDuration * 1000.0) : 0.0);
					ContextSwitches.Add(JsonObjectValue(Item));
				}
			});
		}
	}
	Root->SetArrayField(TEXT("context_switches"), ContextSwitches);
	Coverage->SetObjectField(TEXT("context_switches"), MakeCoverageEntry(bHasContextData, ContextSwitches.Num()));

	const TraceServices::IStackSamplesProvider* StackSamplesProvider = TraceServices::ReadStackSamplesProvider(*Session);
	TMap<uint32, TSharedPtr<FJsonObject>> StackFrameByTimer;
	TArray<TSharedPtr<FJsonValue>> StackFrames;
	TArray<TSharedPtr<FJsonValue>> StackSampleEvents;
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
	Root->SetArrayField(TEXT("stack_frames"), StackFrames);
	Root->SetArrayField(TEXT("stack_sample_events"), StackSampleEvents);
	Coverage->SetObjectField(TEXT("stack_samples"), MakeCoverageEntry(StackSamplesProvider != nullptr, StackSampleEvents.Num()));

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
			AllocationsProvider->EnumerateTimeline(TraceServices::IAllocationsProvider::ETimelineU64::MaxTotalAllocatedMemory, 0, LastIndex,
				[&](double Time, double Duration, uint64 Value)
				{
					(void)Time; (void)Duration;
					if (Count == 0) { FirstBytes = Value; } LastBytes = Value; PeakBytes = FMath::Max(PeakBytes, Value); ++Count;
				});
			AllocationSummary->SetNumberField(TEXT("first_bytes"), static_cast<double>(FirstBytes));
			AllocationSummary->SetNumberField(TEXT("last_bytes"), static_cast<double>(LastBytes));
			AllocationSummary->SetNumberField(TEXT("peak_bytes"), static_cast<double>(PeakBytes));
			AllocationSummary->SetNumberField(TEXT("growth_bytes"), static_cast<double>(LastBytes) - static_cast<double>(FirstBytes));
			AllocationSummary->SetBoolField(TEXT("has_allocation_events"), AllocationsProvider->HasAllocationEvents());
		}
	}
	Root->SetObjectField(TEXT("allocation_summary"), AllocationSummary);
	Coverage->SetObjectField(TEXT("allocations"), MakeCoverageEntry(bAllocationsInitialized));

	const TraceServices::IObjectProvider* ObjectProvider = TraceServices::ReadObjectProvider(*Session);
	TArray<TSharedPtr<FJsonValue>> ObjectSnapshots;
	TArray<TSharedPtr<FJsonValue>> ObjectClasses;
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
	Root->SetArrayField(TEXT("object_snapshots"), ObjectSnapshots);
	Root->SetArrayField(TEXT("object_classes"), ObjectClasses);
	Coverage->SetObjectField(TEXT("objects"), MakeCoverageEntry(ObjectProvider != nullptr, ObjectSnapshots.Num()));

	const TraceServices::INetProfilerProvider* NetProvider = TraceServices::ReadNetProfilerProvider(*Session);
	TArray<TSharedPtr<FJsonValue>> NetworkConnections;
	if (NetProvider)
	{
		NetProvider->ReadGameInstances([&](const TraceServices::FNetProfilerGameInstance& Instance)
		{
			NetProvider->ReadConnections(Instance.GameInstanceIndex, [&](const TraceServices::FNetProfilerConnection& Connection)
			{
				for (uint8 ModeValue = 0; ModeValue < TraceServices::ENetProfilerConnectionMode::Count; ++ModeValue)
				{
					const TraceServices::ENetProfilerConnectionMode Mode = static_cast<TraceServices::ENetProfilerConnectionMode>(ModeValue);
					const uint32 PacketCount = NetProvider->GetPacketCount(Connection.ConnectionIndex, Mode);
					uint64 TotalBytes = 0; uint32 MaxBytes = 0; uint32 Dropped = 0;
					if (PacketCount > 0)
					{
						NetProvider->EnumeratePackets(Connection.ConnectionIndex, Mode, 0, PacketCount - 1,
							[&](const TraceServices::FNetProfilerPacket& Packet)
							{
								TotalBytes += Packet.TotalPacketSizeInBytes;
								MaxBytes = FMath::Max(MaxBytes, Packet.TotalPacketSizeInBytes);
								Dropped += Packet.DeliveryStatus == TraceServices::ENetProfilerDeliveryStatus::Dropped ? 1 : 0;
							});
					}
					if (PacketCount > 0)
					{
						TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
						Item->SetStringField(TEXT("instance"), Instance.InstanceName ? Instance.InstanceName : TEXT(""));
						Item->SetBoolField(TEXT("server"), Instance.bIsServer);
						Item->SetStringField(TEXT("connection"), Connection.Name ? Connection.Name : TEXT(""));
						Item->SetStringField(TEXT("address"), Connection.AddressString ? Connection.AddressString : TEXT(""));
						Item->SetStringField(TEXT("mode"), Mode == TraceServices::ENetProfilerConnectionMode::Outgoing ? TEXT("outgoing") : TEXT("incoming"));
						Item->SetNumberField(TEXT("packet_count"), PacketCount);
						Item->SetNumberField(TEXT("total_bytes"), static_cast<double>(TotalBytes));
						Item->SetNumberField(TEXT("max_packet_bytes"), MaxBytes);
						Item->SetNumberField(TEXT("dropped_packets"), Dropped);
						NetworkConnections.Add(JsonObjectValue(Item));
					}
				}
			});
		});
	}
	Root->SetArrayField(TEXT("network_connections"), NetworkConnections);
	Coverage->SetObjectField(TEXT("network"), MakeCoverageEntry(NetProvider != nullptr, NetworkConnections.Num()));

	const TraceServices::IScreenshotProvider* ScreenshotProvider = Session->ReadProvider<TraceServices::IScreenshotProvider>(TraceServices::GetScreenshotProviderName());
	Coverage->SetObjectField(TEXT("screenshots"), MakeCoverageEntry(ScreenshotProvider != nullptr));
	Root->SetObjectField(TEXT("coverage"), Coverage);

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
#endif // UE 5.8+ TraceServices extraction
}
