// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** A single per-scope CPU cost sample harvested from the engine stats system for one frame. */
struct FPerfSentinelStatSample
{
	/** Short stat name, e.g. "ExecuteUbergraph_BP_Customer" or "STAT_ ... ". */
	FString ScopeName;
	double InclusiveMs = 0.0;
	double ExclusiveMs = 0.0;
	int64 CallCount = 0;
};

/**
 * Reads the most expensive named CPU scopes for the most recent stats frame from the engine
 * stats system. This only produces data in builds where STATS is compiled in and stats collection
 * has been enabled (see Enable/Disable). When unavailable, IsAvailable() returns false and the
 * harvest functions return false so callers can record a clear "stats_unavailable" status instead
 * of silently reporting zero cost.
 */
class FPerfSentinelStatHarvester
{
public:
	/** True when this build can harvest stats (STATS compiled in). */
	static bool IsAvailable();

	/** Begin forcing stats collection so per-frame scope data is populated. Safe to call when unavailable. */
	static void Enable();

	/** Stop forcing stats collection. Must be paired with Enable(). */
	static void Disable();

	/**
	 * Harvest up to TopN scopes (sorted by inclusive time descending) for the latest complete frame.
	 * Returns true if a valid frame was read (even if it had no scopes), false if stats are unavailable.
	 */
	static bool HarvestTopScopes(int32 TopN, TArray<FPerfSentinelStatSample>& OutSamples);
};
