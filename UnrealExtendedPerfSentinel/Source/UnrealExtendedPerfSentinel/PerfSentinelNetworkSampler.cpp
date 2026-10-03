// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#include "PerfSentinelNetworkSampler.h"

#include "PerfSentinelSettings.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/ActorChannel.h"
#include "Engine/Channel.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"

namespace
{
const TCHAR* NetModeName(ENetMode Mode)
{
	switch (Mode)
	{
	case NM_Client: return TEXT("client");
	case NM_ListenServer: return TEXT("listen_server");
	case NM_DedicatedServer: return TEXT("dedicated_server");
	default: return TEXT("standalone");
	}
}
}

void FPerfSentinelNetworkSampler::Reset()
{
	ObjectIds.Reset();
	PreviousConnections.Reset();
	NextObjectId = 0;
}

int32 FPerfSentinelNetworkSampler::GetObjectId(const UObject* Object)
{
	const TWeakObjectPtr<UObject> Key(const_cast<UObject*>(Object));
	if (const int32* Existing = ObjectIds.Find(Key)) { return *Existing; }
	// Active observations are bounded below; historical IDs do not retain UObjects.
	return ObjectIds.Add(Key, ++NextObjectId);
}

TSharedRef<FJsonObject> FPerfSentinelNetworkSampler::SampleConnection(UNetConnection* Connection, bool bServerConnection, double Seconds, int32 MaxChannels)
{
	const TWeakObjectPtr<UNetConnection> Key(Connection);
	FConnectionSample Current;
	Current.Id = GetObjectId(Connection);
	Current.Seconds = Seconds;
	// These signed engine lifetime counters can wrap. A decrease invalidates one interval rather than inventing traffic.
	Current.InBytes = static_cast<uint32>(Connection->InTotalBytes);
	Current.OutBytes = static_cast<uint32>(Connection->OutTotalBytes);
	Current.InPackets = static_cast<uint32>(Connection->InTotalPackets);
	Current.OutPackets = static_cast<uint32>(Connection->OutTotalPackets);
	Current.InLost = static_cast<uint32>(Connection->InTotalPacketsLost);
	Current.OutLost = static_cast<uint32>(Connection->OutTotalPacketsLost);
	Current.OutNotified = Connection->GetOutTotalNotifiedPackets();

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("connection_id"), Current.Id);
	if (Connection->GetConnectionHandle().IsValid())
	{
		Root->SetNumberField(TEXT("native_connection_id"), Connection->GetConnectionHandle().GetParentConnectionId());
	}
	Root->SetStringField(TEXT("state"), LexToString(Connection->GetConnectionState()));
	Root->SetBoolField(TEXT("is_server_connection"), bServerConnection);
	Root->SetNumberField(TEXT("in_total_bytes"), Current.InBytes);
	Root->SetNumberField(TEXT("out_total_bytes"), Current.OutBytes);
	Root->SetNumberField(TEXT("in_total_packets"), Current.InPackets);
	Root->SetNumberField(TEXT("out_total_packets"), Current.OutPackets);
	Root->SetNumberField(TEXT("in_total_packets_lost"), Current.InLost);
	Root->SetNumberField(TEXT("out_total_packets_lost"), Current.OutLost);
	Root->SetNumberField(TEXT("out_total_notified_packets"), Current.OutNotified);
	const FConnectionSample* Previous = PreviousConnections.Find(Key);
	const bool bValidInterval = Previous && Seconds > Previous->Seconds
		&& Current.InBytes >= Previous->InBytes && Current.OutBytes >= Previous->OutBytes
		&& Current.InPackets >= Previous->InPackets && Current.OutPackets >= Previous->OutPackets
		&& Current.InLost >= Previous->InLost && Current.OutLost >= Previous->OutLost
		&& Current.OutNotified >= Previous->OutNotified;
	Root->SetStringField(TEXT("counter_status"), Previous ? (bValidInterval ? TEXT("valid") : TEXT("reset")) : TEXT("baseline"));
	if (bValidInterval)
	{
		const double Interval = Seconds - Previous->Seconds;
		const uint32 InPackets = Current.InPackets - Previous->InPackets;
		const uint32 OutPackets = Current.OutPackets - Previous->OutPackets;
		const uint32 InLost = Current.InLost - Previous->InLost;
		const uint32 OutLost = Current.OutLost - Previous->OutLost;
		const uint32 OutNotified = Current.OutNotified - Previous->OutNotified;
		Root->SetNumberField(TEXT("sample_interval_seconds"), Interval);
		Root->SetNumberField(TEXT("in_bytes_delta"), Current.InBytes - Previous->InBytes);
		Root->SetNumberField(TEXT("out_bytes_delta"), Current.OutBytes - Previous->OutBytes);
		Root->SetNumberField(TEXT("in_packets_delta"), InPackets);
		Root->SetNumberField(TEXT("out_packets_delta"), OutPackets);
		Root->SetNumberField(TEXT("in_packets_lost_delta"), InLost);
		Root->SetNumberField(TEXT("out_packets_lost_delta"), OutLost);
		Root->SetNumberField(TEXT("out_packets_notified_delta"), OutNotified);
		Root->SetNumberField(TEXT("in_bytes_per_second"), (Current.InBytes - Previous->InBytes) / Interval);
		Root->SetNumberField(TEXT("out_bytes_per_second"), (Current.OutBytes - Previous->OutBytes) / Interval);
		Root->SetNumberField(TEXT("in_packets_per_second"), InPackets / Interval);
		Root->SetNumberField(TEXT("out_packets_per_second"), OutPackets / Interval);
		if (static_cast<uint64>(InPackets) + InLost > 0) { Root->SetNumberField(TEXT("in_loss_percent"), 100.0 * InLost / (static_cast<double>(InPackets) + InLost)); }
		// Use the engine's ACK/NAK-notified packet count, excluding packets still in flight at high RTT.
		// No notified packets means loss is unknown for this interval, rather than zero.
		if (OutNotified > 0 && OutLost <= OutNotified) { Root->SetNumberField(TEXT("out_loss_percent"), 100.0 * OutLost / OutNotified); }
	}
	PreviousConnections.Add(Key, Current);
	if (FMath::IsFinite(Connection->AvgLag) && Connection->AvgLag > 0.0f) { Root->SetNumberField(TEXT("avg_rtt_ms"), Connection->AvgLag * 1000.0); }
	if (FMath::IsFinite(Connection->RawPingInSeconds) && Connection->RawPingInSeconds > 0.0) { Root->SetNumberField(TEXT("raw_ping_ms"), Connection->RawPingInSeconds * 1000.0); }
	const float Jitter = Connection->GetAverageJitterInMS();
	if (FMath::IsFinite(Jitter) && Jitter >= 0.0f) { Root->SetNumberField(TEXT("jitter_ms"), Jitter); }
	Root->SetStringField(TEXT("ping_semantics"), TEXT("native_ack_rtt_may_exclude_remote_frame_time"));
	Root->SetStringField(TEXT("loss_semantics"), TEXT("incoming_sequence_gaps_outgoing_naks_over_ack_nak_notified_packets"));
	Root->SetNumberField(TEXT("queued_bits"), Connection->QueuedBits);
	Root->SetStringField(TEXT("queued_bits_semantics"), TEXT("signed_bandwidth_budget_debt_not_payload_queue"));
	Root->SetBoolField(TEXT("is_net_ready"), Connection->IsNetReady());
	Root->SetNumberField(TEXT("net_speed_bytes_per_second"), Connection->CurrentNetSpeed);
	Root->SetNumberField(TEXT("open_channels"), Connection->OpenChannels.Num());
	Root->SetNumberField(TEXT("child_connections"), Connection->Children.Num());
	Root->SetNumberField(TEXT("total_delayed_rpcs"), Connection->TotalDelayedRPCs);
	Root->SetNumberField(TEXT("total_delayed_rpc_frames"), Connection->TotalDelayedRPCsFrameCount);
	int32 OutReliable = 0, Incoming = 0, MaxReliable = 0, ActorChannels = 0, Sampled = 0;
	for (UChannel* Channel : Connection->OpenChannels)
	{
		if (Sampled >= MaxChannels) { break; }
		++Sampled;
		if (!IsValid(Channel)) { continue; }
		OutReliable += FMath::Max(0, Channel->NumOutRec);
		Incoming += FMath::Max(0, Channel->NumInRec);
		MaxReliable = FMath::Max(MaxReliable, Channel->NumOutRec);
		ActorChannels += Cast<UActorChannel>(Channel) ? 1 : 0;
	}
	Root->SetNumberField(TEXT("reliable_outstanding_bunches"), OutReliable);
	Root->SetNumberField(TEXT("max_channel_reliable_backlog"), MaxReliable);
	Root->SetNumberField(TEXT("pending_incoming_bunches"), Incoming);
	Root->SetNumberField(TEXT("actor_channels"), ActorChannels);
	Root->SetNumberField(TEXT("channels_sampled"), Sampled);
	Root->SetBoolField(TEXT("channels_truncated"), Connection->OpenChannels.Num() > Sampled);
	return Root;
}

TSharedRef<FJsonObject> FPerfSentinelNetworkSampler::Sample(double Seconds, const UPerfSentinelSettings& Settings)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schema_version"), 1);
	Root->SetNumberField(TEXT("process_id"), FPlatformProcess::GetCurrentProcessId());
	Root->SetStringField(TEXT("source"), TEXT("native_gameplay_transport_counters"));
	Root->SetStringField(TEXT("scope"), TEXT("gameplay_netdrivers_excludes_eos_rtc_http_platform_services"));
	TArray<TSharedPtr<FJsonValue>> Worlds, Disconnected;
	TSet<TWeakObjectPtr<UNetConnection>> SeenConnections;
	TSet<UNetDriver*> SeenDrivers;
	const int32 MaxWorlds = FMath::Clamp(Settings.MaxNetworkWorlds, 1, 64);
	const int32 MaxDrivers = FMath::Clamp(Settings.MaxNetworkDrivers, 1, 64);
	const int32 MaxConnections = FMath::Clamp(Settings.MaxNetworkConnections, 1, 2048);
	const int32 MaxChannels = FMath::Clamp(Settings.MaxNetworkChannelsPerConnection, 1, 16384);
	int32 WorldCount = 0, DriverCount = 0, ConnectionCount = 0, EligibleWorldCount = 0;
	bool bTruncated = false;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!IsValid(World) || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game)) { continue; }
			++EligibleWorldCount;
			if (WorldCount >= MaxWorlds) { bTruncated = true; continue; }
			++WorldCount;
			TSharedRef<FJsonObject> WorldObject = MakeShared<FJsonObject>();
			WorldObject->SetNumberField(TEXT("world_id"), GetObjectId(World));
			WorldObject->SetStringField(TEXT("map_name"), World->GetMapName());
			WorldObject->SetStringField(TEXT("world_type"), World->WorldType == EWorldType::PIE ? TEXT("PIE") : TEXT("Game"));
			WorldObject->SetStringField(TEXT("net_mode"), NetModeName(World->GetNetMode()));
			TArray<UNetDriver*> Drivers;
			for (const FNamedNetDriver& NamedDriver : Context.ActiveNetDrivers)
			{
				if (IsValid(NamedDriver.NetDriver)) { Drivers.AddUnique(NamedDriver.NetDriver); }
			}
			if (IsValid(World->GetNetDriver())) { Drivers.AddUnique(World->GetNetDriver()); }
			TArray<TSharedPtr<FJsonValue>> DriverValues;
			for (UNetDriver* Driver : Drivers)
			{
				if (SeenDrivers.Contains(Driver)) { continue; }
				if (DriverCount >= MaxDrivers) { bTruncated = true; continue; }
				SeenDrivers.Add(Driver);
				++DriverCount;
				TSharedRef<FJsonObject> DriverObject = MakeShared<FJsonObject>();
				DriverObject->SetNumberField(TEXT("driver_id"), GetObjectId(Driver));
				DriverObject->SetNumberField(TEXT("native_game_instance_id"), Driver->GetNetTraceId());
				DriverObject->SetStringField(TEXT("driver_name"), Driver->NetDriverName.ToString());
				DriverObject->SetStringField(TEXT("driver_class"), Driver->GetClass()->GetPathName());
				DriverObject->SetStringField(TEXT("net_mode"), NetModeName(Driver->GetNetMode()));
				DriverObject->SetBoolField(TEXT("uses_iris"), Driver->GetReplicationSystem() != nullptr);
				DriverObject->SetNumberField(TEXT("net_server_max_tick_rate"), Driver->GetNetServerMaxTickRate());
				DriverObject->SetNumberField(TEXT("in_total_bytes"), Driver->InTotalBytes);
				DriverObject->SetNumberField(TEXT("out_total_bytes"), Driver->OutTotalBytes);
				DriverObject->SetNumberField(TEXT("in_total_packets"), Driver->InTotalPackets);
				DriverObject->SetNumberField(TEXT("out_total_packets"), Driver->OutTotalPackets);
				DriverObject->SetNumberField(TEXT("total_rpcs_called"), Driver->TotalRPCsCalled);
				DriverObject->SetNumberField(TEXT("total_reliable_bunches_sent"), Driver->OutTotalReliableBunches);
#if DO_ENABLE_NET_TEST
				const FPacketSimulationSettings& Simulation = Driver->PacketSimulationSettings;
				TSharedRef<FJsonObject> SimulationObject = MakeShared<FJsonObject>();
				SimulationObject->SetNumberField(TEXT("outgoing_loss_percent"), Simulation.PktLoss);
				SimulationObject->SetNumberField(TEXT("incoming_loss_percent"), Simulation.PktIncomingLoss);
				SimulationObject->SetNumberField(TEXT("lag_ms"), Simulation.PktLag);
				SimulationObject->SetNumberField(TEXT("lag_variance_ms"), Simulation.PktLagVariance);
				SimulationObject->SetNumberField(TEXT("lag_min_ms"), Simulation.PktLagMin);
				SimulationObject->SetNumberField(TEXT("lag_max_ms"), Simulation.PktLagMax);
				SimulationObject->SetNumberField(TEXT("incoming_lag_min_ms"), Simulation.PktIncomingLagMin);
				SimulationObject->SetNumberField(TEXT("incoming_lag_max_ms"), Simulation.PktIncomingLagMax);
				DriverObject->SetObjectField(TEXT("packet_simulation"), SimulationObject);
#endif
				TArray<TSharedPtr<FJsonValue>> ConnectionValues;
				auto AddConnection = [&](UNetConnection* Connection, bool bServer)
				{
					if (!IsValid(Connection) || SeenConnections.Contains(Connection)) { return; }
					if (ConnectionCount >= MaxConnections) { bTruncated = true; return; }
					SeenConnections.Add(Connection);
					++ConnectionCount;
					ConnectionValues.Add(MakeShared<FJsonValueObject>(SampleConnection(Connection, bServer, Seconds, MaxChannels)));
				};
				AddConnection(Driver->ServerConnection, true);
				for (UNetConnection* Connection : Driver->ClientConnections)
				{
					if (ConnectionCount >= MaxConnections) { bTruncated = true; break; }
					AddConnection(Connection, false);
				}
				DriverObject->SetArrayField(TEXT("connections"), ConnectionValues);
				DriverValues.Add(MakeShared<FJsonValueObject>(DriverObject));
			}
			WorldObject->SetArrayField(TEXT("drivers"), DriverValues);
			Worlds.Add(MakeShared<FJsonValueObject>(WorldObject));
		}
	}
	for (auto It = PreviousConnections.CreateIterator(); It; ++It)
	{
		if (SeenConnections.Contains(It.Key())) { continue; }
		TSharedRef<FJsonObject> Connection = MakeShared<FJsonObject>();
		Connection->SetNumberField(TEXT("connection_id"), It.Value().Id);
		const UNetConnection* NativeConnection = It.Key().Get();
		Connection->SetStringField(TEXT("state"), !NativeConnection || NativeConnection->GetConnectionState() == USOCK_Closed ? TEXT("disconnected") : TEXT("not_observed"));
		Disconnected.Add(MakeShared<FJsonValueObject>(Connection));
		It.RemoveCurrent();
	}
	for (auto It = ObjectIds.CreateIterator(); It; ++It) { if (!It.Key().IsValid()) { It.RemoveCurrent(); } }
	Root->SetStringField(TEXT("coverage_status"), EligibleWorldCount == 0 ? TEXT("no_game_world") : (DriverCount == 0 ? TEXT("no_net_driver") : (ConnectionCount == 0 ? TEXT("no_connections") : TEXT("sampled"))));
	Root->SetBoolField(TEXT("truncated"), bTruncated);
	Root->SetNumberField(TEXT("worlds_sampled"), WorldCount);
	Root->SetNumberField(TEXT("drivers_sampled"), DriverCount);
	Root->SetNumberField(TEXT("connections_sampled"), ConnectionCount);
	Root->SetArrayField(TEXT("worlds"), Worlds);
	Root->SetArrayField(TEXT("disconnected_connections"), Disconnected);
	return Root;
}
