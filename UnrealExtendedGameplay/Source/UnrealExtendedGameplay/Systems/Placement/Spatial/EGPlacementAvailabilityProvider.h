// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "EGPlacementAvailabilityProvider.generated.h"

class UWorld;

/**
 * Tells the spatial subsystem whether an area's regions may be missing because they are not
 * streamed in. Region rules answer Pending where it says no. Replace it (Project Settings >
 * Extended Placement, or UEGPlacementSpatialSubsystem::SetAvailabilityProvider) for a project's
 * own streaming.
 *
 * The default asks World Partition whether streaming is complete for the bounds. Worlds without
 * World Partition are always available.
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementAvailabilityProvider : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Placement")
	bool IsAreaAvailable(UWorld* World, const FBox& Bounds) const;
	virtual bool IsAreaAvailable_Implementation(UWorld* World, const FBox& Bounds) const;
};
