// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSMetricsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSPlayerSessionStarted, const FString&, SessionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSPlayerSessionEnded, const FString&, SessionId);

/**
 * Tracks player session metrics for EOS analytics.
 * BeginPlayerSession / EndPlayerSession report play sessions
 * to your EOS Dashboard metrics.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSMetricsSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Session Tracking ─────────────────────────────────────────────────────

	/** Begin tracking a player session (call when joining a match) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Metrics")
	void BeginPlayerSession(const FString& GameSessionId, const FString& ServerIp = TEXT(""), const FString& GameMode = TEXT(""));

	/** End the current player session (call when leaving a match) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Metrics")
	void EndPlayerSession();

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Check if a player session is currently active */
	UFUNCTION(BlueprintPure, Category = "EOS|Metrics")
	bool IsSessionActive() const;

	/** Get the current session ID */
	UFUNCTION(BlueprintPure, Category = "EOS|Metrics")
	FString GetCurrentSessionId() const;

	/** Get session duration in seconds */
	UFUNCTION(BlueprintPure, Category = "EOS|Metrics")
	float GetSessionDuration() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|Metrics")
	FOnEOSPlayerSessionStarted OnPlayerSessionStarted;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Metrics")
	FOnEOSPlayerSessionEnded OnPlayerSessionEnded;

private:

	FString ActiveSessionId;
	FString ActiveGameMode;
	FString ActiveServerIp;
	bool bSessionActive = false;
	FDateTime SessionStartTime;

	// ── Identity used at BeginPlayerSession — reused verbatim at EndPlayerSession ────────
	// The backend matches End to Begin by account id; re-deriving it at end time orphans the
	// backend session if the login state changed mid-session. Cleared when the session ends.

	/** True only if EOS_Metrics_BeginPlayerSession succeeded — the SDK End call is skipped otherwise */
	bool bSessionBeganViaSDK = false;

	/** True if the session was opened with an Epic account id, false for an external id */
	bool bSessionUsedEpicAccount = false;

	/** The exact account id string used at Begin: the Epic Account ID, or the external id (PUID or "local") */
	FString SessionAccountId;
};
