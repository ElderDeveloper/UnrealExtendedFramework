// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTypes.h"

#include "EGPlacementAimProvider.generated.h"

class APlayerController;
class UEGPlacementAnchorComponent;
class UEGPlacementProfile;

/** Where a placement is aimed from: a camera, a motion controller, a script. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FVector Origin = FVector::ZeroVector;

	/** Unit direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TWeakObjectPtr<AActor> Requester;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TWeakObjectPtr<APlayerController> PlayerController;
};

/**
 * UEGPlacementAimProvider
 * Turns a view into an intent. Instanced into a session's modes, so one instance serves one
 * session and may keep aim state (a distance, the anchor it is holding on to).
 *
 * Intents follow the pivot-pose convention: the pivot ContactOffset above the aimed point along
 * the surface normal, the object's up along that normal.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories, Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementAimProvider : public UObject
{
	GENERATED_BODY()

public:
	/** False when this mode has nothing to aim at; the session then treats the mode as invalid without a pose. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Placement")
	bool BuildIntent(const FEGPlacementView& View, const FEGPlacementSpec& Spec, const UEGPlacementProfile* Profile, const FEGPlacementContext& Context, FEGPlacementIntent& OutIntent);
	virtual bool BuildIntent_Implementation(const FEGPlacementView& View, const FEGPlacementSpec& Spec, const UEGPlacementProfile* Profile, const FEGPlacementContext& Context, FEGPlacementIntent& OutIntent) { return false; }

	/** Push the aim further (positive) or closer (negative), in the provider's own steps. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Placement")
	void AdjustDistance(float Delta);
	virtual void AdjustDistance_Implementation(float Delta) {}

	/** Forget aim state; the session calls it when a selection starts. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Placement")
	void ResetAim();
	virtual void ResetAim_Implementation() {}

	/** A rotation whose up is Up, facing along ViewDirection (or back at the viewer). */
	static FQuat MakeFacingRotation(const FVector& Up, const FVector& ViewDirection, bool bFaceViewer);
};

/**
 * Traces from the view, with an adjustable distance and a bounded surface search: a floor item
 * aimed at a wall, or into the air, searches downwards for a floor; a ceiling item searches up.
 */
UCLASS(meta = (DisplayName = "View trace"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementAim_ViewTrace : public UEGPlacementAimProvider
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "1.0"))
	float MaxDistance = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0"))
	float MinDistance = 60.0f;

	/** Distance change per AdjustDistance unit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0"))
	float DistanceStep = 25.0f;

	/** How far the search for an allowed surface goes from where the aim ended. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0"))
	float SurfaceSearchDistance = 300.0f;

	/** The object faces back at the viewer; off faces away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bFaceViewer = true;

	virtual bool BuildIntent_Implementation(const FEGPlacementView& View, const FEGPlacementSpec& Spec, const UEGPlacementProfile* Profile, const FEGPlacementContext& Context, FEGPlacementIntent& OutIntent) override;
	virtual void AdjustDistance_Implementation(float Delta) override;
	virtual void ResetAim_Implementation() override;

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	float GetCurrentDistance() const { return CurrentDistance > 0.0f ? CurrentDistance : MaxDistance; }

private:
	float CurrentDistance = 0.0f;
};

/**
 * Picks an anchor from the view: the compatible anchor closest to the view ray, within its snap
 * radius of the ray. Holds on to the current anchor unless another is clearly better, so the
 * choice does not flicker between two close anchors. Hysteresis affects only this local choice;
 * the authority still validates the chosen anchor.
 */
UCLASS(meta = (DisplayName = "Anchor pick"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementAim_AnchorPick : public UEGPlacementAimProvider
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "1.0"))
	float MaxDistance = 400.0f;

	/** Multiplies each anchor's snap radius to get how far from the view ray it may be. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.1"))
	float AimToleranceScale = 1.0f;

	/** A new anchor must score this much better (0.25 = 25 %) to replace the current one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Hysteresis = 0.25f;

	/** Matched against the anchors' AnchorTags. Empty: every anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FGameplayTagQuery AnchorFilter;

	/** Skip anchors that are not available for the spec (occupied), instead of showing them refused. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bSkipUnavailable = true;

	/** Ignore anchors hidden behind something other than their own actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bRequireLineOfSight = true;

	virtual bool BuildIntent_Implementation(const FEGPlacementView& View, const FEGPlacementSpec& Spec, const UEGPlacementProfile* Profile, const FEGPlacementContext& Context, FEGPlacementIntent& OutIntent) override;
	virtual void ResetAim_Implementation() override;

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	UEGPlacementAnchorComponent* GetCurrentAnchor() const { return Current.Get(); }

private:
	/** Lower is better; negative when the anchor does not qualify. */
	double Score(const UEGPlacementAnchorComponent& Anchor, const FEGPlacementView& View, const FEGPlacementSpec& Spec, const UEGPlacementProfile* Profile, const FEGPlacementContext& Context) const;

	TWeakObjectPtr<UEGPlacementAnchorComponent> Current;
};
