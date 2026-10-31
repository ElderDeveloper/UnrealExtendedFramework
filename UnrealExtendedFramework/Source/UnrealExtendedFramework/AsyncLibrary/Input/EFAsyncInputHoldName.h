// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "UnrealExtendedFramework/AsyncLibrary/EFAsyncNodeTimer.h"
#include "EFAsyncInputHoldName.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FButtonAction);

/**
 * One button hold per Action Name, from Start Input Action Name to End Input Action Name: fires Hold
 * Exceeded once the button has been down for holdTime, then Hold (held that long) or Press (released
 * sooner) on release. Starting a name that is already running measures a new hold on the same node.
 * Cancelling it through the Async Task pin ends it without firing anything.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncInputHoldName : public UEFAsyncActionBase
{
	GENERATED_BODY()



public:

	UFUNCTION(BlueprintCallable,meta=(WorldContext="WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UEFAsyncInputHoldName* EFStartInputActionName(const UObject* WorldContextObject, FName ActionName , float holdTime = 1.f);

	UFUNCTION(BlueprintCallable,meta=(WorldContext="WorldContextObject"))
	static void EFEndInputActionName(const UObject* WorldContextObject, FName ActionName);

	UPROPERTY(BlueprintAssignable)
	FButtonAction Press;
	UPROPERTY(BlueprintAssignable)
	FButtonAction Hold;
	UPROPERTY(BlueprintAssignable)
	FButtonAction HoldExceeded;

protected:

	FEFAsyncNodeTimer HoldTimer;
	FName MappedActionName;

	bool isHold = false;
	float HoldTime = 0;
	float TimePassed = 0;


	/** Starts (or restarts) measuring the hold. */
	void StartHold();
	void PassTime();
	/** The button was released: ends the node, then fires Hold or Press. */
	void ReleaseInput();

	virtual void ExecuteAction() override;
	/** Stops the timer and frees the Action Name for the next Start Input Action Name. */
	virtual void CancelWork() override;
	/** No failure pin: cancellation and failures end the node without firing anything. */
	virtual void BroadcastFailure(const FEFResult& /*Result*/) override {}
};
