// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGInteractionTask.h"

#include "EGInteractionTask_WaitHold.generated.h"

/**
 * Accumulates time while the interaction is active. Fires OnProgress every tick, OnCompleted
 * when Duration elapses, and OnCancelled when the input is released first (or Cancel is called).
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGInteractionTask_WaitHold : public UEGInteractionTask
{
	GENERATED_BODY()

public:
	UEGInteractionTask_WaitHold(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable)
	FEGInteractionTaskProgressDelegate OnProgress;

	UPROPERTY(BlueprintAssignable)
	FEGInteractionTaskSimpleDelegate OnCompleted;

	UPROPERTY(BlueprintAssignable)
	FEGInteractionTaskSimpleDelegate OnCancelled;

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System|Tasks", meta = (HidePin = "OwningInteraction", DefaultToSelf = "OwningInteraction", BlueprintInternalUseOnly = "TRUE", DisplayName = "Wait Hold"))
	static UEGInteractionTask_WaitHold* WaitHold(UEGInteraction* OwningInteraction, float Duration, bool bCancelOnInputRelease = true);

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System|Tasks")
	void Cancel();

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System|Tasks")
	float GetProgress() const;

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void ExternalCancel() override;

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void HandleInputReleased();

	float Duration = 1.0f;
	float Elapsed = 0.0f;
	bool bCancelOnInputRelease = true;
	FDelegateHandle ReleaseHandle;
};
