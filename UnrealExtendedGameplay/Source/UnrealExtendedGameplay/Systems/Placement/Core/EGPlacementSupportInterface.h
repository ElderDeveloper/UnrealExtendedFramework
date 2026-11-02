// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTypes.h"

#include "EGPlacementSupportInterface.generated.h"

class UPrimitiveComponent;

UINTERFACE(MinimalAPI, BlueprintType)
class UEGPlacementSupportInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Optional, on actors that things are placed on. Without it a support is identified by its
 * component's path, reports no revision (a move is caught by solving again), carries its
 * component tags as surface tags and accepts everything the rules allow.
 *
 * In networked games, supports that are spawned at runtime should implement GetPlacementSupportHandle:
 * runtime object paths are not guaranteed to match between machines.
 */
class UNREALEXTENDEDGAMEPLAY_API IEGPlacementSupportInterface
{
	GENERATED_BODY()

public:
	/** The project's identity for this support, the same on every machine. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Placement")
	FEGPlacementHandle GetPlacementSupportHandle(const UPrimitiveComponent* Component) const;
	virtual FEGPlacementHandle GetPlacementSupportHandle_Implementation(const UPrimitiveComponent* Component) const { return FEGPlacementHandle(); }

	/** Changes whenever what rests on it would need checking again (stocked, moved, rebuilt). 0 is untracked. Same on every machine. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Placement")
	int32 GetPlacementSupportRevision(const UPrimitiveComponent* Component) const;
	virtual int32 GetPlacementSupportRevision_Implementation(const UPrimitiveComponent* Component) const { return 0; }

	/** Extra surface tags for SurfaceTags rules: "shelf", "rail", "timber". */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Placement")
	void GetPlacementSurfaceTags(const UPrimitiveComponent* Component, FGameplayTagContainer& OutTags) const;
	virtual void GetPlacementSurfaceTags_Implementation(const UPrimitiveComponent* Component, FGameplayTagContainer& OutTags) const {}

	/** Read by the SupportFilter rule. A shelf that is stocked rather than decorated refuses here. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Placement")
	bool CanSupportPlacement(const UPrimitiveComponent* Component, const FEGPlacementSpec& Spec, const FEGPlacementContext& Context) const;
	virtual bool CanSupportPlacement_Implementation(const UPrimitiveComponent* Component, const FEGPlacementSpec& Spec, const FEGPlacementContext& Context) const { return true; }
};
