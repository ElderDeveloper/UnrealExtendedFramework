// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Feedback/EGChessFeedbackTypes.h"

#include "EGChessTableStyle.generated.h"

class UEGChessFeedbackRenderer;
class UMaterialInterface;
class UStaticMesh;

/**
 * The furniture and the board. Everything optional; a table with no style draws a flat board of
 * engine planes and no clock.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGChessTableStyle : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UEGChessTableStyle();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	TSoftObjectPtr<UStaticMesh> TableMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	TSoftObjectPtr<UStaticMesh> ChairMesh;

	/** Custom-depth stencil for the chair the player is looking at. 0 disables the outline. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture", meta = (ClampMin = "0", ClampMax = "255"))
	int32 ChairOutlineStencil = 120;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Board")
	TSoftObjectPtr<UStaticMesh> BoardMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Board", meta = (ClampMin = "0.1", Units = "cm"))
	float SquareSize = 5.0f;

	/**
	 * Where the board's squares are, relative to the board mesh: the centre of the corner square
	 * on Seat A's left, on the playing surface, with +X towards Seat B and +Y to Seat A's right.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Board")
	FTransform BoardSurfaceTransform;

	/** Lights and darks of the fallback board when there is no board mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Board")
	FLinearColor FallbackLightColor = FLinearColor(0.8f, 0.7f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Board")
	FLinearColor FallbackDarkColor = FLinearColor(0.25f, 0.15f, 0.08f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coordinates")
	bool bShowCoordinatesByDefault = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coordinates")
	FLinearColor CoordinateColor = FLinearColor(0.9f, 0.85f, 0.75f);

	/** Text world size as a fraction of a square. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coordinates", meta = (ClampMin = "0.05"))
	float CoordinateSize = 0.3f;

	/** Distance from the board edge as a fraction of a square. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coordinates")
	float CoordinateOffset = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock")
	TSoftObjectPtr<UStaticMesh> ClockMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock")
	TSoftObjectPtr<UStaticMesh> ClockButtonMesh;

	/** Socket on the clock mesh where Seat A's button sits (raised). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock")
	FName ButtonSocketA = TEXT("ButtonWhite");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock")
	FName ButtonSocketB = TEXT("ButtonBlack");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock", meta = (ClampMin = "0", Units = "cm"))
	float ButtonPressTravel = 0.8f;

	/** Optional dial hand. Empty means the dials show nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock")
	TSoftObjectPtr<UStaticMesh> ClockHandMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock")
	FName HandSocketA = TEXT("HandA");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock")
	FName HandSocketB = TEXT("HandB");

	/** Axis the hands turn around, in socket space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock")
	FVector HandAxis = FVector::ForwardVector;

	/** A minute hand turns 6 degrees per minute left. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock")
	float HandDegreesPerMinute = 6.0f;

	/** How marks are drawn. Empty uses the instanced renderer. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Markers")
	TObjectPtr<UEGChessFeedbackRenderer> Renderer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Markers")
	TMap<EEGChessMarkType, FEGChessMarkerVisual> Markers;

	const FEGChessMarkerVisual* FindMarker(EEGChessMarkType Type) const { return Markers.Find(Type); }

	void GatherSoftPaths(TArray<FSoftObjectPath>& OutPaths) const;
};
