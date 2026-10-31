// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintAsyncActionBase.h"

#include "EGAsyncAction_PlayMontage.generated.h"

class UAnimMontage;
class UEGAnimationSet;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGPlayMontageAsyncDelegate, FName, NotifyName);

/**
 * Play Montage And Wait For Notify, for actors that are not driven by GAS: plays through
 * UEGMontageLibrary (so IEGMontagePlayerInterface is honoured) and reports blend out, completion,
 * interruption and the montage's Play Montage Notify events for this play only.
 *
 * The tag version resolves the actor's animation set and loads the montage asynchronously first.
 */
UCLASS(meta = (ExposedAsyncProxy = "AsyncTask"))
class UNREALEXTENDEDGAMEPLAY_API UEGAsyncAction_PlayMontage : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FEGPlayMontageAsyncDelegate OnCompleted;

	UPROPERTY(BlueprintAssignable)
	FEGPlayMontageAsyncDelegate OnBlendOut;

	UPROPERTY(BlueprintAssignable)
	FEGPlayMontageAsyncDelegate OnInterrupted;

	/** Nothing played: no montage, no mesh, or the montage failed to start. */
	UPROPERTY(BlueprintAssignable)
	FEGPlayMontageAsyncDelegate OnFailed;

	UPROPERTY(BlueprintAssignable)
	FEGPlayMontageAsyncDelegate OnNotifyBegin;

	UPROPERTY(BlueprintAssignable)
	FEGPlayMontageAsyncDelegate OnNotifyEnd;

	UFUNCTION(BlueprintCallable, Category = "Extended|Montage", meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "Actor", AdvancedDisplay = "StartSection,StartTime,MeshPurpose", DisplayName = "Play Montage And Wait For Notify"))
	static UEGAsyncAction_PlayMontage* PlayMontageAndWaitForNotify(AActor* Actor, UAnimMontage* Montage, float PlayRate = 1.0f, FName StartSection = NAME_None, float StartTime = 0.0f, FGameplayTag MeshPurpose = FGameplayTag());

	UFUNCTION(BlueprintCallable, Category = "Extended|Montage", meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "Actor", AdvancedDisplay = "VariantIndex,MeshPurpose,FallbackSet", DisplayName = "Play Animation And Wait For Notify"))
	static UEGAsyncAction_PlayMontage* PlayAnimationAndWaitForNotify(AActor* Actor, FGameplayTag AnimationTag, int32 VariantIndex = -1, FGameplayTag MeshPurpose = FGameplayTag(), UEGAnimationSet* FallbackSet = nullptr);

	/** Stops the montage (if it is still playing) and ends the node without further events. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Montage")
	void Cancel(float BlendOutTime = 0.25f);

	virtual void Activate() override;

private:
	void StartPlayback();
	void HandleLoaded();
	void Finish();

	UFUNCTION()
	void HandleNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

	UFUNCTION()
	void HandleNotifyEnd(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

	void HandleBlendingOut(UAnimMontage* InMontage, bool bInterrupted);
	void HandleEnded(UAnimMontage* InMontage, bool bInterrupted);

	/**
	 * Used instead of the per-montage delegates when the owner played the montage (GAS and the like
	 * bind those single-slot delegates themselves, and overwriting them would hang the ability).
	 */
	UFUNCTION()
	void HandleSharedBlendingOut(UAnimMontage* InMontage, bool bInterrupted);

	UFUNCTION()
	void HandleSharedEnded(UAnimMontage* InMontage, bool bInterrupted);

	/**
	 * Uninitializing the anim instance (owner destroyed, mesh or anim class changed) deletes the
	 * montage instances without their ended events; this is what finishes the node then.
	 */
	UFUNCTION()
	void HandleAllMontagesEnded();

	/** False while our montage instance is still the active one (the event is about another instance). */
	bool IsEventForOurInstance(const UAnimMontage* InMontage) const;

	TWeakObjectPtr<AActor> Actor;

	UPROPERTY()
	TObjectPtr<UAnimMontage> Montage;

	TSoftObjectPtr<UAnimMontage> PendingMontage;
	TSharedPtr<FStreamableHandle> LoadHandle;

	float PlayRate = 1.0f;
	FName StartSection;
	float StartTime = 0.0f;
	FGameplayTag MeshPurpose;

	TWeakObjectPtr<UAnimInstance> AnimInstance;
	int32 MontageInstanceId = INDEX_NONE;
	bool bUsesSharedDelegates = false;
	bool bInterruptedReported = false;
	bool bFinished = false;
};
