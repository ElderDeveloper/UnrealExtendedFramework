// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "EGChessInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

/** How a seated player points at squares. */
UENUM(BlueprintType)
enum class EEGChessAimMode : uint8
{
	/** No cursor: the square at the centre of the view is the one under the cursor, and the mouse turns the camera. */
	ViewCentre,
	/** A free mouse cursor picks squares; the camera turns while the right mouse button is held. */
	MouseCursor,
};

/**
 * Board input. With no config (or an empty field) the player component builds a default action
 * and mapping in code: mouse, arrows/WASD, and gamepad.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGChessInputConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Axis2D. Moves the square cursor one square per step; X is right, Y is away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UInputAction> Cursor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UInputAction> Select = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UInputAction> Cancel = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UInputAction> ToggleView = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UInputAction> Menu = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UInputAction> Leave = nullptr;

	/** How the player points at squares. The default, ViewCentre, is first-person play with no cursor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess|Look")
	EEGChessAimMode AimMode = EEGChessAimMode::ViewCentre;

	/**
	 * Axis2D, in degrees: X turns the seated camera right, Y tilts it up. It is added to the view as
	 * an offset, so an authored mapping carries its own sensitivity (Scalar / Scale By Delta Time).
	 * The default mapping is the mouse (only while the right button is held in MouseCursor mode) and
	 * the right stick, scaled by the values below.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess|Look")
	TObjectPtr<UInputAction> Look = nullptr;

	/**
	 * Default mapping only: degrees per unit of mouse movement. A unit is what Mouse2D reports, i.e.
	 * pixels times the project's mouse axis sensitivity (0.07 by default), so 1.25 is about 0.09
	 * degrees per pixel, the usual first-person feel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess|Look", meta = (ClampMin = "0.0"))
	float MouseLookDegreesPerUnit = 1.25f;

	/** Default mapping only: degrees per second at full right-stick deflection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess|Look", meta = (ClampMin = "0.0"))
	float GamepadLookDegreesPerSecond = 120.0f;

	/** How far the seated camera may turn away from the board, either side. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess|Look", meta = (ClampMin = "0.0", ClampMax = "179.0"))
	float MaxLookYaw = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess|Look", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxLookPitchUp = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess|Look", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxLookPitchDown = 45.0f;

	/** ViewCentre only: a small dot at the centre of the screen while seated, drawn in code (no asset). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess|Look")
	bool bShowCentreDot = true;

	/** Diameter in slate units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess|Look", meta = (ClampMin = "1.0", EditCondition = "bShowCentreDot"))
	float CentreDotSize = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess|Look", meta = (EditCondition = "bShowCentreDot"))
	FLinearColor CentreDotColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	TObjectPtr<UInputMappingContext> MappingContext = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess")
	int32 MappingPriority = 10;

	/** Seconds a held cursor direction waits before repeating, then the repeat interval. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0.05"))
	float CursorRepeatDelay = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chess", meta = (ClampMin = "0.02"))
	float CursorRepeatInterval = 0.12f;
};
