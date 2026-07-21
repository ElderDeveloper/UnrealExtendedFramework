// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/TimerHandle.h"
#include "ESteamAsyncActionBase.generated.h"

/**
 * Shared base for the plugin's Blueprint async-action nodes. Centralizes the world-context handle,
 * the single-shot completion guard, and a MANDATORY safety timeout so a node can never leak while
 * waiting on a subsystem delegate that never fires (previously each node armed a timer only when
 * Timeout > 0, so a caller passing 0 could hang the node for the GameInstance lifetime).
 *
 * Derived-node contract:
 *  - the static factory sets WorldContextObject and Timeout;
 *  - Activate() subscribes to the subsystem delegate, then calls ArmTimeout(Timeout);
 *  - each Complete(...) overload starts with `if (!BeginComplete()) return;`, then unsubscribes,
 *    broadcasts its success/failure pin, and calls SetReadyToDestroy();
 *  - OnTimeoutFailure() is overridden to call Complete(false, <default payload>).
 */
UCLASS(Abstract)
class USteamAsyncActionBase : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

protected:
	/**
	 * Arms the safety timeout. A TimeoutSeconds <= 0 falls back to DefaultTimeoutCapSeconds rather
	 * than arming nothing, so a node whose completion delegate never fires still tears down.
	 */
	void ArmTimeout(float TimeoutSeconds);

	/**
	 * Single-shot completion guard: returns true exactly once (clearing the timeout timer), and false
	 * on every later call. Call it first in each Complete(); bail out when it returns false.
	 */
	bool BeginComplete();

	/** Resolves the world from the stored context object (for timer management). */
	UWorld* GetWorldSafe() const;

	/** Broadcast the failure pin with default payload and SetReadyToDestroy(). Invoked on timeout. */
	virtual void OnTimeoutFailure() {}

	/** Default safety cap (seconds) applied when a caller passes Timeout <= 0. */
	static constexpr float DefaultTimeoutCapSeconds = 30.0f;

	UPROPERTY()
	TObjectPtr<UObject> WorldContextObject;

	float Timeout = 10.0f;
	FTimerHandle TimeoutHandle;
	bool bCompleted = false;

private:
	UFUNCTION()
	void HandleTimeoutInternal();
};
