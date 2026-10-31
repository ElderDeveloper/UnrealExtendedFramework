// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "ESteamAsyncActionBase.generated.h"

/**
 * Shared base for the plugin's Blueprint async-action nodes, built on UEFAsyncActionBase (which
 * keeps the node alive, finishes it exactly once, runs the timeout and handles cancellation through
 * the node's Async Task pin). On top of that it keeps the plugin's MANDATORY safety timeout: a node
 * can never leak while waiting on a subsystem delegate that never fires, even when the caller
 * passes Timeout <= 0.
 *
 * Derived-node contract:
 *  - the static factory stores the node's inputs, then calls InitAsyncAction(WorldContext, Timeout);
 *  - ExecuteAction() resolves the subsystem (RequireGameInstanceSubsystem), binds the subsystem
 *    delegate and starts the call inside CallWithContext(); when the call cannot be issued it
 *    calls Complete(false, <default payload>);
 *  - each delegate handler returns early unless IsResultForThisCall() (and, where the delegate
 *    carries one, unless the id matches the node's request), then calls Complete(...);
 *  - each Complete(...) calls CancelWork(), returns unless FinishAction(), then broadcasts its
 *    success/failure pin;
 *  - BroadcastFailure() fires the failure pin with the node's default payload (cancellation,
 *    timeout, missing subsystem), and CancelWork() removes the node's delegate bindings.
 */
UCLASS(Abstract)
class USteamAsyncActionBase : public UEFAsyncActionBase
{
	GENERATED_BODY()

protected:
	/**
	 * Call from the node's static factory instead of UEFAsyncActionBase::InitAsyncAction (which this
	 * hides). A TimeoutSeconds <= 0 falls back to DefaultTimeoutCapSeconds rather than disabling the
	 * timeout, so a node whose completion delegate never fires still tears down.
	 */
	void InitAsyncAction(UObject* WorldContext, float TimeoutSeconds);

	/** Source of the node's own Cancelled, TimedOut and NotAvailable results. */
	virtual FName GetResultSource() const override { return TEXT("Steam"); }

	/** Default safety cap (seconds) applied when a caller passes Timeout <= 0. */
	static constexpr float DefaultTimeoutCapSeconds = 30.0f;

	/** The context the node was created with, kept alive for the node's lifetime. */
	UPROPERTY()
	TObjectPtr<UObject> WorldContextObject;
};
