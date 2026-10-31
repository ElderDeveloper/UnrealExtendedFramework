// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "UnrealExtendedFramework/AsyncLibrary/EFAsyncNodeTimer.h"
#include "EFAsyncForEachLoopDelay.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArrayOutputPin,int32,ArrayIndex);

/**
 * Fires Loop once per element of TargetArray, Delay seconds apart, then Completed with the last
 * index (-1 for an empty array). Cancelling it through the Async Task pin stops the loop; Completed
 * does not fire then.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncForEachLoopDelay : public UEFAsyncActionBase
{
	GENERATED_BODY()

public:

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

	UPROPERTY()
	TArray<UObject*> Array;
	bool ExecuteFirstWithoutDelay = false;

public:

	UPROPERTY(BlueprintAssignable)
	FArrayOutputPin Loop;

	UPROPERTY(BlueprintAssignable)
	FArrayOutputPin Completed;



	/**
	* InternalUseOnly to hide sync version in BPs
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Extended Framework For Each Loop Delay", WorldContext = "WorldContextObject" , BlueprintInternalUseOnly = "true"), Category = "AsyncNode")
	static UEFAsyncForEachLoopDelay* ForEachLoopDelayObject(const TArray<UObject*>& TargetArray ,const UObject* WorldContextObject , float Delay = 0.1 , bool bExecuteFirstWithoutDelay = false);

protected:

	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
	/** No failure pin: cancellation and failures end the node without firing Completed. */
	virtual void BroadcastFailure(const FEFResult& /*Result*/) override {}

	FEFAsyncNodeTimer LoopTimer;
};
