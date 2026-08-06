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

UENUM(BlueprintType)
enum class EEGInteractionActivation : uint8
{
	/** Fires as soon as the interaction input is pressed. */
	Instant,
	/** Fires only after the input has been held for HoldDuration. */
	Hold
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

/** Everything about one completed interaction, handed to the success delegate. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGInteractionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Extended|Interaction")
	TObjectPtr<AActor> Interactor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Extended|Interaction")
	TObjectPtr<AActor> Target = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Extended|Interaction")
	FHitResult HitResult;

	UPROPERTY(BlueprintReadOnly, Category = "Extended|Interaction")
	FGameplayTag Mode;
};

/** Identifies one pushed entry on the interaction mode stack. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGInteractionModeHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Extended|Interaction")
	int32 Id = 0;

	bool IsValid() const { return Id != 0; }
	void Reset() { Id = 0; }

	friend bool operator==(const FEGInteractionModeHandle& Left, const FEGInteractionModeHandle& Right)
	{
		return Left.Id == Right.Id;
	}

	friend bool operator!=(const FEGInteractionModeHandle& Left, const FEGInteractionModeHandle& Right)
	{
		return !(Left == Right);
	}
};

/**
 * Priority bands for the mode stack. Higher wins; ties go to the entry pushed last.
 * A game is free to pass its own numbers, but staying inside these bands keeps the
 * ordering between unrelated systems predictable.
 */
namespace EGInteractionModePriority
{
	inline constexpr int32 Baseline = 0;
	inline constexpr int32 ToolState = 100;
	inline constexpr int32 WorldOverride = 200;
	inline constexpr int32 BlockingModal = 1000;
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEGInteractionFocusChanged, AActor*, FocusedActor, FText, InteractionText, UTexture2D*, Icon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEGInteractionFocusLost);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGInteractionSucceeded, const FEGInteractionContext&, Context);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGInteractionFailed, AActor*, TargetActor, FText, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGInteractionHoldProgressChanged, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGInteractableInteracted, AActor*, Interactor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGInteractableHoldStateChanged, bool, bHoldActive, AActor*, Interactor);
