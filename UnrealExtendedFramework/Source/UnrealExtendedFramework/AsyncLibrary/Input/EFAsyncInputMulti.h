// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "UnrealExtendedFramework/AsyncLibrary/EFAsyncNodeTimer.h"
#include "EFAsyncInputMulti.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FButtonClickAction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FButtonEndAction,int32,ClickIndex);



/**
 * Counts clicks of one Action Name: each further call within clickWaitTime of the previous one fires
 * Double Click, then Triple Click, and restarts the wait. End fires with the click count once a wait
 * passes without another click, and also when the node is cancelled through the Async Task pin.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncInputMulti : public UEFAsyncActionBase
{
	GENERATED_BODY()


public:

	UFUNCTION(BlueprintCallable,meta=(WorldContext="WorldContextObject", BlueprintInternalUseOnly = "true") , Category = "AsyncNode")
	static UEFAsyncInputMulti* EFInputMultiClick(const UObject* WorldContextObject, FName ActionName , float clickWaitTime = 0.5f , int32 maxClickCount = 3);


	void BroadcastClickIndex();

	UPROPERTY(BlueprintAssignable)
	FButtonEndAction DoubleClick;

	UPROPERTY(BlueprintAssignable)
	FButtonEndAction TripleClick;

	UPROPERTY(BlueprintAssignable)
	FButtonEndAction End;


protected:

	FEFAsyncNodeTimer ClickTimer;


	FName ActionName;
	int32 ClickIndex = 0;
	int32 MaxClickCount = 3;
	float ClickWaitTime = 0;

	/** Starts (or restarts) the wait for the next click. */
	void StartClickWindow();

	UFUNCTION()
	void PassTime();

	virtual void ExecuteAction() override;
	/** Stops the wait and frees the Action Name for the next click sequence. */
	virtual void CancelWork() override;
	/** Cancellation and failures end the sequence early: End fires with the clicks counted so far. */
	virtual void BroadcastFailure(const FEFResult& Result) override;
};
