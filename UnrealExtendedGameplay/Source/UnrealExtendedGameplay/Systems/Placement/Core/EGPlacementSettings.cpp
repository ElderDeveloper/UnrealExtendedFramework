// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementSettings.h"

#include "Materials/MaterialInterface.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementAvailabilityProvider.h"

UEGPlacementSettings::UEGPlacementSettings()
{
	DefaultGhostMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/UnrealExtendedGameplay/Placement/M_PlacementGhost.M_PlacementGhost")));
}
