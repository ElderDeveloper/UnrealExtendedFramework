// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "UnrealExtendedFramework/AsyncLibrary/EFAsyncNodeTimer.h"
#include "EFAsyncForLoopDelay.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FForLoopArrayDelegate,int32,ArrayIndex);

/**
 * Fires Loop for every index from FirstIndex up to (not including) LastIndex, Delay seconds apart,
 * then Completed with LastIndex. Cancelling it through the Async Task pin stops the loop; Completed
 * does not fire then.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncForLoopDelay : public UEFAsyncActionBase
{
	GENERATED_BODY()


	/**
	* Internal tick. Will also broadcast the Tick Execution pin in the editor
	*/
	UFUNCTION()
	void InternalTick();

	/**
	* Internal completed. Clears the timer and broadcasts the Completed Execution pin in the editor
	*/
	UFUNCTION()
	void InternalCompleted();

	float LoopTime = 0.1f;
	int32 LoopIndex = 0;

	int32 FirstArrayIndex = 0;
	int32 LastArrayIndex = 0;

	FEFAsyncNodeTimer LoopTimer;


public:


	UPROPERTY(BlueprintAssignable)
	FForLoopArrayDelegate Loop;

	UPROPERTY(BlueprintAssignable)
	FForLoopArrayDelegate Completed;


	/**
	* InternalUseOnly to hide sync version in BPs
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Extended For Loop With Delay", WorldContext = "WorldContextObject" , BlueprintInternalUseOnly = "true"), Category = "AsyncNode")
	static UEFAsyncForLoopDelay* EFForLoopWithDelay(const int32 FirstIndex , const int32 LastIndex ,const UObject* WorldContextObject , float Delay = 0.1);

protected:

	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
	/** No failure pin: cancellation and failures end the node without firing Completed. */
	virtual void BroadcastFailure(const FEFResult& /*Result*/) override {}
};
