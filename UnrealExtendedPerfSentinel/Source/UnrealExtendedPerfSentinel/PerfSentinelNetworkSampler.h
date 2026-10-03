// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UNetConnection;
class UPerfSentinelSettings;

/** Bounded game-thread snapshots of native gameplay transport counters. Never records addresses or account IDs. */
class FPerfSentinelNetworkSampler
{
public:
	void Reset();
	TSharedRef<FJsonObject> Sample(double PlatformSeconds, const UPerfSentinelSettings& Settings);

private:
	struct FConnectionSample
	{
		int32 Id = 0;
		double Seconds = 0.0;
		uint32 InBytes = 0, OutBytes = 0, InPackets = 0, OutPackets = 0, InLost = 0, OutLost = 0;
		uint32 OutNotified = 0;
	};
	int32 GetObjectId(const UObject* Object);
	TSharedRef<FJsonObject> SampleConnection(UNetConnection* Connection, bool bServerConnection, double Seconds, int32 MaxChannels);
	TMap<TWeakObjectPtr<UObject>, int32> ObjectIds;
	TMap<TWeakObjectPtr<UNetConnection>, FConnectionSample> PreviousConnections;
	int32 NextObjectId = 0;
};
