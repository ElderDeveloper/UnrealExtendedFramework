// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EFUIInteractionScript.generated.h"

class UEFUIFixture;

/** Semantic operations recorded/replayed by the UI Lab (no screen coordinates). */
UENUM()
enum class EEFUIScriptOp : uint8
{
	/** Set Slate focus to Target (automation identity). */
	SetFocus,
	/** Assert focus is on Target; polls until TimeoutSeconds. */
	ExpectFocus,
	/** Navigate: Value = Up/Down/Left/Right (injected as D-pad input through real routing). */
	Navigate,
	/** Press and release the key named in Value (e.g. "Gamepad_FaceButton_Bottom", "Enter"). */
	PressKey,
	/** Click the center of Target through real pointer routing. */
	Click,
	/** Type Value as character input. */
	TypeText,
	/** Wait Value seconds (e.g. for animations). */
	Wait,
	/** Assert Target visibility == Value ("true"/"false"); polls until timeout. */
	ExpectVisible,
	/** Assert Target enabled state == Value ("true"/"false"); polls until timeout. */
	ExpectEnabled,
	/** Assert Target's Text equals Value; polls until timeout. */
	ExpectText,
	/** Assert Target property: Value = "Property=ExpectedExportedValue"; polls until timeout. */
	ExpectValue,
	/** Record a diagnostic snapshot (focus, widget counts) into the results. */
	Capture
};

USTRUCT()
struct FEFUIScriptStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Step")
	EEFUIScriptOp Op = EEFUIScriptOp::ExpectFocus;

	/** Automation identity of the target widget (where applicable). */
	UPROPERTY(EditAnywhere, Category = "Step")
	FName Target;

	/** Operation payload (key name, direction, text, expected value, seconds). */
	UPROPERTY(EditAnywhere, Category = "Step")
	FString Value;

	/** Expect/Wait budget in seconds. */
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float TimeoutSeconds = 2.0f;

	/** Navigate/PressKey execute this many times (recording collapses repeated presses). */
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = "1", ClampMax = "100"))
	int32 RepeatCount = 1;
};

/**
 * UL-6 interaction script: a fixture plus semantic steps, stored as a data asset in the
 * UncookedOnly editor module. Runs interactively in the UI Lab and through Unreal automation.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDFRAMEWORKEDITOR_API UEFUIInteractionScript : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Fixture opened before the steps run. */
	UPROPERTY(EditAnywhere, Category = "Script")
	TObjectPtr<UEFUIFixture> Fixture;

	/**
	 * UL-7 variant matrix: additional fixtures (typically variants of Fixture — cultures,
	 * resolutions, input devices, states) the script also runs against. Automation registers
	 * one test per variant, reporting layout/focus/input results per variant.
	 */
	UPROPERTY(EditAnywhere, Category = "Script")
	TArray<TObjectPtr<UEFUIFixture>> MatrixFixtures;

	UPROPERTY(EditAnywhere, Category = "Script")
	TArray<FEFUIScriptStep> Steps;

	/** Excluded from the auto-registered automation pass (still runnable interactively). */
	UPROPERTY(EditAnywhere, Category = "Script")
	bool bSkipInAutomation = false;
};
