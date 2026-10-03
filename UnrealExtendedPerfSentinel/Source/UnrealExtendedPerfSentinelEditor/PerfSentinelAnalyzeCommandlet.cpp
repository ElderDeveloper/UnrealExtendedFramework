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

#if !UE_VERSION_OLDER_THAN(5, 8, 0)
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

void ExtractNetworkEvidence(const TraceServices::IAnalysisSession& Session, double SessionDuration,
	const TArray<FHitchFrame>& HitchFrames, const TSharedRef<FJsonObject>& Root, const TSharedRef<FJsonObject>& Coverage)
{
	const TraceServices::INetProfilerProvider* Provider = TraceServices::ReadNetProfilerProvider(Session);
	const uint32 Version = Provider ? Provider->GetNetTraceVersion() : 0;
	// NetTraceAnalyzer does not maintain IAnalysisSession's duration in UE 5.8.
	// A custom net-only capture therefore needs its own clock bounds. Derive
	// them before computing rates, including finite connection/instance closes.
	// Keep the global session duration separate so this never invents CPU/GPU time.
	const bool bHasSessionSpan = FMath::IsFinite(SessionDuration) && SessionDuration > 0.0;
	double NetworkSpanStart = bHasSessionSpan ? 0.0 : DBL_MAX;
	double NetworkSpanEnd = bHasSessionSpan ? SessionDuration : 0.0;
	uint64 TimedPacketCount = 0, PacketsBeyondSessionDuration = 0;
	auto IncludeTimestamp = [&](double Time)
	{
		if (!FMath::IsFinite(Time) || Time < 0.0) { return; }
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

void ExtractTimingScopeTotals(const TraceServices::ITimingProfilerProvider* Provider, double SessionDuration,
	const TSharedRef<FJsonObject>& Root, const TSharedRef<FJsonObject>& Coverage)
{
	TArray<TSharedPtr<FJsonValue>> Totals;
	TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("start_seconds"), 0.0); Summary->SetNumberField(TEXT("end_seconds"), SessionDuration);
	Summary->SetNumberField(TEXT("duration_seconds"), SessionDuration);
	Summary->SetNumberField(TEXT("rows_per_kind_limit"), MaxTimingScopeRowsPerKind);
	Summary->SetStringField(TEXT("semantics"), TEXT("Whole-session instrumented timer aggregation across all CPU threads or GPU queues. Inclusive scopes overlap their children; exclusive scopes subtract children. Concurrent threads/queues may sum beyond wall time. Not total CPU utilization, a causal critical path, or a complete cost of uninstrumented SDK work."));
	uint32 GpuQueueCount = 0;
	if (Provider)
	{
		Provider->EnumerateGpuQueues([&](const TraceServices::FGpuQueueInfo& Queue) { (void)Queue; ++GpuQueueCount; });
		for (int32 KindIndex = 0; KindIndex < 2; ++KindIndex)
		{
			const bool bCpu = KindIndex == 0;
			TraceServices::FCreateAggregationParams Params;
			Params.IntervalStart = 0.0; Params.IntervalEnd = SessionDuration;
			Params.SortBy = TraceServices::FCreateAggregationParams::ESortBy::TotalInclusiveTime;
			Params.SortOrder = TraceServices::FCreateAggregationParams::ESortOrder::Descending;
			// One look-ahead row makes truncation explicit without serializing every timer.
			Params.TableEntryLimit = MaxTimingScopeRowsPerKind + 1;
			if (bCpu) { Params.CpuThreadFilter = [](uint32 ThreadId) { (void)ThreadId; return true; }; }
			else if (GpuQueueCount > 0) { Params.GpuQueueFilter = [](uint32 QueueId) { (void)QueueId; return true; }; }
			else
			{
				uint32 TimelineIndex = ~0u;
				Params.bIncludeOldGpu1 = Provider->GetGpuTimelineIndex(TimelineIndex);
				Params.bIncludeOldGpu2 = Provider->GetGpu2TimelineIndex(TimelineIndex);
			}
			TUniquePtr<TraceServices::ITable<TraceServices::FTimingProfilerAggregatedStats>> Table(Provider->CreateAggregation(Params));
			const uint64 ReturnedRows = Table ? Table->GetRowCount() : 0;
			Summary->SetNumberField(bCpu ? TEXT("cpu_rows_returned") : TEXT("gpu_rows_returned"), static_cast<double>(ReturnedRows));
			Summary->SetBoolField(bCpu ? TEXT("cpu_truncated") : TEXT("gpu_truncated"), ReturnedRows > MaxTimingScopeRowsPerKind);
			if (!Table) { continue; }
			TUniquePtr<TraceServices::ITableReader<TraceServices::FTimingProfilerAggregatedStats>> Reader(Table->CreateReader());
			for (int32 RowIndex = 0; Reader && Reader->IsValid() && RowIndex < MaxTimingScopeRowsPerKind; ++RowIndex, Reader->NextRow())
			{
				const TraceServices::FTimingProfilerAggregatedStats* Row = Reader->GetCurrentRow();
				if (!Row || !Row->Timer || Row->InstanceCount == 0 || !FMath::IsFinite(Row->TotalInclusiveTime) || !FMath::IsFinite(Row->TotalExclusiveTime)) { continue; }
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("kind"), bCpu ? TEXT("cpu") : TEXT("gpu"));
				Item->SetNumberField(TEXT("timer_id"), Row->Timer->Id);
				Item->SetStringField(TEXT("timer"), Row->Timer->Name ? Row->Timer->Name : TEXT("<unnamed>"));
				Item->SetStringField(TEXT("source_file"), Row->Timer->File ? Row->Timer->File : TEXT(""));
				Item->SetNumberField(TEXT("line"), Row->Timer->Line);
				Item->SetNumberField(TEXT("instance_count"), static_cast<double>(Row->InstanceCount));
				Item->SetNumberField(TEXT("inclusive_ms"), Row->TotalInclusiveTime * 1000.0);
				Item->SetNumberField(TEXT("exclusive_ms"), Row->TotalExclusiveTime * 1000.0);
				Item->SetNumberField(TEXT("max_inclusive_ms"), Row->MaxInclusiveTime * 1000.0);
				Item->SetNumberField(TEXT("max_exclusive_ms"), Row->MaxExclusiveTime * 1000.0);
				Item->SetNumberField(TEXT("mean_inclusive_ms"), Row->AverageInclusiveTime * 1000.0);
				Item->SetNumberField(TEXT("mean_exclusive_ms"), Row->AverageExclusiveTime * 1000.0);
				Totals.Add(JsonObjectValue(Item));
			}
		}
	}
	Summary->SetNumberField(TEXT("gpu_queue_count"), GpuQueueCount);
	Root->SetArrayField(TEXT("timing_scope_totals"), Totals); Root->SetObjectField(TEXT("timing_scope_summary"), Summary);
	Coverage->SetObjectField(TEXT("timing_aggregate"), MakeCoverageEntry(Totals.Num() > 0, Totals.Num()));
}
#endif

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
	const int32 HitchWindowsSeen = HitchFrames.Num();
	if (HitchFrames.Num() > MaxHitchWindows)
	{
		HitchFrames.SetNum(MaxHitchWindows, EAllowShrinking::No);
	}
	Root->SetArrayField(TEXT("game_frames"), GameFrames);
	Root->SetArrayField(TEXT("render_frames"), RenderFrames);
	Coverage->SetObjectField(TEXT("frames"), MakeCoverageEntry(FrameProvider != nullptr, GameFrames.Num()));

	const TraceServices::IThreadProvider* ThreadProvider = Session->ReadProvider<TraceServices::IThreadProvider>(TraceServices::GetThreadProviderName());
	const TraceServices::ITimingProfilerProvider* TimingProvider = TraceServices::ReadTimingProfilerProvider(*Session);
	ExtractTimingScopeTotals(TimingProvider, SessionDuration, Root, Coverage);

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
							const double ClippedStart = FMath::Max(Start, Hitch.Start);
							const double ScopeEnd = FMath::IsFinite(End) ? End : SessionDuration;
							const double ClippedEnd = FMath::Min(ScopeEnd, Hitch.End);
							if (!FMath::IsFinite(Start) || ClippedEnd <= ClippedStart) { return TraceServices::EEventEnumerate::Continue; }
							if (TimingEventCount >= MaxTimingEvents || (ClippedEnd - ClippedStart) < 0.00005)
							{
								return TimingEventCount >= MaxTimingEvents ? TraceServices::EEventEnumerate::Stop : TraceServices::EEventEnumerate::Continue;
							}
							TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
							Item->SetNumberField(TEXT("frame_index"), static_cast<double>(Hitch.Index));
							Item->SetNumberField(TEXT("thread_id"), Thread.Id);
							Item->SetStringField(TEXT("thread"), Thread.Name ? Thread.Name : TEXT(""));
							Item->SetNumberField(TEXT("timer_id"), Event.TimerIndex);
							const TraceServices::FTimingProfilerTimer* Timer = TimingProvider->GetTimerReader().GetTimer(Event.TimerIndex);
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
							++TimingEventCount;
							return TraceServices::EEventEnumerate::Continue;
						});
				}
			});
		});
	}

	if (TimingProvider && TimingEventCount < MaxTimingEvents)
	{
		TMap<uint32, FString> GpuTimelines;
		TimingProvider->EnumerateGpuQueues([&](const TraceServices::FGpuQueueInfo& Queue)
		{
			uint32 TimelineIndex = ~0u;
			if (TimingProvider->GetGpuQueueTimelineIndex(Queue.Id, TimelineIndex)) { GpuTimelines.Add(TimelineIndex, Queue.GetDisplayName()); }
		});
		if (GpuTimelines.IsEmpty())
		{
			uint32 TimelineIndex = ~0u;
			if (TimingProvider->GetGpuTimelineIndex(TimelineIndex)) { GpuTimelines.Add(TimelineIndex, TEXT("GPU")); }
			if (TimingProvider->GetGpu2TimelineIndex(TimelineIndex)) { GpuTimelines.Add(TimelineIndex, TEXT("GPU2")); }
		}
		for (const TPair<uint32, FString>& GpuTimeline : GpuTimelines)
		{
			if (TimingEventCount >= MaxTimingEvents) { break; }
			TimingProvider->ReadTimeline(GpuTimeline.Key, [&](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
			{
				for (const FHitchFrame& Hitch : HitchFrames)
				{
					Timeline.EnumerateEvents(Hitch.Start, Hitch.End,
						[&](double Start, double End, uint32 Depth, const TraceServices::FTimingProfilerEvent& Event)
						{
							const double ClippedStart = FMath::Max(Start, Hitch.Start);
							const double ScopeEnd = FMath::IsFinite(End) ? End : SessionDuration;
							const double ClippedEnd = FMath::Min(ScopeEnd, Hitch.End);
							if (!FMath::IsFinite(Start) || ClippedEnd <= ClippedStart) { return TraceServices::EEventEnumerate::Continue; }
							if (TimingEventCount >= MaxTimingEvents) { return TraceServices::EEventEnumerate::Stop; }
							TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
							Item->SetNumberField(TEXT("frame_index"), static_cast<double>(Hitch.Index));
							Item->SetNumberField(TEXT("thread_id"), -1);
							Item->SetNumberField(TEXT("timeline_index"), GpuTimeline.Key);
							Item->SetStringField(TEXT("thread"), GpuTimeline.Value);
							Item->SetNumberField(TEXT("timer_id"), Event.TimerIndex);
							const TraceServices::FTimingProfilerTimer* Timer = TimingProvider->GetTimerReader().GetTimer(Event.TimerIndex);
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
							++TimingEventCount;
							return TraceServices::EEventEnumerate::Continue;
						});
				}
			});
		}
	}
	Root->SetArrayField(TEXT("threads"), Threads);
	Root->SetArrayField(TEXT("timing_events"), TimingEvents);
	Root->SetStringField(TEXT("timing_events_scope"), TEXT("retained_hitch_windows_only"));
	Root->SetBoolField(TEXT("timing_events_truncated"), TimingEventCount >= MaxTimingEvents);
	Root->SetNumberField(TEXT("timing_events_limit"), MaxTimingEvents);
	Root->SetNumberField(TEXT("hitch_windows_exported"), HitchFrames.Num());
	Root->SetNumberField(TEXT("hitch_windows_seen"), HitchWindowsSeen);
	Root->SetBoolField(TEXT("hitch_windows_truncated"), HitchWindowsSeen > HitchFrames.Num());
	Root->SetNumberField(TEXT("hitch_windows_limit"), MaxHitchWindows);
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

	ExtractNetworkEvidence(*Session, SessionDuration, HitchFrames, Root, Coverage);

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
