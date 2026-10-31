// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "EGAnimEventTypes.generated.h"

class AActor;
class UAnimSequenceBase;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EEGAnimEventPhase : uint8
{
	/** A single-frame notify fired. */
	Trigger,
	/** A notify window opened. */
	Begin,
	/** A notify window ticked (only when the notify asks for tick events). */
	Tick,
	/** A notify window closed, including when its montage was interrupted. */
	End
};

/** Data authored on the notify and handed to whoever receives the event. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGAnimEventPayload
{
	GENERATED_BODY()

	/** A socket or bone the receiver should use (a foot, a weapon tip). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim Event")
	FName SocketName;

	/** Free scalar: a damage multiplier, a loudness, an intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim Event")
	float Magnitude = 1.0f;

	/** Extra context tags (a damage type, a surface override, a combo branch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim Event")
	FGameplayTagContainer ExtraTags;

	/** Optional asset the receiver understands (a data asset, a sound, a curve). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim Event")
	TObjectPtr<UObject> OptionalObject = nullptr;
};

/** One event as received by IEGAnimEventReceiver. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGAnimEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Anim Event")
	FGameplayTag Tag;

	UPROPERTY(BlueprintReadOnly, Category = "Anim Event")
	EEGAnimEventPhase Phase = EEGAnimEventPhase::Trigger;

	UPROPERTY(BlueprintReadOnly, Category = "Anim Event")
	FEGAnimEventPayload Payload;

	UPROPERTY(BlueprintReadOnly, Category = "Anim Event")
	TObjectPtr<USkeletalMeshComponent> Mesh = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Anim Event")
	TObjectPtr<UAnimSequenceBase> Animation = nullptr;

	/** Begin: the window's length in seconds. Tick: the frame's delta time. */
	UPROPERTY(BlueprintReadOnly, Category = "Anim Event")
	float Time = 0.0f;

	/**
	 * Identifies one window across its Begin, Tick and End events. Two windows with the same tag
	 * can overlap (a combo blending into the next attack); close by this id, not by tag.
	 * INDEX_NONE for single-frame events.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Anim Event")
	int32 WindowId = INDEX_NONE;

	AActor* GetOwner() const;
};
