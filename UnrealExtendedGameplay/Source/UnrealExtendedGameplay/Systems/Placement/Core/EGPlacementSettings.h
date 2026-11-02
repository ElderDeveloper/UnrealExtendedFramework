// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Settings/EFDeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"

#include "EGPlacementSettings.generated.h"

class UEGPlacementAvailabilityProvider;
class UMaterialInterface;

/** Project Settings > Extended Framework > Extended Placement. */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Extended Placement"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementSettings : public UEFDeveloperSettings
{
	GENERATED_BODY()

public:
	UEGPlacementSettings();

	static const UEGPlacementSettings* Get() { return GetDefault<UEGPlacementSettings>(); }

	/** Tells region rules where regions may not be loaded yet. Empty uses the World Partition provider. */
	UPROPERTY(config, EditAnywhere, Category = "Spatial")
	TSoftClassPtr<UEGPlacementAvailabilityProvider> AvailabilityProviderClass;

	/** Cell size of the region and anchor hash, in cm. */
	UPROPERTY(config, EditAnywhere, Category = "Spatial", meta = (ClampMin = "50.0"))
	float SpatialCellSize = 1000.0f;

	/** Entries spanning more cells than this sit in a list every query checks. */
	UPROPERTY(config, EditAnywhere, Category = "Spatial", meta = (ClampMin = "1"))
	int32 MaxCellsPerEntry = 512;

	/**
	 * Ghost material for the stock preview renderers when a renderer names none. It should read a
	 * vector parameter (the renderer's ColorParameter) for the valid, invalid and pending tints.
	 * The plugin's M_PlacementGhost by default; projects cook /UnrealExtendedGameplay/Placement
	 * (Directories To Always Cook) because a config path is not a package reference.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Preview")
	TSoftObjectPtr<UMaterialInterface> DefaultGhostMaterial;

	/** Report placement state to the Extended.Status console command and overlay. */
	UPROPERTY(config, EditAnywhere, Category = "Diagnostics")
	bool bReportStatus = true;
};
