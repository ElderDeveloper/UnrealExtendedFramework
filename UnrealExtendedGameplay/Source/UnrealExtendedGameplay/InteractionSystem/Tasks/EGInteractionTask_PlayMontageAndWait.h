// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"
#include "EGInteractionTask.h"

#include "EGInteractionTask_PlayMontageAndWait.generated.h"

class UAnimMontage;
class USkeletalMeshComponent;

/**
 * Plays a montage on the interactor and waits for it. Defaults to the first skeletal mesh on
 * the interactor; pass a mesh for first-person arms or a specific body.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGInteractionTask_PlayMontageAndWait : public UEGInteractionTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FEGInteractionTaskSimpleDelegate OnCompleted;

	UPROPERTY(BlueprintAssignable)
	FEGInteractionTaskSimpleDelegate OnBlendOut;

	UPROPERTY(BlueprintAssignable)
	FEGInteractionTaskSimpleDelegate OnInterrupted;

	UPROPERTY(BlueprintAssignable)
	FEGInteractionTaskSimpleDelegate OnCancelled;

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System|Tasks", meta = (HidePin = "OwningInteraction", DefaultToSelf = "OwningInteraction", BlueprintInternalUseOnly = "TRUE", DisplayName = "Play Montage And Wait"))
	static UEGInteractionTask_PlayMontageAndWait* PlayMontageAndWait(UEGInteraction* OwningInteraction, UAnimMontage* Montage, float PlayRate = 1.0f, FName StartSection = NAME_None, bool bStopWhenInteractionEnds = true, USkeletalMeshComponent* MeshOverride = nullptr);

	virtual void Activate() override;
	virtual void ExternalCancel() override;

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void OnMontageBlendingOut(UAnimMontage* InMontage, bool bInterrupted);
	void OnMontageEnded(UAnimMontage* InMontage, bool bInterrupted);
	UAnimInstance* GetAnimInstance() const;
	void StopMontage();

	UPROPERTY()
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Mesh;

	float PlayRate = 1.0f;
	FName StartSection;
	bool bStopWhenInteractionEnds = true;
	bool bPlayed = false;

	FOnMontageBlendingOutStarted BlendingOutDelegate;
	FOnMontageEnded MontageEndedDelegate;
};
