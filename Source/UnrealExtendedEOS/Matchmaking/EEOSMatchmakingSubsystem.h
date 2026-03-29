// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSMatchmakingSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSMatchFound, const FString&, SessionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSMatchmakingComplete, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEOSMatchmakingCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSMatchmakingStatusChanged, const FString&, StatusMessage);

/**
 * Manages queue-based matchmaking through EOS.
 * Supports custom queue names, skill-based matching, and status tracking.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSMatchmakingSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Start matchmaking with the given queue name */
	UFUNCTION(BlueprintCallable, Category = "EOS|Matchmaking")
	void StartMatchmaking(const FString& QueueName = TEXT("Default"));

	/** Start matchmaking with custom attributes for filtering */
	UFUNCTION(BlueprintCallable, Category = "EOS|Matchmaking")
	void StartMatchmakingWithAttributes(const FString& QueueName, const TMap<FString, FString>& Attributes);

	/** Cancel the current matchmaking request */
	UFUNCTION(BlueprintCallable, Category = "EOS|Matchmaking")
	void CancelMatchmaking();

	/** Accept a found match and join the session */
	UFUNCTION(BlueprintCallable, Category = "EOS|Matchmaking")
	void AcceptMatch();

	/** Reject a found match and re-queue */
	UFUNCTION(BlueprintCallable, Category = "EOS|Matchmaking")
	void RejectMatch();

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Check if currently matchmaking */
	UFUNCTION(BlueprintPure, Category = "EOS|Matchmaking")
	bool IsMatchmaking() const;

	/** Get the current queue name */
	UFUNCTION(BlueprintPure, Category = "EOS|Matchmaking")
	FString GetCurrentQueueName() const;

	/** Get the elapsed matchmaking time in seconds */
	UFUNCTION(BlueprintPure, Category = "EOS|Matchmaking")
	float GetMatchmakingElapsedTime() const;

	/** Get estimated wait time in seconds (if available, -1 if unknown) */
	UFUNCTION(BlueprintPure, Category = "EOS|Matchmaking")
	float GetEstimatedWaitTime() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|Matchmaking")
	FOnEOSMatchFound OnMatchFound;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Matchmaking")
	FOnEOSMatchmakingComplete OnMatchmakingComplete;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Matchmaking")
	FOnEOSMatchmakingCancelled OnMatchmakingCancelled;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Matchmaking")
	FOnEOSMatchmakingStatusChanged OnMatchmakingStatusChanged;

private:

	bool bIsMatchmaking = false;
	FString CurrentQueueName;
	double MatchmakingStartTime = 0.0;
	float CachedEstimatedWait = -1.f;

	/** Cached session search results from the last matchmaking attempt */
	TSharedPtr<class FOnlineSessionSearch> CachedSearchSettings;

	/** Delegate handle for session search completion */
	FDelegateHandle MatchmakingCompleteDelegateHandle;

	/** Delegate handle for join session completion in AcceptMatch */
	FDelegateHandle JoinSessionDelegateHandle;

	/** Called when FindSessions completes */
	void HandleFindSessionsComplete(bool bWasSuccessful);
};

