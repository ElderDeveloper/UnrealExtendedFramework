// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "Curves/CurveVector.h"
#include "UnrealExtendedFramework/AsyncLibrary/EFAsyncNodeTimer.h"
#include "EFAsyncVectorTimeline.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTimelineVectorLoop,FVector,Vector);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTimelineVectorLoopComplete);


/**
 * Plays VectorCurve forward, firing Loop with the curve's value every PassTime seconds, then
 * Completed at the curve's end. Cancelling it through the Async Task pin ends it without firing
 * Completed.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncVectorTimeline : public UEFAsyncActionBase
{
	GENERATED_BODY()

	/**
* Internal tick. Will also broadcast the Tick Execution pin in the editor
*/
	UFUNCTION()
	void InternalTick();

	/**
	* Internal completed. Stops the timeline without broadcasting anything
	*/
	UFUNCTION()
	void InternalCompleted();

	float LoopTime = 0.1f;
	float TimePassed = 0.f;
	float CurveLastTime = 0;

	FEFAsyncNodeTimer TickTimer;

public:

	UPROPERTY(BlueprintAssignable)
	FTimelineVectorLoop Loop;

	UPROPERTY(BlueprintAssignable)
	FTimelineVectorLoopComplete Completed;

	UPROPERTY()
	UCurveVector* SelectedFloatCurve = nullptr;


	/**
	* InternalUseOnly to hide sync version in BPs
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Extended Vector Curve Timeline", WorldContext = "WorldContextObject" , BlueprintInternalUseOnly = "true"), Category = "AsyncNode")
	static UEFAsyncVectorTimeline* ExtendedFloatTimeline(UCurveVector* VectorCurve ,const UObject* WorldContextObject , float PassTime = 0.005 );

protected:

	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
	/** No failure pin: cancellation and failures end the timeline without firing Completed. */
	virtual void BroadcastFailure(const FEFResult& /*Result*/) override {}
};
