// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Engine/TimerHandle.h"
#include "EEOSMatchmakingSubsystem.generated.h"

class UEEOSSearchCoordinator;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSMatchFound, const FString&, SessionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSMatchmakingComplete, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEOSMatchmakingCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSMatchmakingStatusChanged, const FString&, StatusMessage);

/**
 * Search-based matchmaking through EOS sessions.
 *
 * Matchmaking is a filtered session search: StartMatchmaking looks for sessions advertising a
 * "MATCHMAKINGPOOL" attribute equal to the queue name, retrying up to MaxSearchAttempts times
 * (RetryDelaySeconds apart) until a match is found or attempts are exhausted. A found match is
 * surfaced via OnMatchFound and must be confirmed with AcceptMatch (joins the session) or
 * RejectMatch (excludes that session id for the rest of the cycle and re-queues).
 *
 * Hosts join a matchmaking pool by advertising it on their session: pass the pool name in
 * UEEOSSessionSubsystem::CreateSessionAdvanced's CustomSettings map under the key
 * "MATCHMAKINGPOOL", e.g. Settings.CustomSettings.Add(TEXT("MATCHMAKINGPOOL"), TEXT("Default")).
 *
 * Return-value contract: true means the request really took effect. false means it was
 * rejected or failed pre-flight — and NO delegate fires for a false return (rejections are
 * log-only; they never broadcast the cycle-terminal OnMatchmakingComplete while a cycle is
 * alive). The one exception is documented on StartMatchmaking.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSMatchmakingSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Start matchmaking with the given queue name.
	 *  @return false if rejected (a matchmaking cycle is already in flight — logged, and NO
	 *  delegate fires for this call) or failed pre-flight with no cycle in flight (EOS
	 *  unavailable / session interface missing — these DO broadcast
	 *  OnMatchmakingComplete(false) since no legitimate cycle waiter exists); true if a new
	 *  cycle started (it ends with exactly one OnMatchmakingComplete or
	 *  OnMatchmakingCancelled). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Matchmaking")
	bool StartMatchmaking(const FString& QueueName = TEXT("Default"));

	/** Start matchmaking with custom attributes for filtering.
	 *  @return same contract as StartMatchmaking. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Matchmaking")
	bool StartMatchmakingWithAttributes(const FString& QueueName, const TMap<FString, FString>& Attributes);

	/** Cancel the current matchmaking request.
	 *  @return false if there was nothing to cancel (logged; NO delegate fires); true if the
	 *  cycle was cancelled (OnMatchmakingCancelled fires exactly once). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Matchmaking")
	bool CancelMatchmaking();

	/** Accept a found match and join the session (as "GameSession").
	 *  Requires that no named "GameSession" already exists — destroy it first via
	 *  UEEOSSessionSubsystem::DestroySession. (Known limitation: matchmade joins and
	 *  Sessions.JoinSession share the single "GameSession" name the travel code expects,
	 *  so only one may hold it at a time.)
	 *  @return false if rejected (no match pending / a match join already in flight / a
	 *  "GameSession" already exists / EOS or interface unavailable) — logged, and NO delegate
	 *  fires, the cycle stays alive; true if the join started (the cycle then ends with
	 *  exactly one OnMatchmakingComplete). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Matchmaking")
	bool AcceptMatch();

	/** Reject a found match and re-queue.
	 *  @return false if rejected (no match pending / a match join already in flight / EOS
	 *  unavailable) — logged, NO delegate fires, the cycle state is untouched; true if the
	 *  match was rejected and the cycle re-queued. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Matchmaking")
	bool RejectMatch();

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

	/**
	 * Get the elapsed time (seconds) in the current matchmaking cycle, or -1 when not matchmaking.
	 * EOS session-search matchmaking has no backend wait estimate, so this reports elapsed time.
	 */
	UFUNCTION(BlueprintPure, Category = "EOS|Matchmaking")
	float GetEstimatedWaitTime() const;

	// ── Config ───────────────────────────────────────────────────────────────

	/** How many searches a matchmaking cycle runs before giving up (each RetryDelaySeconds apart) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Matchmaking", meta = (ClampMin = "1"))
	int32 MaxSearchAttempts = 5;

	/** Delay in seconds between search attempts (<= 0 retries immediately) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Matchmaking", meta = (ClampMin = "0.0"))
	float RetryDelaySeconds = 3.0f;

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

	/** Attributes the current cycle was started with (re-applied on every retry/re-queue search) */
	TMap<FString, FString> CurrentAttributes;

	/** Session ids rejected via RejectMatch this cycle (excluded from search results until the next StartMatchmaking) */
	TSet<FString> RejectedSessionIds;

	/** 1-based search attempt counter within the current cycle (reset by StartMatchmaking* and RejectMatch) */
	int32 CurrentSearchAttempt = 0;

	/** Index into CachedSearchSettings->SearchResults of the match offered via OnMatchFound (INDEX_NONE == no offer pending) */
	int32 SelectedResultIndex = INDEX_NONE;

	/** Pending retry between search attempts (cleared in CancelMatchmaking / Deinitialize) */
	FTimerHandle RetrySearchTimerHandle;

	/** Cached session search results from the last matchmaking attempt */
	TSharedPtr<class FOnlineSessionSearch> CachedSearchSettings;

	/** Delegate handle for session search completion (valid == a matchmaking search is bound) */
	FDelegateHandle MatchmakingCompleteDelegateHandle;

	/** Delegate handle for join session completion in AcceptMatch (valid == a match join is in flight) */
	FDelegateHandle JoinSessionDelegateHandle;

	/** Session name the pending AcceptMatch join was started with (filters the interface-wide join delegate) */
	FName PendingAcceptSessionName;

	/** Session id of the match being joined by the pending AcceptMatch (for logging/errors) */
	FString PendingAcceptSessionId;

	// ── Search coordination ──────────────────────────────────────────────────

	/** The shared search coordinator (may be null during GameInstance teardown). */
	UEEOSSearchCoordinator* GetSearchCoordinator() const;
	/** Acquire the cross-subsystem search slot; false while any session/lobby search is in flight. */
	bool TryAcquireSearchSlot();
	/** Release the search slot if this subsystem holds it (safe to call on every terminal path). */
	void ReleaseSearchSlot();

	/**
	 * Issue one FindSessions attempt using CurrentQueueName/CurrentAttributes. Called by
	 * StartMatchmakingWithAttributes, RejectMatch (re-queue) and the retry timer; no-ops if
	 * matchmaking was cancelled in the meantime. An attempt that can't start (search slot
	 * busy, synchronous FindSessions failure) counts as a failed attempt and feeds the
	 * retry loop.
	 */
	void IssueMatchmakingSearch();

	/**
	 * One search attempt ended without an acceptable match (failed, empty, all results
	 * rejected, or the attempt couldn't start). Retries after RetryDelaySeconds while
	 * attempts remain; otherwise ends the cycle with exactly one
	 * OnMatchmakingComplete(false) broadcast.
	 */
	void RetryOrEndCycle(const FString& AttemptOutcome);

	/**
	 * Called when ANY FindSessions completes. The delegate carries no search identity, but it
	 * is only bound while OUR search is in flight and the search coordinator guarantees no
	 * sibling subsystem search overlaps ours — so any trigger is our search's terminal event.
	 * (Do NOT gate on CachedSearchSettings->SearchState: the engine's zero-result path — the
	 * most common matchmaking outcome — fires the delegate without ever setting it.)
	 */
	void HandleFindSessionsComplete(bool bWasSuccessful);

	/** Called when ANY JoinSession completes; filtered on PendingAcceptSessionName */
	void HandleAcceptMatchJoinComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result);
};
