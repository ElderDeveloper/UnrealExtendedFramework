// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PerfSentinelStatHarvester.h"

#include "HAL/PlatformTime.h"

#if STATS
#include "Stats/Stats.h"
#include "Stats/StatsData.h"
#endif

bool FPerfSentinelStatHarvester::IsAvailable()
{
#if STATS
	return true;
#else
	return false;
#endif
}

void FPerfSentinelStatHarvester::Enable()
{
#if STATS
	// Force the stats system to collect per-frame data even when no stat group is being viewed.
	FThreadStats::PrimaryEnableAdd();
#endif
}

void FPerfSentinelStatHarvester::Disable()
{
#if STATS
	FThreadStats::PrimaryEnableSubtract();
#endif
}

bool FPerfSentinelStatHarvester::HarvestTopScopes(int32 TopN, TArray<FPerfSentinelStatSample>& OutSamples)
{
	OutSamples.Reset();

#if STATS
	if (!FThreadStats::IsCollectingData())
	{
		return false;
	}

	// Make sure the stats thread has drained queued messages so the latest frame is consistent to read.
	FThreadStats::WaitForStats();

	const FStatsThreadState& Stats = FStatsThreadState::GetLocalState();
	const int64 TargetFrame = Stats.GetLatestValidFrame();
	if (TargetFrame < 0 || !Stats.IsFrameValid(TargetFrame))
	{
		return false;
	}

	// Inclusive (total) time per scope, ignoring non-stack (counter/memory) stats.
	TArray<FStatMessage> InclusiveStats;
	Stats.GetInclusiveAggregateStackStats(TargetFrame, InclusiveStats, nullptr, false);

	// Exclusive (self) time per scope, keyed by name for a best-effort merge.
	TArray<FStatMessage> ExclusiveStats;
	Stats.GetExclusiveAggregateStackStats(TargetFrame, ExclusiveStats, nullptr, false);

	TMap<FName, double> ExclusiveMsByName;
	ExclusiveMsByName.Reserve(ExclusiveStats.Num());
	for (const FStatMessage& Message : ExclusiveStats)
	{
		const uint32 DurationCycles = FromPackedCallCountDuration_Duration(Message.GetValue_int64());
		ExclusiveMsByName.Add(Message.NameAndInfo.GetShortName(), static_cast<double>(FPlatformTime::ToMilliseconds(DurationCycles)));
	}

	OutSamples.Reserve(InclusiveStats.Num());
	for (const FStatMessage& Message : InclusiveStats)
	{
		const int64 Packed = Message.GetValue_int64();
		const uint32 DurationCycles = FromPackedCallCountDuration_Duration(Packed);
		const uint32 CallCount = FromPackedCallCountDuration_CallCount(Packed);

		const double InclusiveMs = static_cast<double>(FPlatformTime::ToMilliseconds(DurationCycles));
		if (InclusiveMs <= 0.0)
		{
			continue;
		}

		const FName ShortName = Message.NameAndInfo.GetShortName();
		FPerfSentinelStatSample Sample;
		Sample.ScopeName = ShortName.ToString();
		Sample.InclusiveMs = InclusiveMs;
		Sample.ExclusiveMs = ExclusiveMsByName.FindRef(ShortName);
		Sample.CallCount = static_cast<int64>(CallCount);
		OutSamples.Add(MoveTemp(Sample));
	}

	OutSamples.Sort([](const FPerfSentinelStatSample& A, const FPerfSentinelStatSample& B)
	{
		return A.InclusiveMs > B.InclusiveMs;
	});

	if (TopN > 0 && OutSamples.Num() > TopN)
	{
		OutSamples.SetNum(TopN, EAllowShrinking::No);
	}

	return true;
#else
	(void)TopN;
	return false;
#endif
}
