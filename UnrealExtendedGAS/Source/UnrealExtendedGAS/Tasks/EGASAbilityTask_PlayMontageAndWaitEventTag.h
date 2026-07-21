#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "CoreMinimal.h"

#include "EGASAbilityTask_PlayMontageAndWaitEventTag.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FEGASPlayMontageAndWaitForEventDelegate,
	FGameplayTag, EventTag,
	FGameplayEventData, EventData);

/** Plays a montage while forwarding matching Gameplay Events to the owning ability. */
UCLASS()
class UNREALEXTENDEDGAS_API UEGASAbilityTask_PlayMontageAndWaitEventTag : public UAbilityTask
{
	GENERATED_BODY()

public:
	UEGASAbilityTask_PlayMontageAndWaitEventTag();

	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Ability Tasks", meta = (DisplayName = "Play Montage And Wait For Event Tag", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UEGASAbilityTask_PlayMontageAndWaitEventTag* PlayMontageAndWaitForEventTag(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		UAnimMontage* MontageToPlay,
		FGameplayTagContainer EventTags,
		float PlaybackRate = 1.0f,
		FName StartSection = NAME_None,
		bool bStopWhenAbilityEnds = true,
		float AnimRootMotionTranslationScale = 1.0f);

	virtual void Activate() override;
	virtual void ExternalCancel() override;
	virtual FString GetDebugString() const override;

	UPROPERTY(BlueprintAssignable)
	FEGASPlayMontageAndWaitForEventDelegate OnMontageCompleted;

	UPROPERTY(BlueprintAssignable)
	FEGASPlayMontageAndWaitForEventDelegate OnMontageBlendOut;

	UPROPERTY(BlueprintAssignable)
	FEGASPlayMontageAndWaitForEventDelegate OnMontageInterrupted;

	UPROPERTY(BlueprintAssignable)
	FEGASPlayMontageAndWaitForEventDelegate OnMontageCancelled;

	UPROPERTY(BlueprintAssignable)
	FEGASPlayMontageAndWaitForEventDelegate OnMontageEventReceived;

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	bool StopPlayingMontage() const;
	void ResetRootMotionScale() const;
	void HandleMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	void HandleAbilityCancelled();
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void HandleGameplayEventReceived(FGameplayTag EventTag, const FGameplayEventData* Payload) const;

	UPROPERTY()
	TObjectPtr<UAnimMontage> MontageToPlay;

	FGameplayTagContainer EventTags;
	float PlaybackRate = 1.0f;
	FName StartSection = NAME_None;
	bool bStopWhenAbilityEnds = true;
	float AnimRootMotionTranslationScale = 1.0f;
	bool bInterruptedBroadcast = false;

	FOnMontageBlendingOutStarted MontageBlendingOutDelegate;
	FOnMontageEnded MontageEndedDelegate;
	FDelegateHandle MontageCancelledHandle;
	FDelegateHandle MontageEventHandle;
};
