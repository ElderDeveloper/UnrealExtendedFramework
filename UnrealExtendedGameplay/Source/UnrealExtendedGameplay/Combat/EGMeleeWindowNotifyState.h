// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Animation/Events/EGAnimEventNotifyState.h"

#include "EGMeleeWindowNotifyState.generated.h"

/**
 * Opens melee shapes on the owner's UEGMeleeTraceComponent for the length of the window. The
 * notify only says when and which shapes; the component owns detection and the damage policy
 * owns what a hit does.
 */
UCLASS(meta = (DisplayName = "Melee Window"))
class UNREALEXTENDEDGAMEPLAY_API UEGMeleeWindowNotifyState : public UEGAnimEventNotifyStateBase
{
	GENERATED_BODY()

public:
	/** Opens every shape at or under this tag. Empty opens all shapes under ExtendedGameplay.Melee.Window. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", meta = (Categories = "ExtendedGameplay.Melee.Window"))
	FGameplayTag WindowTag;

	/** Multiplies the damage policy's damage for hits in this window (a heavy attack, a finisher). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0"))
	float DamageMultiplier = 1.0f;

	/** Handed to the hit and the damage policy (an attack type, a damage element). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee")
	FGameplayTagContainer AttackTags;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

protected:
	virtual FGameplayTag GetEventTag() const override;
	virtual FEGAnimEventPayload GetEventPayload() const override;
};
