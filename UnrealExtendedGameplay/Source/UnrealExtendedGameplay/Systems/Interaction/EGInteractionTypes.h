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
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEGInteractionFocusChanged, AActor*, FocusedActor, FText, InteractionText, UTexture2D*, Icon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEGInteractionFocusLost);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGInteractionFailed, AActor*, TargetActor, FText, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGInteractionHoldProgressChanged, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGInteractableInteracted, AActor*, Interactor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGInteractableHoldStateChanged, bool, bHoldActive, AActor*, Interactor);
