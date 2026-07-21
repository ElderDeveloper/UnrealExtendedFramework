// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSMatchmakingSubsystem.h"
#include "Sessions/EEOSSearchCoordinator.h"
#include "UnrealExtendedEOS.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

/** Owner tag this subsystem uses with the shared UEEOSSearchCoordinator. */
static const FName MatchmakingSearchOwner(TEXT("EEOSMatchmakingSubsystem"));

void UEEOSMatchmakingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSMatchmakingSubsystem::Deinitialize()
{
	if (bIsMatchmaking)
	{
		CancelMatchmaking();
	}

	// Stop any pending retry so the timer can't restart a search on a dead subsystem
	// (CancelMatchmaking already clears it when matchmaking was active; this covers the rest).
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(RetrySearchTimerHandle);
	}

	// Clear any still-pending per-operation handles so late completions can't reach
	// a dead subsystem.
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			if (MatchmakingCompleteDelegateHandle.IsValid())	SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(MatchmakingCompleteDelegateHandle);
			if (JoinSessionDelegateHandle.IsValid())			SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
		}
	}
	MatchmakingCompleteDelegateHandle.Reset();
	JoinSessionDelegateHandle.Reset();
	PendingAcceptSessionName = NAME_None;
	PendingAcceptSessionId.Empty();

	// If a search of ours was still in flight, free the cross-subsystem search slot.
	ReleaseSearchSlot();

	bIsMatchmaking = false;
	CurrentQueueName.Empty();
	CurrentAttributes.Empty();
	RejectedSessionIds.Empty();
	CurrentSearchAttempt = 0;
	SelectedResultIndex = INDEX_NONE;
	Super::Deinitialize();
}

// ── Search coordination ──────────────────────────────────────────────────────

UEEOSSearchCoordinator* UEEOSMatchmakingSubsystem::GetSearchCoordinator() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UEEOSSearchCoordinator>() : nullptr;
}

bool UEEOSMatchmakingSubsystem::TryAcquireSearchSlot()
{
	UEEOSSearchCoordinator* Coordinator = GetSearchCoordinator();
	// No coordinator only happens during GameInstance teardown — nothing else can be
	// searching then, so proceed rather than deadlock.
	return Coordinator ? Coordinator->TryAcquire(MatchmakingSearchOwner) : true;
}

void UEEOSMatchmakingSubsystem::ReleaseSearchSlot()
{
	if (UEEOSSearchCoordinator* Coordinator = GetSearchCoordinator())
	{
		if (Coordinator->GetCurrentOwner() == MatchmakingSearchOwner)
		{
			Coordinator->Release(MatchmakingSearchOwner);
		}
	}
}

// ── Actions ──────────────────────────────────────────────────────────────────

bool UEEOSMatchmakingSubsystem::StartMatchmaking(const FString& QueueName)
{
	return StartMatchmakingWithAttributes(QueueName, TMap<FString, FString>());
}

bool UEEOSMatchmakingSubsystem::StartMatchmakingWithAttributes(const FString& QueueName, const TMap<FString, FString>& Attributes)
{
	// In-flight rejection comes FIRST and is log-only: broadcasting the cycle-terminal
	// OnMatchmakingComplete(false) here would be consumed by the live cycle's waiters as
	// their completion (terminal-semantics listeners would tear down a healthy cycle).
	if (bIsMatchmaking)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMatchmakingSubsystem::StartMatchmaking — Already matchmaking; rejecting new request for queue '%s' (no delegate will fire)"), *QueueName);
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("StartMatchmaking"));
		OnMatchmakingComplete.Broadcast(false, TEXT("EOS not available"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSMatchmakingSubsystem::StartMatchmaking — Session interface not available"));
		OnMatchmakingComplete.Broadcast(false, TEXT("Session interface not available"));
		return false;
	}

	// Begin a fresh matchmaking cycle: new start timestamp, empty reject-exclusion set,
	// full attempt budget.
	bIsMatchmaking = true;
	CurrentQueueName = QueueName;
	CurrentAttributes = Attributes;
	MatchmakingStartTime = FPlatformTime::Seconds();
	RejectedSessionIds.Empty();
	CurrentSearchAttempt = 0;
	SelectedResultIndex = INDEX_NONE;

	OnMatchmakingStatusChanged.Broadcast(FString::Printf(TEXT("Searching for match in queue '%s'..."), *QueueName));
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::StartMatchmaking — Queue='%s', Attributes=%d — Searching..."), *QueueName, Attributes.Num());

	IssueMatchmakingSearch();
	return true;
}

void UEEOSMatchmakingSubsystem::IssueMatchmakingSearch()
{
	// Runs inline (StartMatchmaking, RejectMatch re-queue) and from the retry timer. If
	// matchmaking was cancelled during the retry window, do nothing — CancelMatchmaking
	// already broadcast OnMatchmakingCancelled (exactly once).
	if (!bIsMatchmaking)
	{
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub ? EOSSub->GetSessionInterface() : nullptr;
	if (!SessionInterface.IsValid())
	{
		// The interface disappeared mid-cycle — a REAL terminal event for the live cycle,
		// ended with exactly one completion broadcast.
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSMatchmakingSubsystem::IssueMatchmakingSearch — Session interface not available"));
		bIsMatchmaking = false;
		CurrentQueueName.Empty();
		CurrentAttributes.Empty();
		CachedSearchSettings.Reset();
		SelectedResultIndex = INDEX_NONE;
		OnMatchmakingComplete.Broadcast(false, TEXT("Session interface not available"));
		return;
	}

	++CurrentSearchAttempt;
	SelectedResultIndex = INDEX_NONE;

	// The engine cannot run concurrent searches (see UEEOSSearchCoordinator). A sibling
	// search in flight (server browser, lobby list) is transient — count this as a failed
	// attempt and let the retry loop try again after the sibling finishes.
	if (!TryAcquireSearchSlot())
	{
		RetryOrEndCycle(TEXT("another session/lobby search is in flight"));
		return;
	}

	// EOS matchmaking uses session search with a "MATCHMAKINGPOOL" bucket attribute
	// Create a session search with the queue name as the bucket
	TSharedRef<FOnlineSessionSearch> SearchSettings = MakeShared<FOnlineSessionSearch>();
	// More than 1 result so reject-exclusion can still pick an alternative when the first
	// result is a session the player already rejected this cycle.
	SearchSettings->MaxSearchResults = 10;
	SearchSettings->bIsLanQuery = false;
	SearchSettings->TimeoutInSeconds = 60.0f;

	// Set the matchmaking bucket/pool (this is how EOS routes matchmaking). Hosts advertise
	// it via CreateSessionAdvanced's CustomSettings map (key "MATCHMAKINGPOOL").
	SearchSettings->QuerySettings.Set(TEXT("MATCHMAKINGPOOL"), CurrentQueueName, EOnlineComparisonOp::Equals);

	// Add custom attributes for filtering
	for (const auto& Attr : CurrentAttributes)
	{
		SearchSettings->QuerySettings.Set(FName(*Attr.Key), Attr.Value, EOnlineComparisonOp::Equals);
		UE_LOG(LogExtendedEOS, Log, TEXT("  Matchmaking Attribute: '%s' = '%s'"), *Attr.Key, *Attr.Value);
	}

	// Store the search ref for later use (accept/reject)
	CachedSearchSettings = SearchSettings;

	// Register the completion delegate
	MatchmakingCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UEEOSMatchmakingSubsystem::HandleFindSessionsComplete));

	// Start the session search (this is the EOS matchmaking call). A synchronous false
	// return means the engine fires NO delegate at all — clean up and feed the retry loop
	// here, or the cycle wedges forever with IsMatchmaking() stuck true.
	if (!SessionInterface->FindSessions(0, SearchSettings))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMatchmakingSubsystem::IssueMatchmakingSearch — FindSessions failed to start (synchronous failure; no delegate will fire from the engine)"));
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(MatchmakingCompleteDelegateHandle);
		MatchmakingCompleteDelegateHandle.Reset();
		CachedSearchSettings.Reset();
		ReleaseSearchSlot();
		RetryOrEndCycle(TEXT("FindSessions failed to start"));
		return;
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::IssueMatchmakingSearch — Queue='%s', attempt %d/%d"), *CurrentQueueName, CurrentSearchAttempt, MaxSearchAttempts);
}

void UEEOSMatchmakingSubsystem::RetryOrEndCycle(const FString& AttemptOutcome)
{
	// Retry after RetryDelaySeconds while attempts remain.
	if (CurrentSearchAttempt < MaxSearchAttempts)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem: No match on attempt %d/%d (%s) — retrying in %.1f seconds"),
			CurrentSearchAttempt, MaxSearchAttempts, *AttemptOutcome, RetryDelaySeconds);
		OnMatchmakingStatusChanged.Broadcast(FString::Printf(TEXT("No match yet (attempt %d/%d), retrying..."), CurrentSearchAttempt, MaxSearchAttempts));

		UGameInstance* GameInstance = GetGameInstance();
		if (GameInstance && RetryDelaySeconds > 0.f)
		{
			// CreateUObject won't fire on a stale subsystem; the handle is also cleared
			// explicitly in CancelMatchmaking and Deinitialize.
			GameInstance->GetTimerManager().SetTimer(RetrySearchTimerHandle,
				FTimerDelegate::CreateUObject(this, &UEEOSMatchmakingSubsystem::IssueMatchmakingSearch),
				RetryDelaySeconds, false);
		}
		else
		{
			IssueMatchmakingSearch();
		}
		return;
	}

	// Attempts exhausted — end the cycle with exactly one completion broadcast.
	bIsMatchmaking = false;
	CurrentQueueName.Empty();
	CurrentAttributes.Empty();
	CachedSearchSettings.Reset();
	SelectedResultIndex = INDEX_NONE;

	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMatchmakingSubsystem: Matchmaking failed — no match found after %d attempts (last attempt: %s)"),
		MaxSearchAttempts, *AttemptOutcome);
	OnMatchmakingComplete.Broadcast(false, TEXT("No match found"));
}

void UEEOSMatchmakingSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	// This handler is only bound while OUR search is in flight, and the search coordinator
	// guarantees no sibling subsystem (Sessions/Lobby) search overlaps ours — so ANY trigger
	// here is OUR search's terminal event. Do NOT gate on CachedSearchSettings->SearchState:
	// the engine's zero-result path — matchmaking's most common outcome (searching a pool
	// with no host yet) — fires this delegate without ever setting SearchState
	// (OnlineSessionEOS.cpp:2675-2679). The old "InProgress means not ours" check ignored
	// our own completion and wedged the cycle with IsMatchmaking() stuck true forever.
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(MatchmakingCompleteDelegateHandle);
		}
	}
	MatchmakingCompleteDelegateHandle.Reset();
	ReleaseSearchSlot();

	if (!bIsMatchmaking)
	{
		// Defensive: every path that ends the cycle clears the find handle first, so this
		// should be unreachable. Log-only — the cycle's terminal broadcast already fired,
		// and a second one here would violate exactly-once semantics.
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem: Search completed after the cycle ended; ignoring"));
		return;
	}

	// Read results from OUR search object; the trigger's payload carries success. Empty
	// results with bWasSuccessful == true is a successful (but matchless) search.
	const bool bSearchSucceeded = bWasSuccessful && CachedSearchSettings.IsValid();

	// Pick the first result the player hasn't rejected this cycle.
	int32 FoundIndex = INDEX_NONE;
	if (bSearchSucceeded)
	{
		for (int32 Index = 0; Index < CachedSearchSettings->SearchResults.Num(); ++Index)
		{
			if (!RejectedSessionIds.Contains(CachedSearchSettings->SearchResults[Index].Session.GetSessionIdStr()))
			{
				FoundIndex = Index;
				break;
			}
		}
	}

	if (FoundIndex != INDEX_NONE)
	{
		// Match found
		SelectedResultIndex = FoundIndex;
		const FString SessionId = CachedSearchSettings->SearchResults[FoundIndex].Session.GetSessionIdStr();

		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem: Match found — Session ID: %s"), *SessionId);
		OnMatchmakingStatusChanged.Broadcast(TEXT("Match found!"));
		OnMatchFound.Broadcast(SessionId);
		return;
	}

	// No acceptable match this attempt (search failed, empty, or every result was rejected).
	RetryOrEndCycle(bSearchSucceeded ? TEXT("no acceptable sessions") : TEXT("search failed"));
}

bool UEEOSMatchmakingSubsystem::CancelMatchmaking()
{
	if (!bIsMatchmaking)
	{
		// Nothing to cancel — log-only rejection. Broadcasting OnMatchmakingCancelled here
		// would fake a terminal event for a cycle that never existed.
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMatchmakingSubsystem::CancelMatchmaking — Not currently matchmaking (no delegate will fire)"));
		return false;
	}

	const float ElapsedTime = GetMatchmakingElapsedTime();

	// Stop a pending retry so the timer can't restart the search after cancellation. With the
	// timer cleared and the find-complete handle cleared below, no other path can broadcast
	// for this cycle — OnMatchmakingCancelled fires exactly once, here.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(RetrySearchTimerHandle);
	}

	// Cancel the ongoing session search — but ONLY when the current engine-side search is
	// OURS: CancelFindSessions is search-agnostic and would kill a sibling subsystem's
	// (server browser / lobby list) search. Our find handle being bound plus the coordinator
	// slot being ours is the proof of ownership.
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	if (EOSSub)
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			UEEOSSearchCoordinator* Coordinator = GetSearchCoordinator();
			const bool bOwnSearchInFlight = MatchmakingCompleteDelegateHandle.IsValid() &&
				(!Coordinator || Coordinator->GetCurrentOwner() == MatchmakingSearchOwner);

			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(MatchmakingCompleteDelegateHandle);
			if (bOwnSearchInFlight)
			{
				SessionInterface->CancelFindSessions();
			}
		}
	}
	MatchmakingCompleteDelegateHandle.Reset();
	ReleaseSearchSlot();

	bIsMatchmaking = false;
	CurrentQueueName.Empty();
	CurrentAttributes.Empty();
	RejectedSessionIds.Empty();
	CurrentSearchAttempt = 0;
	SelectedResultIndex = INDEX_NONE;
	CachedSearchSettings.Reset();

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::CancelMatchmaking — Cancelled after %.1f seconds"), ElapsedTime);
	OnMatchmakingCancelled.Broadcast();
	return true;
}

bool UEEOSMatchmakingSubsystem::AcceptMatch()
{
	// ALL Accept guards are non-terminal: the matchmaking cycle stays alive, so none of them
	// may broadcast the cycle-terminal OnMatchmakingComplete — log and return false instead
	// (a rejected call fires no delegate).
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("AcceptMatch"));
		return false;
	}

	// SelectedResultIndex is only valid while a found match is awaiting accept/reject.
	if (!CachedSearchSettings.IsValid() || !CachedSearchSettings->SearchResults.IsValidIndex(SelectedResultIndex))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMatchmakingSubsystem::AcceptMatch — No match available to accept (no delegate will fire)"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSMatchmakingSubsystem::AcceptMatch — Session interface not available (no delegate will fire)"));
		return false;
	}

	if (JoinSessionDelegateHandle.IsValid())
	{
		// A previous AcceptMatch join is still in flight; rebinding here would orphan its
		// completion.
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMatchmakingSubsystem::AcceptMatch — A match join is already in flight; rejecting duplicate call (no delegate will fire)"));
		return false;
	}

	// Matchmade joins use the "GameSession" name the travel/game code expects — the same
	// single name UEEOSSessionSubsystem uses for its creates/joins. The engine's synchronous
	// "session already exists" failure trigger would be consumed ambiguously by both
	// subsystems' name-filtered join handlers, so refuse up front while ANY "GameSession"
	// exists (created, joined, or a Sessions.JoinSession in flight — the engine registers
	// the named session synchronously at JoinSession time, so this check covers in-flight
	// joins too). Known limitation: only one subsystem can hold "GameSession" at a time.
	const FName GameSessionName(TEXT("GameSession"));
	if (SessionInterface->GetNamedSession(GameSessionName) != nullptr)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSMatchmakingSubsystem::AcceptMatch — A 'GameSession' already exists (or a join for it is in flight); destroy it first via UEEOSSessionSubsystem::DestroySession before accepting the match (no delegate will fire)"));
		return false;
	}

	// Join the found session (the reject-filtered pick, not necessarily index 0)
	PendingAcceptSessionId = CachedSearchSettings->SearchResults[SelectedResultIndex].Session.GetSessionIdStr();
	PendingAcceptSessionName = GameSessionName;

	JoinSessionDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UEEOSMatchmakingSubsystem::HandleAcceptMatchJoinComplete));

	// Engine-documented requirement (same class as the lobby join fix): bUsesPresence is
	// false by default in search results and must be set game-side before JoinSession, or
	// the joiner loses presence/invites. Matchmade sessions are game sessions, not lobbies —
	// force bUseLobbiesIfAvailable off. Modify a local copy so the cached results (still
	// needed for RejectMatch bookkeeping) stay pristine.
	FOnlineSessionSearchResult SearchResultCopy = CachedSearchSettings->SearchResults[SelectedResultIndex];
	SearchResultCopy.Session.SessionSettings.bUsesPresence = true;
	SearchResultCopy.Session.SessionSettings.bUseLobbiesIfAvailable = false;

	SessionInterface->JoinSession(0, PendingAcceptSessionName, SearchResultCopy);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::AcceptMatch — Joining session '%s'..."), *PendingAcceptSessionId);
	OnMatchmakingStatusChanged.Broadcast(TEXT("Match accepted, joining session..."));
	return true;
}

void UEEOSMatchmakingSubsystem::HandleAcceptMatchJoinComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result)
{
	// OnJoinSessionComplete is interface-wide: a lobby (or other) join completing also lands
	// here. Ignore completions that aren't our pending match join — without clearing the
	// handle or broadcasting.
	if (InSessionName != PendingAcceptSessionName)
	{
		return;
	}

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
		}
	}
	JoinSessionDelegateHandle.Reset();
	PendingAcceptSessionName = NAME_None;

	const FString SessionId = PendingAcceptSessionId;
	PendingAcceptSessionId.Empty();

	const bool bJoinSuccess = (Result == EOnJoinSessionCompleteResult::Success);

	// The matchmaking cycle ends with this join (success or not) — reset cycle state.
	bIsMatchmaking = false;
	CurrentQueueName.Empty();
	CurrentAttributes.Empty();
	RejectedSessionIds.Empty();
	CurrentSearchAttempt = 0;
	SelectedResultIndex = INDEX_NONE;
	CachedSearchSettings.Reset();

	if (bJoinSuccess)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::AcceptMatch — Joined session '%s' successfully"), *SessionId);
		OnMatchmakingStatusChanged.Broadcast(TEXT("Match accepted, joined session"));
		OnMatchmakingComplete.Broadcast(true, TEXT(""));
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMatchmakingSubsystem::AcceptMatch — Failed to join session '%s'"), *SessionId);
		OnMatchmakingComplete.Broadcast(false, TEXT("Failed to join session"));
	}
}

bool UEEOSMatchmakingSubsystem::RejectMatch()
{
	// ALL Reject guards are non-terminal: log and return false, never broadcast the
	// cycle-terminal OnMatchmakingComplete (see AcceptMatch).
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("RejectMatch"));
		return false;
	}

	if (JoinSessionDelegateHandle.IsValid())
	{
		// An AcceptMatch join is in flight; the match can no longer be rejected.
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMatchmakingSubsystem::RejectMatch — A match join is already in flight; rejecting call (no delegate will fire)"));
		return false;
	}

	// SelectedResultIndex is only valid while a found match is awaiting accept/reject.
	if (!CachedSearchSettings.IsValid() || !CachedSearchSettings->SearchResults.IsValidIndex(SelectedResultIndex))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMatchmakingSubsystem::RejectMatch — No match available to reject (no delegate will fire)"));
		return false;
	}

	// Exclude this session for the rest of the cycle so re-searches can't re-find it.
	const FString RejectedSessionId = CachedSearchSettings->SearchResults[SelectedResultIndex].Session.GetSessionIdStr();
	RejectedSessionIds.Add(RejectedSessionId);
	SelectedResultIndex = INDEX_NONE;
	CachedSearchSettings.Reset();

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::RejectMatch — Rejected session '%s', re-queuing"), *RejectedSessionId);

	// Re-queue directly, NOT via StartMatchmaking (which would clear RejectedSessionIds as a
	// fresh cycle). Keeps the queue name, attributes, start timestamp and reject-exclusion
	// set; grants a fresh search-attempt budget.
	CurrentSearchAttempt = 0;
	OnMatchmakingStatusChanged.Broadcast(TEXT("Match rejected, searching again..."));
	IssueMatchmakingSearch();
	return true;
}

bool UEEOSMatchmakingSubsystem::IsMatchmaking() const
{
	return bIsMatchmaking;
}

FString UEEOSMatchmakingSubsystem::GetCurrentQueueName() const
{
	return CurrentQueueName;
}

float UEEOSMatchmakingSubsystem::GetMatchmakingElapsedTime() const
{
	if (!bIsMatchmaking) return 0.f;
	return static_cast<float>(FPlatformTime::Seconds() - MatchmakingStartTime);
}

float UEEOSMatchmakingSubsystem::GetEstimatedWaitTime() const
{
	// EOS session-search matchmaking has no backend wait estimate — report elapsed time in
	// the current cycle instead, -1 when idle.
	if (!bIsMatchmaking) return -1.f;
	return static_cast<float>(FPlatformTime::Seconds() - MatchmakingStartTime);
}
