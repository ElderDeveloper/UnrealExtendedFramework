// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "EGATaskPlayMontageAndWaitEvent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGAPlayMontageAndWaitForEventDelegate, FGameplayTag, EventTag , FGameplayEventData, EventData);

UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGATaskPlayMontageAndWaitEvent : public UAbilityTask
{
	GENERATED_BODY()

	
public:
	// Sets default values for this actor's properties
	UEGATaskPlayMontageAndWaitEvent();


	virtual void Activate() override;
	virtual void ExternalCancel() override;
	virtual FString GetDebugString() const override;
	virtual void OnDestroy(bool bInOwnerFinished) override;


	UPROPERTY(BlueprintAssignable)
	FGAPlayMontageAndWaitForEventDelegate OnMontageCompleted;

	UPROPERTY(BlueprintAssignable)
	FGAPlayMontageAndWaitForEventDelegate OnMontageBlendOut;

	UPROPERTY(BlueprintAssignable)
	FGAPlayMontageAndWaitForEventDelegate OnMontageInterrupted;

	UPROPERTY(BlueprintAssignable)
	FGAPlayMontageAndWaitForEventDelegate OnMontageCancelled;

	UPROPERTY(BlueprintAssignable)
	FGAPlayMontageAndWaitForEventDelegate OnMontageEventReceived;

	UFUNCTION(BlueprintCallable, Category="UEExtended|Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UEGATaskPlayMontageAndWaitEvent* PlayMontageAndWaitForEvent
	(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		UAnimMontage* MontageToPlay,
		FGameplayTagContainer EventTags,
		float PlaybackRate = 1.f,
		FName StartSection = NAME_None,
		bool bStopWhenAbilityEnds = true,
		float AnimRootMotionTranslationScale = 1.f
	);


	private:

	UPROPERTY()
	UAnimMontage* MontageToPlay;

	UPROPERTY()
	FGameplayTagContainer EventTags;

	UPROPERTY()
	float PlaybackRate;

	UPROPERTY()
	FName StartSection;

	UPROPERTY()
	bool bStopWhenAbilityEnds;

	UPROPERTY()
	float AnimRootMotionTranslationScale;


	bool StopPlayingMontage() const;

	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted) const;

	void OnAbilityCanceled() const;

	void OnMontageEnded(UAnimMontage* Montage,bool bInterrupted);

	void OnGameplayEventReceived(FGameplayTag EventTag , const FGameplayEventData* Payload) const;

	FOnMontageBlendingOutStarted MontageBlendingOutDelegate;
	FOnMontageEnded MontageEndedDelegate;
	FDelegateHandle MontageCancelledHandle;
	FDelegateHandle MontageEventHandle;
};
