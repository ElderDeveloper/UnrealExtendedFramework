// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "Curves/CurveFloat.h"
#include "UnrealExtendedFramework/AsyncLibrary/EFAsyncNodeTimer.h"
#include "UnrealExtendedFramework/Data/EFAsyncTimelineData.h"
#include "EFAsyncFloatTimeline.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTimelineFloatLoop,float,Alpha);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTimelineFloatLoopComplete);


/**
 * Plays FloatCurve, firing Loop with the curve's value every PassTime seconds, then Completed when
 * it reaches the curve's end (its start when reversed). Timelines are named: starting one with a
 * name already playing replaces it, and Extended Float Timeline Manipulate steers or stops it.
 * Stopping, replacing or cancelling it (Async Task pin) ends it without firing Completed.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncFloatTimeline : public UEFAsyncActionBase
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

	/** Clears the timer and forgets this timeline's name. */
	void StopTimeline();

	/** Reached the end: stops and fires Completed. */
	void FinishTimeline();

	FName ExtendedTimelineName;
	float LoopTime = 0.1f;
	float TimePassed = 0.f;
	float CurveLastTime = 0;
	float CurveFirstTime = 0;
	EFTimelinePlaySide TimelinePlaySide = Ext_Forward;
	bool bTickWhenPaused = false;

	FEFAsyncNodeTimer TickTimer;

public:

	UPROPERTY(BlueprintAssignable)
	FTimelineFloatLoop Loop;

	UPROPERTY(BlueprintAssignable)
	FTimelineFloatLoopComplete Completed;

	UPROPERTY()
	UCurveFloat* SelectedFloatCurve = nullptr;




	/**
	* InternalUseOnly to hide sync version in BPs
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Extended Float Timeline", WorldContext = "WorldContextObject" , BlueprintInternalUseOnly = "true"), Category = "AsyncNode")
	static UEFAsyncFloatTimeline* ExtendedFloatTimeline(const FName TimelineName , UCurveFloat* FloatCurve , const UObject* WorldContextObject  , float PassTime = 0.005 , TEnumAsByte<EFTimelinePlayType> TimelinePlayType = Ext_Play , bool bTickWhenPaused = false );

	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Extended Float Timeline Manipulate", WorldContext = "WorldContextObject"), Category = "AsyncNode")
	static void ExtendedFloatTimelineManipulate(const FName TimelineName, const UObject* WorldContextObject , TEnumAsByte<EFTimelinePlayType> TimelinePlayType = Ext_Play);

	/** Restarts the tick timer. */
	void Reset();

protected:

	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
	/** No failure pin: cancellation and failures end the timeline without firing Completed. */
	virtual void BroadcastFailure(const FEFResult& /*Result*/) override {}
};
