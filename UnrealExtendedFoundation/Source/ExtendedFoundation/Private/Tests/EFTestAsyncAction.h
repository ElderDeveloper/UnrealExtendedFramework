// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "EFTestAsyncAction.generated.h"

/** Records what UEFAsyncActionBase does, for the foundation's automation tests. Not for use in games. */
UCLASS(Transient, NotBlueprintable, HideDropdown)
class UEFTestAsyncAction : public UEFAsyncActionBase
{
	GENERATED_BODY()

public:
	void Init(float InTimeoutSeconds = 0.f) { InitAsyncAction(nullptr, InTimeoutSeconds); }
	void FailEarly(const FString& Problem) { FailInvalidArgument(Problem); }
	bool Finish() { return FinishAction(); }
	void RunInContext(TFunctionRef<void()> Call) { CallWithContext(Call); }
	bool AcceptsCurrentResult() const { return IsResultForThisCall(); }

	TFunction<void()> MakeTestCallback()
	{
		return MakeWeakCallback<>([this]() { ++CallbackRuns; });
	}

	int32 ExecuteCount = 0;
	int32 FailureCount = 0;
	int32 CancelWorkCount = 0;
	int32 CallbackRuns = 0;
	FEFResult LastFailure;

protected:
	virtual void ExecuteAction() override { ++ExecuteCount; }
	virtual void BroadcastFailure(const FEFResult& Result) override { ++FailureCount; LastFailure = Result; }
	virtual void CancelWork() override { ++CancelWorkCount; }
	virtual FName GetResultSource() const override { return TEXT("Test"); }
};
