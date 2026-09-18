// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGInteractionTask.h"

#include "EGInteractionTask_WaitTargetInRange.generated.h"

/**
 * Watches the distance between the interactor and the target part it activated on. Fires
 * OnOutOfRange once and ends when the target moves too far or, optionally, becomes occluded.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGInteractionTask_WaitTargetInRange : public UEGInteractionTask
{
	GENERATED_BODY()

public:
	UEGInteractionTask_WaitTargetInRange(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable)
	FEGInteractionTaskSimpleDelegate OnOutOfRange;

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System|Tasks", meta = (HidePin = "OwningInteraction", DefaultToSelf = "OwningInteraction", BlueprintInternalUseOnly = "TRUE", DisplayName = "Wait Target In Range"))
	static UEGInteractionTask_WaitTargetInRange* WaitTargetInRange(UEGInteraction* OwningInteraction, float MaxDistance = 300.0f, bool bRequireLineOfSight = false, float CheckInterval = 0.1f);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

private:
	bool IsTargetInRange() const;

	float MaxDistance = 300.0f;
	float CheckInterval = 0.1f;
	float Accumulator = 0.0f;
	bool bRequireLineOfSight = false;
};
