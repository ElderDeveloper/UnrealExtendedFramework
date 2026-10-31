// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameplayTagContainer.h"

#include "EGInteractionTypes.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EEGInteractionTraceMode : uint8
{
	/** Camera-centre line trace only. */
	CameraCenter UMETA(DisplayName = "Camera Center Line Trace"),
	/** Line trace first for precision, sphere sweep as a fallback. */
	SphereSweep UMETA(DisplayName = "Sphere Sweep")
};

UENUM(BlueprintType)
enum class EEGInteractionPriority : uint8
{
	Nearest UMETA(DisplayName = "Nearest"),
	MostCentered UMETA(DisplayName = "Most Centered")
};

UENUM(BlueprintType)
enum class EEGInteractionDebugVerbosity : uint8
{
	Basic UMETA(DisplayName = "Basic (Focus Changes Only)"),
	Detailed UMETA(DisplayName = "Detailed (Every Trace)"),
	Verbose UMETA(DisplayName = "Verbose (Tags + Distances)")
};

/** What an interactable wants shown while it is focused. Queried during focus updates. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGInteractionPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Extended|Interaction")
	FText Text;

	UPROPERTY(BlueprintReadWrite, Category = "Extended|Interaction")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/**
	 * Show the ordinary Text + Icon prompt even when the prompt widget is set to use its static icon
	 * (which otherwise hides the text and keeps the icon authored on the widget). Leave it off to let
	 * the widget decide, which is what an interactable that merely forwards its catalog icon wants.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Extended|Interaction")
	bool bOverrideStaticIcon = false;

	/**
	 * In static-icon mode, draw this instead of the icon authored on the prompt widget - still icon
	 * only, no text, at the authored icon's size. For an interactable whose kind should read at a
	 * glance (an eye on a hiding place instead of the hand). Null keeps the authored icon.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Extended|Interaction")
	TObjectPtr<UTexture2D> StaticIcon = nullptr;

	/**
	 * A second line under the prompt that says how to use what is looked at ("Hold to hide"). Shown
	 * in static-icon mode too; the prompt widget decides how to draw it. Empty draws nothing.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Extended|Interaction")
	FText HintText;

	/**
	 * Lead HintText with the interact key, for a line that says what pressing it does. Off for a line
	 * that only informs (a balance, a reason it cannot be used), where a key in front reads as an action.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Extended|Interaction")
	bool bHintShowsKey = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEGInteractionFocusChanged, AActor*, FocusedActor, FText, InteractionText, UTexture2D*, Icon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEGInteractionFocusLost);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGInteractionFailed, AActor*, TargetActor, FText, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGInteractionHoldProgressChanged, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGInteractableInteracted, AActor*, Interactor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGInteractableHoldStateChanged, bool, bHoldActive, AActor*, Interactor);
