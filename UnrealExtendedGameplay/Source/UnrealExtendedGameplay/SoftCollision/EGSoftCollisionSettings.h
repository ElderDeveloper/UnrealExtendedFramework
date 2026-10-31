// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Settings/EFDeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"

#include "EGSoftCollisionSettings.generated.h"

class UEGSoftCollisionProfile;

/**
 * Project Settings > Extended Framework > Extended Soft Collision.
 *
 * Project-wide defaults. The eg.SoftCollision.* console variables still override the performance
 * switches at runtime, for profiling.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Extended Soft Collision"))
class UNREALEXTENDEDGAMEPLAY_API UEGSoftCollisionSettings : public UEFDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UEGSoftCollisionSettings* Get() { return GetDefault<UEGSoftCollisionSettings>(); }

	/** Profile for components that have none assigned. Empty uses the profile class defaults. Preloaded when a world starts. */
	UPROPERTY(config, EditAnywhere, Category = "Soft Collision")
	TSoftObjectPtr<UEGSoftCollisionProfile> DefaultProfile;

	/** Run the scan on worker threads when there are enough scanning pawns. */
	UPROPERTY(config, EditAnywhere, Category = "Performance")
	bool bParallelScan = true;

	/** Scanning pawns below which the scan runs inline on the game thread. */
	UPROPERTY(config, EditAnywhere, Category = "Performance", meta = (ClampMin = "1", EditCondition = "bParallelScan"))
	int32 ParallelThreshold = 64;

	/** Log setup problems once each, with the fix (a primitive that still blocks pawns, push model off, ...). */
	UPROPERTY(config, EditAnywhere, Category = "Diagnostics")
	bool bReportSetupProblems = true;

	/** Report each world's soft collision state to the Extended.Status console command and overlay. */
	UPROPERTY(config, EditAnywhere, Category = "Diagnostics")
	bool bReportStatus = true;
};
