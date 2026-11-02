// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTypes.h"
#include "UnrealExtendedGameplay/Systems/Placement/Preview/EGPlacementVisuals.h"

#include "EGPlaceableDefinition.generated.h"

class AActor;

/**
 * A placeable item for projects without their own item data: spec, preview visual and the actor to
 * spawn. Projects with their own data map it to a spec and skip this. Cooking these assets is the
 * owning project's job (Asset Manager primary asset type "EGPlaceable", or a hard reference).
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGPlaceableDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FEGPlacementSpec Spec;

	/** What the preview draws. Empty: built from ActorClass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FEGPlacementVisual Visual;

	/** What the SpawnActor commit handler spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	TSoftClassPtr<AActor> ActorClass;

	/** Source handle for this definition: Namespace "EGPlaceable", Value the asset name, unless the spec names its own. */
	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	FEGPlacementHandle GetSourceHandle() const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;

	/** Fits footprint boxes to the visual's meshes (or the actor class's) by their simple collision. */
	UFUNCTION(CallInEditor, Category = "Placement")
	void FitShapeFromCollision();

	/** Fits one footprint box per mesh to the visual's render bounds. */
	UFUNCTION(CallInEditor, Category = "Placement")
	void FitShapeFromBounds();

	void FitShape(EEGPlacementFitSource Source, float Padding = 0.0f);
#endif
};

/** A DataTable row with the same data as UEGPlaceableDefinition. The row name becomes the source handle. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementSpecRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FEGPlacementSpec Spec;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FEGPlacementVisual Visual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	TSoftClassPtr<AActor> ActorClass;
};
