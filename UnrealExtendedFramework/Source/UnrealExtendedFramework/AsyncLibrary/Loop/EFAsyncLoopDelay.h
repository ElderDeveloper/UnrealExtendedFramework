// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "UnrealExtendedFramework/AsyncLibrary/EFAsyncNodeTimer.h"
#include "EFAsyncLoopDelay.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLoopDelay);

/**
 * Fires Loop after Delay seconds, and keeps firing it every Delay seconds while shouldLoop is set,
 * until StopLoop or Cancel (Async Task pin) is called.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncLoopDelay : public UEFAsyncActionBase
{
	GENERATED_BODY()


public:

	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Extended Loop Delay", WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"), Category = "AsyncNode")
	static UEFAsyncLoopDelay* LoopDelay(const UObject* WorldContextObject ,bool shouldLoop = false, float Delay = 1.f);

	/** Ends the loop without firing anything; the same as Cancel. */
	UFUNCTION(BlueprintCallable)
	void StopLoop();

	UPROPERTY(BlueprintAssignable)
	FLoopDelay Loop;

protected:

	FEFAsyncNodeTimer LoopTimer;
	bool ShouldLoop = false;
	float LoopTime = 1.f;

	void TickLoop();

	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
	/** No failure pin: cancellation and failures end the node silently. */
	virtual void BroadcastFailure(const FEFResult& /*Result*/) override {}
};
