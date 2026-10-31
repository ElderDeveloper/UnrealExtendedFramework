// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGChessBoardFeedback.h"
#include "UObject/Object.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Table/EGChessBoardGeometry.h"

#include "EGChessFeedbackRenderer.generated.h"

class UDecalComponent;
class UEGChessBoardFeedbackComponent;
class UEGChessTableStyle;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UStaticMesh;
class UTexture2D;

/**
 * UEGChessFeedbackRenderer
 *
 * Turns the per-square mark flags into something visible. Picked (and tuned) in the table
 * style; a game can write its own by subclassing.
 *
 * Instanced into the style asset and duplicated per table, so it must not hold a reflected
 * pointer back to its owner: the owner and style are kept as weak, unreflected pointers.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories, Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API UEGChessFeedbackRenderer : public UObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(UEGChessBoardFeedbackComponent* InOwner, const UEGChessTableStyle* InStyle);
	virtual void Apply(const FEGChessSquareMarks& Marks, const FEGChessBoardGeometry& Geometry) {}
	virtual void Shutdown() {}

	/** True while something on the board animates; the owner ticks the renderer only then. */
	virtual bool WantsTick() const { return false; }
	virtual void Tick(float DeltaTime) {}

protected:
	UEGChessBoardFeedbackComponent* GetOwnerComponent() const { return OwnerComponent.Get(); }
	const UEGChessTableStyle* GetStyle() const { return Style.Get(); }

	/** The style's visual for a mark type, or a built-in default. */
	struct FResolvedMarker
	{
		UStaticMesh* Mesh = nullptr;
		UMaterialInterface* Material = nullptr;
		FLinearColor Color = FLinearColor::White;
		float Scale = 1.0f;
		float HeightOffset = 0.1f;
		EEGChessMarkerShape Shape = EEGChessMarkerShape::Plate;
		float FrameWidth = 0.1f;
		FLinearColor FillColor = FLinearColor::Transparent;
		FLinearColor OutlineColor = FLinearColor::Transparent;
		float OutlineWidth = 0.0f;
		float PulseAmount = 0.0f;
		float PulsePeriod = 1.0f;
		bool bUsesFallbackMaterial = false;
		/** The style named a mesh; false when Mesh is an engine stand-in. */
		bool bAuthoredMesh = false;
	};
	FResolvedMarker ResolveMarker(EEGChessMarkType Type) const;

private:
	TWeakObjectPtr<UEGChessBoardFeedbackComponent> OwnerComponent;
	TWeakObjectPtr<const UEGChessTableStyle> Style;
};

/**
 * Default renderer: one mesh component per mark type and part (the mark, plus its fill and
 * contrast stroke where it has them), flat on the squares just above the surface. A style's own
 * mesh is instanced, a frame being four bars of it; with none, the shapes are built as a
 * procedural mesh. A change rewrites only the types whose squares changed, so all marks together
 * cost a handful of draw calls on any board. Translucent marks draw in HeightOffset order.
 */
UCLASS(meta = (DisplayName = "Instanced meshes"))
class UNREALEXTENDEDGAMEPLAY_API UEGChessFeedbackRenderer_Instanced : public UEGChessFeedbackRenderer
{
	GENERATED_BODY()

public:
	virtual void Initialize(UEGChessBoardFeedbackComponent* InOwner, const UEGChessTableStyle* InStyle) override;
	virtual void Apply(const FEGChessSquareMarks& Marks, const FEGChessBoardGeometry& Geometry) override;
	virtual void Shutdown() override;
	virtual bool WantsTick() const override;
	virtual void Tick(float DeltaTime) override;

private:
	/** A mark type's components, bottom to top. */
	enum ELayerPart : int32 { Outline, Fill, Main, NumParts };
	static int32 LayerIndex(int32 TypeIndex, ELayerPart Part) { return TypeIndex * NumParts + Part; }

	/** NumParts per mark type; null where the type has no such part. Instanced or procedural. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> Layers;

	/** Each layer's material, for the pulse. Parallel to Layers. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> LayerMaterials;

	/** The colour each layer was given, before any pulse. Parallel to Layers. */
	TArray<FLinearColor> LayerColors;

	/** Seconds since each type's squares last changed, so a newly marked square starts its pulse bright. */
	TArray<float> PulseClocks;

	/** Whether any square carries each type, as of the last Apply. */
	TArray<bool> TypeShowing;

	/** Each type's pulse amount (X) and period (Y), resolved once. */
	TArray<FVector2f> PulseSettings;

	FEGChessSquareMarks LastMarks;
	EEGChessSeat LastWhiteSeat = EEGChessSeat::A;
	float LastSquareSize = 0.0f;
	bool bHasApplied = false;
};

/** Projects each mark as a decal. Suits carved or curved boards; pieces must not receive decals. */
UCLASS(meta = (DisplayName = "Decals"))
class UNREALEXTENDEDGAMEPLAY_API UEGChessFeedbackRenderer_Decal : public UEGChessFeedbackRenderer
{
	GENERATED_BODY()

public:
	/** Projection depth, in uu. */
	UPROPERTY(EditAnywhere, Category = "Chess", meta = (ClampMin = "0.1"))
	float DecalDepth = 4.0f;

	virtual void Apply(const FEGChessSquareMarks& Marks, const FEGChessBoardGeometry& Geometry) override;
	virtual void Shutdown() override;

private:
	UDecalComponent* AcquireDecal(int32 Index);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> Pool;
};

/**
 * Writes the marks into an 8x8 texture that the game's own board material reads: texel (X, Y)
 * is the square at grid X (towards Seat B) and grid Y (to Seat A's right); its R channel holds
 * mark bits 0-7 and G bits 8-15, in EEGChessMarkType order. The most integrated look, but the
 * board material has to be built for it.
 */
UCLASS(meta = (DisplayName = "Board material"))
class UNREALEXTENDEDGAMEPLAY_API UEGChessFeedbackRenderer_BoardMaterial : public UEGChessFeedbackRenderer
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Chess")
	FName TextureParameter = TEXT("ChessMarks");

	UPROPERTY(EditAnywhere, Category = "Chess")
	int32 MaterialSlot = 0;

	virtual void Initialize(UEGChessBoardFeedbackComponent* InOwner, const UEGChessTableStyle* InStyle) override;
	virtual void Apply(const FEGChessSquareMarks& Marks, const FEGChessBoardGeometry& Geometry) override;
	virtual void Shutdown() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MarksTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BoardMaterial;
};
