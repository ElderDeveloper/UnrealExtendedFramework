// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFConnectionMonitor.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFConnectionLost);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFConnectionRestored);

/**
 * Monitors PlayFab connectivity by periodically pinging the server.
 * Fires events when connection is lost or restored — useful for showing
 * "connection lost" UI overlays and pausing backend operations.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFConnectionMonitor : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Start monitoring connectivity (pings every IntervalSeconds) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Connection")
	void StartMonitoring(float IntervalSeconds = 30.0f);

	/** Stop monitoring */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Connection")
	void StopMonitoring();

	/** Manually ping the PlayFab server to check connectivity */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Connection")
	void Ping();

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Is connectivity monitoring active? */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Connection")
	bool IsMonitoring() const;

	/** Is PlayFab currently reachable? */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Connection")
	bool IsConnected() const;

	/** Get the time of the last successful ping */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Connection")
	float GetLastPingTime() const;

	/** Get consecutive failed ping count */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Connection")
	int32 GetConsecutiveFailures() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	/** Fired when connectivity is lost (after FailureThreshold consecutive failures) */
	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Connection")
	FOnEPFConnectionLost OnConnectionLost;

	/** Fired when connectivity is restored after being lost */
	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Connection")
	FOnEPFConnectionRestored OnConnectionRestored;

private:

	bool bIsConnected = true;
	bool bIsMonitoring = false;
	int32 ConsecutiveFailures = 0;
	float LastPingTime = 0.0f;
	FTimerHandle PingTimerHandle;

	/** Number of consecutive failures before declaring connection lost */
	static constexpr int32 FailureThreshold = 2;

	void PerformPing();
	void HandlePingResponse(bool bSuccess, TSharedPtr<FJsonObject> Response);
};
