// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UPerfSentinelSettings;

/** Integration callbacks run on the game thread and must bound their own work. Return false when unavailable. */
DECLARE_DELEGATE_RetVal_OneParam(bool, FPerfSentinelCollectorDelegate, TSharedRef<FJsonObject> /*DeclaredMetrics*/);

struct FPerfSentinelCollectorDefinition
{
	FName Id;
	FString Domain;
	FString Source;
	FString Description;
	bool bDetailed = false;
	double MinimumIntervalSeconds = 1.0;
	FPerfSentinelCollectorDelegate Collect;
};

/** Native integration catalog. A scheduling budget cannot interrupt arbitrary user callbacks. */
class UNREALEXTENDEDPERFSENTINEL_API FPerfSentinelCollectorRegistry
{
public:
	static FPerfSentinelCollectorRegistry& Get();
	void InitializeBuiltins();
	bool RegisterCollector(const FPerfSentinelCollectorDefinition& Definition);
	void UnregisterCollector(FName Id);
	void ResetCapture();
	TSharedRef<FJsonObject> GetCatalog() const;
	TSharedRef<FJsonObject> Sample(double PlatformSeconds, const UPerfSentinelSettings& Settings);

private:
	struct FCollectorState
	{
		FPerfSentinelCollectorDefinition Definition;
		double LastSampleSeconds = -TNumericLimits<double>::Max();
		int32 ConsecutiveOverruns = 0;
	};
	TMap<FName, FCollectorState> Collectors;
	bool bSampling = false;
};
