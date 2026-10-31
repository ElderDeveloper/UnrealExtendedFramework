// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "Shared/EEOSOperationTypes.h"
#include "EEOSAsyncActionBase.generated.h"

/** Fired when any EOS async action fails. Provides an error description. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncFailed, const FString&, Error);

/**
 * Base for EOS async nodes: UEFAsyncActionBase plus the EOS failure pin.
 *
 * Nodes start their subsystem call inside CallWithContext() and return early from their delegate
 * handlers unless IsResultForThisCall(). The EOS subsystems carry the call context to their
 * completion broadcasts, so two nodes making the same call at once each finish on their own
 * result. Cancellation (the node's Async Task pin), timeouts and a missing subsystem arrive on
 * OnFailure as a description of the foundation result.
 */
UCLASS(Abstract)
class EXTENDEDEOSBLUEPRINTS_API UEEOSAsyncActionBase : public UEFAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires if the action fails, could not be started, is cancelled or times out. */
	UPROPERTY(BlueprintAssignable)
	FOnAsyncFailed OnFailure;

protected:
	virtual FName GetResultSource() const override { return TEXT("EOS"); }

	/** Fires OnFailure with DescribeFailure(Result). */
	virtual void BroadcastFailure(const FEFResult& Result) override;

	/** Finishes the node and fires OnFailure with Error. Does nothing when the node already finished. */
	void FailWithError(const FString& Error);

	/** The OnFailure text for a foundation result: its message, or its full description when it has none. */
	static FString DescribeFailure(const FEFResult& Result);

	/**
	 * A detailed outcome for a node's own failure (cancelled, timed out, subsystem missing), for
	 * nodes that also fire an OnDetailedCompleted pin.
	 */
	static FEEOSOperationOutcome MakeFailureOutcome(FName Operation, const FEFResult& Result);
};
