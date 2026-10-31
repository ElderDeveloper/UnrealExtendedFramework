// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "UnrealExtendedFramework/AsyncLibrary/EFAsyncNodeTimer.h"
#include "EFAsyncLerp.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FExtendedLerp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLerpOutputPin , float , Alpha);

/**
 * Fires Loop with an alpha going from 0 to 1 (1 to 0 when Reversed) over Duration seconds of game
 * time, then Finished. Cancelling it through the Async Task pin stops the updates; Finished does not
 * fire then.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncLerp : public UEFAsyncActionBase
{
	GENERATED_BODY()


public:

	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Extended Lerp", WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"), Category = "AsyncNode")
	static UEFAsyncLerp* AsyncLerp(const UObject* WorldContextObject , float Duration = 1.f ,  bool Reversed = false , bool EaseInEaseOut = false);

	UPROPERTY(BlueprintAssignable)
	FLerpOutputPin Loop;

	UPROPERTY(BlueprintAssignable)
	FExtendedLerp Finished;

protected:

	bool Reversed = false;
	bool EaseInEaseOut = false;

	double CreatedGameTime = 0.0;
	double TargetGameTime = 0.0;
	bool FinalLoop = false;

	FEFAsyncNodeTimer LerpTimer;

	float LerpDuration = 1.f;
	float LerpAlpha = 0.f;

	UFUNCTION()
	void Timer();

	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
	/** No failure pin: cancellation and failures end the node without firing Finished. */
	virtual void BroadcastFailure(const FEFResult& /*Result*/) override {}

};
