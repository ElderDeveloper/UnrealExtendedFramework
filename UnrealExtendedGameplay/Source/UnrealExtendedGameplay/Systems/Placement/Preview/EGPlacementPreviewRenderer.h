// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UnrealExtendedGameplay/Systems/Placement/Session/EGPlacementSessionTypes.h"

#include "EGPlacementPreviewRenderer.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;

/** How a ghost looks for each status. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementPreviewLook
{
	GENERATED_BODY()

	/** Drawn on every slot. Empty uses Project Settings > Extended Placement's default; empty there keeps the meshes' own materials and only sets the colour parameter on them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TSoftObjectPtr<UMaterialInterface> GhostMaterial;

	/** Vector parameter that receives the status colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FName ColorParameter = TEXT("Color");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FLinearColor ValidColor = FLinearColor(0.15f, 0.9f, 0.35f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FLinearColor InvalidColor = FLinearColor(1.0f, 0.12f, 0.08f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FLinearColor PendingColor = FLinearColor(1.0f, 0.75f, 0.1f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	bool bCastShadows = false;
};

/**
 * UEGPlacementPreviewRenderer
 * Draws a selection where the session's evaluation says, in a valid, invalid or pending look.
 * Instanced into a session; one instance serves one session.
 *
 * Previews never collide, affect navigation, replicate or tick unless they animate, and the plugin
 * never spawns a gameplay class for one. A project that wants an actor-based preview supplies its
 * own renderer and makes its actor inert before FinishSpawning.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories, Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementPreviewRenderer : public UObject
{
	GENERATED_BODY()

public:
	/** Owner hosts any components the renderer makes. Assets from GatherPreload are loaded. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Placement")
	void Begin(AActor* Owner, const FEGPlacementSelection& Selection);
	virtual void Begin_Implementation(AActor* Owner, const FEGPlacementSelection& Selection) {}

	/** bHasPose is false when no mode produced a pose (nothing aimed at). */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Placement")
	void Update(const FEGPlacementEvaluation& Evaluation, bool bHasPose);
	virtual void Update_Implementation(const FEGPlacementEvaluation& Evaluation, bool bHasPose) {}

	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Placement")
	void End();
	virtual void End_Implementation() {}

	/** Soft assets the session loads before Begin. */
	virtual void GatherPreload(const FEGPlacementSelection& Selection, TArray<FSoftObjectPath>& OutPaths) const;
};

/**
 * Draws an FEGPlacementVisual: one transient mesh component per mesh on the session's owner,
 * absolute, without collision, navigation, shadows or tick.
 */
UCLASS(meta = (DisplayName = "Mesh"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementPreview_Mesh : public UEGPlacementPreviewRenderer
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FEGPlacementPreviewLook Look;

	virtual void Begin_Implementation(AActor* Owner, const FEGPlacementSelection& Selection) override;
	virtual void Update_Implementation(const FEGPlacementEvaluation& Evaluation, bool bHasPose) override;
	virtual void End_Implementation() override;
	virtual void GatherPreload(const FEGPlacementSelection& Selection, TArray<FSoftObjectPath>& OutPaths) const override;

protected:
	/** Builds the components for Visual. */
	void BeginVisual(AActor* Owner, const FEGPlacementVisual& Visual);
	void ApplyStatus(EEGPlacementStatus Status);
	UMaterialInterface* ResolveGhostMaterial() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> Components;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	TArray<FTransform> RelativeTransforms;
	TOptional<EEGPlacementStatus> AppliedStatus;
	bool bVisible = false;
};

/**
 * Builds the visual from the selection's actor class without spawning it: the mesh components of
 * the class default object and of the Blueprint construction scripts, parents and inherited
 * overrides included. Cached per class; the session loads the class asynchronously first. A
 * selection that brings its own visual uses that.
 */
UCLASS(meta = (DisplayName = "Actor class"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementPreview_ActorClass : public UEGPlacementPreview_Mesh
{
	GENERATED_BODY()

public:
	virtual void Begin_Implementation(AActor* Owner, const FEGPlacementSelection& Selection) override;
	virtual void GatherPreload(const FEGPlacementSelection& Selection, TArray<FSoftObjectPath>& OutPaths) const override;
};
