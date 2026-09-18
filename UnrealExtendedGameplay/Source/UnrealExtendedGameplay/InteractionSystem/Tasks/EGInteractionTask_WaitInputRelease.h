// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGInteractionTask.h"

#include "EGInteractionTask_WaitInputRelease.generated.h"

/** Fires OnReleased when the slot that activated the interaction is released. */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGInteractionTask_WaitInputRelease : public UEGInteractionTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FEGInteractionTaskSimpleDelegate OnReleased;

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System|Tasks", meta = (HidePin = "OwningInteraction", DefaultToSelf = "OwningInteraction", BlueprintInternalUseOnly = "TRUE", DisplayName = "Wait Input Release"))
	static UEGInteractionTask_WaitInputRelease* WaitInputRelease(UEGInteraction* OwningInteraction);

	virtual void Activate() override;

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void HandleInputReleased();

	FDelegateHandle ReleaseHandle;
};
