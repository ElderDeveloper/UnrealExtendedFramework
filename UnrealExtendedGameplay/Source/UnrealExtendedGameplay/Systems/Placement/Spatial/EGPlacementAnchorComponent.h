// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTypes.h"

#include "EGPlacementAnchorComponent.generated.h"

class UPrimitiveComponent;

/**
 * A socket for exact placement: a hook, a slot, a pedestal. The component's transform is the pose
 * an object takes (its pivot, or its contact point with UEGPlacementSolver_Anchor's
 * bAnchorsMarkContact).
 *
 * Whether it is occupied is project state. The plugin asks through IsAvailableFor (override it,
 * or use a project rule) and never tracks occupancy itself. When occupancy changes, call
 * SetExternalRevision with a value both machines agree on (a replicated counter, the occupant's
 * id hash), so cached answers refresh and a stale commit is caught.
 */
UCLASS(ClassGroup = (Extended), Blueprintable, meta = (BlueprintSpawnableComponent), HideCategories = (Mobility, Rendering, Physics, Collision, Activation))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementAnchorComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UEGPlacementAnchorComponent();

	/** What this anchor is ("hook", "portal socket"), for rules and aim filters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FGameplayTagContainer AnchorTags;

	/** Matched against the spec's tags. Empty accepts every spec. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FGameplayTagQuery AcceptedSpecs;

	/** How far from the anchor a proposed pivot may be and still snap to it. AnchorPick also uses it as its aim tolerance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.1"))
	float SnapRadius = 30.0f;

	/** Let the object spin about the anchor's up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bAllowSpin = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (EditCondition = "bAllowSpin", ClampMin = "0.0", ClampMax = "180.0"))
	float SpinStepDegrees = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bAnchorEnabled = true;

	/** Whether the anchor takes this object now. Default: always. Occupancy is project state. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Placement")
	bool IsAvailableFor(const FEGPlacementSpec& Spec, const FEGPlacementContext& Context) const;
	virtual bool IsAvailableFor_Implementation(const FEGPlacementSpec& Spec, const FEGPlacementContext& Context) const { return true; }

	/** Enabled and AcceptedSpecs matches the spec's tags. */
	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	bool Accepts(const FEGPlacementSpec& Spec) const;

	/** The component's world transform without scale. */
	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	FTransform GetAnchorTransform() const;

	/** The first primitive up the attach chain, or the owner's root primitive: what an anchored object rests on. */
	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	UPrimitiveComponent* GetSupportPrimitive() const;

	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	void SetAnchorEnabled(bool bEnabled);

	/** Project-owned part of the revision, for occupancy. Both machines must set the same value. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	void SetExternalRevision(int32 InExternalRevision);

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

	int32 ExternalRevision = 0;
	int32 Revision = 0;
	bool bRegistered = false;
};
