// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"

#include "EGChessFeedbackTypes.generated.h"

class UMaterialInterface;
class UStaticMesh;

/** One kind of mark a square can carry. A square can carry several at once. */
UENUM(BlueprintType)
enum class EEGChessMarkType : uint8
{
	/** Square under the cursor. Local. */
	Cursor,
	/** The selected piece. Local. */
	Selected,
	/** An empty legal target. Local. */
	Move,
	/** A legal target holding an enemy piece (and the pawn an en passant capture takes). Local. */
	Capture,
	/** The legal target under the cursor. Local. */
	HoverTarget,
	/** The rook that will jump when castling. Local. */
	CastlePartner,
	/** Shared. */
	LastMoveFrom,
	/** Shared. */
	LastMoveTo,
	/** The king in check, and the mated king at game end. Shared. */
	Check,
	/** Short flash on the square of a refused confirm. Local. */
	Illegal,

	Count UMETA(Hidden)
};

namespace EGChessMarks
{
	inline uint16 Bit(EEGChessMarkType Type) { return static_cast<uint16>(1u << static_cast<uint8>(Type)); }
	inline bool Has(uint16 Flags, EEGChessMarkType Type) { return (Flags & Bit(Type)) != 0; }
	constexpr int32 NumTypes = static_cast<int32>(EEGChessMarkType::Count);
}

/** The outline of a mark, as the instanced renderer draws it. */
UENUM(BlueprintType)
enum class EEGChessMarkerShape : uint8
{
	/** A filled square. */
	Plate,
	/** A hollow square, FrameWidth thick. */
	Frame,
	/** A filled circle (the engine cylinder, flattened, when no Mesh is set). */
	Disc,
};

/** How one mark type looks. Everything optional; engine shapes fill the gaps. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGChessMarkerVisual
{
	GENERATED_BODY()

	/** Flat mesh, 100 uu across at scale 1 (the engine plane's size). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Unlit translucent works best. With none, an unlit translucent engine material is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TSoftObjectPtr<UMaterialInterface> Material;

	/** Pushed to the material's "Color" vector parameter. Alpha is the opacity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	FLinearColor Color = FLinearColor::White;

	/** Size as a fraction of a square. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0.01"))
	float Scale = 1.0f;

	/** Height above the playing surface, in uu. Keeps marks from z-fighting the board; higher marks draw on top. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	float HeightOffset = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	EEGChessMarkerShape Shape = EEGChessMarkerShape::Plate;

	/** Thickness of a Frame, as a fraction of a square. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0.005", EditCondition = "Shape == EEGChessMarkerShape::Frame"))
	float FrameWidth = 0.1f;

	/** Drawn inside a Frame, under it. Alpha 0 leaves the frame hollow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (EditCondition = "Shape == EEGChessMarkerShape::Frame"))
	FLinearColor FillColor = FLinearColor::Transparent;

	/**
	 * A stroke drawn under the mark, OutlineWidth past each of its edges. A dark stroke under a
	 * light mark reads on light and dark squares alike. Alpha 0 draws none.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	FLinearColor OutlineColor = FLinearColor::Transparent;

	/** As a fraction of a square. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0"))
	float OutlineWidth = 0.03f;

	/** How far the mark dims at the low point of its pulse, 0..1. 0 holds it steady. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0", ClampMax = "1"))
	float PulseAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0.1", Units = "s"))
	float PulsePeriod = 1.0f;
};

/** Per-player marker options, for a game to bind to its own settings. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGChessFeedbackOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess")
	bool bShowLegalMoves = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess")
	bool bShowLastMove = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess")
	bool bShowCheck = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess")
	bool bShowGhostPiece = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess")
	bool bShowCoordinates = false;

	/**
	 * Mark the square under the cursor even with nothing selected. Off by default: the hovered piece
	 * is outlined, so a square mark only adds noise until a piece is picked up.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess")
	bool bShowCursorWithoutSelection = false;

	/** All marks off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess")
	bool bHardcore = false;
};
