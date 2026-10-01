// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Core/EGChessTypes.h"

#include "EGChessPieceSet.generated.h"

class UCurveFloat;
class UMaterialInterface;
class UStaticMesh;
class UTexture2D;

/** How one piece type looks and is picked. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGChessPieceVisual
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** For sets whose black pieces are different models. Empty uses Mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TSoftObjectPtr<UStaticMesh> MeshBlack;

	/** Material slot overrides for white, by slot index. Empty keeps the mesh's own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TArray<TSoftObjectPtr<UMaterialInterface>> WhiteMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TArray<TSoftObjectPtr<UMaterialInterface>> BlackMaterials;

	/**
	 * Extra yaw on top of facing the opponent, for meshes authored facing another way. The table
	 * already turns black pieces around, so a symmetric set leaves both at 0.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	float YawOffsetWhite = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	float YawOffsetBlack = 0.0f;

	/** For meshes whose origin is not at the base centre. In mesh units, before scaling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	FVector PivotOffset = FVector::ZeroVector;

	/** Upright capsule used for mouse picking, in authored units. No collision setup needed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0.1"))
	float PickRadius = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0.1"))
	float PickHeight = 8.0f;

	/** HUD icons. Empty icons are rendered from the mesh at runtime (or shown as letters). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TSoftObjectPtr<UTexture2D> IconWhite;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TSoftObjectPtr<UTexture2D> IconBlack;
};

/**
 * The pieces. One set per table, used for both colours. Everything is optional: a table with
 * no set builds its pieces from engine basic shapes.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGChessPieceSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	FEGChessPieceVisual King;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	FEGChessPieceVisual Queen;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	FEGChessPieceVisual Rook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	FEGChessPieceVisual Bishop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	FEGChessPieceVisual Knight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	FEGChessPieceVisual Pawn;

	/** The square size the set was modelled for. Pieces scale by board square / this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scale", meta = (ClampMin = "0.1", Units = "cm"))
	float AuthoredSquareSize = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scale")
	bool bScaleToBoard = true;

	/** How high a moving piece lifts, in authored units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move feel", meta = (ClampMin = "0"))
	float LiftHeight = 4.0f;

	/** Travel time when no character drives the move. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move feel", meta = (ClampMin = "0.05", Units = "s"))
	float MoveTime = 0.45f;

	/** 0..1 over the travel, 0..1 height. Empty uses a sine arc. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move feel")
	TSoftObjectPtr<UCurveFloat> MoveArc;

	/** Takebacks play back at this speed-up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move feel", meta = (ClampMin = "1"))
	float TakebackSpeed = 2.0f;

	/** Glide home at game start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move feel", meta = (ClampMin = "0.05", Units = "s"))
	float ResetTime = 1.0f;

	/** See-through material for the move preview. Empty means no ghost piece. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<UMaterialInterface> GhostMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	bool bToppleKingOnLoss = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0.05", Units = "s"))
	float ToppleTime = 0.8f;

	/** Custom-depth stencil for hovered and capture-target pieces. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0", ClampMax = "255"))
	int32 OutlineStencil = 120;

	/** Custom-depth stencil for the selected piece, so it reads apart from hovered ones. 0 = OutlineStencil. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0", ClampMax = "255"))
	int32 SelectedOutlineStencil = 0;

	/** How far the selected piece floats off its square, in authored units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0"))
	float SelectedLiftHeight = 2.5f;

	/** Render missing icons from the meshes at runtime. Off falls back to letters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	bool bCaptureIconsAtRuntime = true;

	const FEGChessPieceVisual& GetVisual(EEGChessPieceType Type) const;

	void GatherSoftPaths(TArray<FSoftObjectPath>& OutPaths) const;
};
