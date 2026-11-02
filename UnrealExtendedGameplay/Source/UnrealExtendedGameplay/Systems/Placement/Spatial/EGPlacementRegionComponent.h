// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTypes.h"

#include "EGPlacementRegionComponent.generated.h"

UENUM(BlueprintType)
enum class EEGPlacementRegionMode : uint8
{
	/** Placement zones: a RegionContainment rule requires the footprint inside them. Their tags are surface tags. */
	Include,
	/** No-build zones: a RegionExclusion rule refuses any footprint touching them. */
	Exclude,
};

/**
 * A placement zone: an oriented box that moves with its actor, so a building's shop floor moves
 * with the building. Non-colliding; it is found through UEGPlacementSpatialSubsystem, never through
 * physics. The box is BoxExtent around the component, scaled by the component's scale.
 *
 * Its revision is a hash of its transform (to 1 cm and 0.5 degrees), size, mode and tags, so two
 * machines with the same region agree on it.
 */
UCLASS(ClassGroup = (Extended), meta = (BlueprintSpawnableComponent), HideCategories = (Mobility, Rendering, Physics, Collision, Activation))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementRegionComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UEGPlacementRegionComponent();

	/** Half size, before the component's scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0"))
	FVector BoxExtent = FVector(100.0, 100.0, 50.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	EEGPlacementRegionMode Mode = EEGPlacementRegionMode::Include;

	/** Matched by region rules; include regions also lend them to the surface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FGameplayTagContainer RegionTags;

	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	void SetBoxExtent(FVector InExtent);

	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	void SetMode(EEGPlacementRegionMode InMode);

	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	void SetRegionTags(const FGameplayTagContainer& InTags);

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	FEGPlacementOrientedBox GetWorldBox() const;

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	int32 GetRevision() const { return Revision; }

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;

private:
	void Refresh();

	int32 Revision = 0;
	bool bRegistered = false;
};
