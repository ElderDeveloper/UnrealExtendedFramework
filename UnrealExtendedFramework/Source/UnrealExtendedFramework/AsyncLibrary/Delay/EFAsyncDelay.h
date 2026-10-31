// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "UnrealExtendedFramework/AsyncLibrary/EFAsyncNodeTimer.h"
#include "EFAsyncDelay.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FExtendedDelay);


/**
 * Waits Delay seconds of game time, then fires Finished once. A delay of zero or less finishes on the
 * next tick. Cancelling it through the Async Task pin stops the wait; Finished does not fire then.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncDelay : public UEFAsyncActionBase
{
	GENERATED_BODY()


public:

	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Extended Delay", WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"), Category = "AsyncNode")
	static UEFAsyncDelay* EFAsyncDelay(const UObject* WorldContextObject , float Delay = 1.f);

	UPROPERTY(BlueprintAssignable)
	FExtendedDelay Finished;

protected:

	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
	/** No failure pin: cancellation and failures end the node without firing Finished. */
	virtual void BroadcastFailure(const FEFResult& /*Result*/) override {}

	void FinishDelay();

	FEFAsyncNodeTimer DelayTimer;
	float DelayTime = 1.f;
};
