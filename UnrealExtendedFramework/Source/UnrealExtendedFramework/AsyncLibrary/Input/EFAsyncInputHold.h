// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "UnrealExtendedFramework/AsyncLibrary/EFAsyncNodeTimer.h"
#include "EFAsyncInputHold.generated.h"



DECLARE_DYNAMIC_MULTICAST_DELEGATE(FButtonActionName);

/**
 * One button hold per Action Index, from Start Input Action to End Input Action: fires Hold Exceeded
 * once the button has been down for holdTime, then Hold (held that long) or Press (released sooner)
 * on release. Starting an index that is already running measures a new hold on the same node.
 * Cancelling it through the Async Task pin ends it without firing anything.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncInputHold : public UEFAsyncActionBase
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable,meta=(WorldContext="WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UEFAsyncInputHold* EFStartInputAction(const UObject* WorldContextObject, float holdTime = 1.f , int32 ActionIndex = 0);

	UFUNCTION(BlueprintCallable,meta=(WorldContext="WorldContextObject"))
	static void EFEndInputAction(const UObject* WorldContextObject, int32 ActionIndex = 0);

	UPROPERTY(BlueprintAssignable)
	FButtonActionName Press;
	UPROPERTY(BlueprintAssignable)
	FButtonActionName Hold;
	UPROPERTY(BlueprintAssignable)
	FButtonActionName HoldExceeded;

protected:

	FEFAsyncNodeTimer HoldTimer;
	int32 MappedActionIndex = 0;

	bool isHold = false;
	float HoldTime = 0;
	float TimePassed = 0;


	/** Starts (or restarts) measuring the hold. */
	void StartHold();
	void PassTime();
	/** The button was released: ends the node, then fires Hold or Press. */
	void ReleaseInput();

	virtual void ExecuteAction() override;
	/** Stops the timer and frees the Action Index for the next Start Input Action. */
	virtual void CancelWork() override;
	/** No failure pin: cancellation and failures end the node without firing anything. */
	virtual void BroadcastFailure(const FEFResult& /*Result*/) override {}
};
