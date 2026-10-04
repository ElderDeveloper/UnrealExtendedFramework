// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSMatchmakingSubsystem.h"
#include "Shared/EEOSNativeOperation.h"
#include "Sessions/EEOSRetiredSessionOperation.h"
#include "Sessions/EEOSCapacity.h"
#include "Interfaces/OnlineIdentityInterface.h"
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
	BeginEOSShutdown(); bShuttingDown = true;
	FEEOSRetiredSessionOperation::Hold(JoinSessions, JoinLease, GetOwningEOSInstanceName(), PendingAcceptSessionName, FEEOSRetiredSessionOperation::EKind::Join, true);
	if (JoinSessions.IsValid()) JoinSessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
	if (SearchSessions.IsValid()) SearchSessions->ClearOnFindSessionsCompleteDelegate_Handle(MatchmakingCompleteDelegateHandle);
	JoinLease.Reset(); JoinSessions.Reset(); SearchSessions.Reset();
	if (UEEOSSearchCoordinator* Coordinator = GetSearchCoordinator()) Coordinator->Retire(MatchmakingSearchOwner);
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
	IOnlineSubsystem* OSS = GetEOSOnlineSubsystem();
	return Coordinator && OSS && Coordinator->TryAcquire(MatchmakingSearchOwner, OSS->GetSessionInterface(), this, TEXT("FindMatch"));
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
	if (bShuttingDown) return false;

	return StartMatchmakingWithAttributes(QueueName, TMap<FString, FString>());
}

bool UEEOSMatchmakingSubsystem::StartMatchmakingWithAttributes(const FString& QueueName, const TMap<FString, FString>& Attributes)
{
	if (bShuttingDown) return false;

	// In-flight rejection comes FIRST and is log-only: broadcasting the cycle-terminal
	// OnMatchmakingComplete(false) here would be consumed by the live cycle's waiters as
	// their completion (terminal-semantics listeners would tear down a healthy cycle).
	if (bIsMatchmaking)
	{
		RejectOperation(TEXT("Matchmaking"), EEOSOperationCode::Busy, TEXT("A matchmaking cycle is already pending."));
		UE_LOG(LogExtendedEOS, Verbose, TEXT("EEOSMatchmakingSubsystem::StartMatchmaking — Already matchmaking; rejecting new request for queue '%s' (no delegate will fire)"), *FEEOSNativeOperationLease::SafeField(QueueName));
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("StartMatchmaking"));
		const auto Outcome = CompleteCycle(false, EEOSOperationCode::UnsupportedCapability, TEXT("EOS not available"));
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnMatchmakingComplete.Broadcast(false, Outcome.Message); OnOperationCompleted.Broadcast(Outcome);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("StartMatchmaking"), TEXT("CapabilityUnavailable"));
		const auto Outcome = CompleteCycle(false, EEOSOperationCode::UnsupportedCapability, TEXT("Session interface not available"));
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnMatchmakingComplete.Broadcast(false, Outcome.Message); OnOperationCompleted.Broadcast(Outcome);
		return false;
	}

	// Begin a fresh matchmaking cycle: new start timestamp, empty reject-exclusion set,
	// full attempt budget.
	bIsMatchmaking = true; CycleContext = CaptureEOSContext();
	CycleRequestId = BeginOperation(TEXT("Matchmaking"));
	const int64 StartedCycle = CycleRequestId; OfferRequestId = 0;
	CurrentQueueName = QueueName;
	CurrentAttributes = Attributes;
	MatchmakingStartTime = FPlatformTime::Seconds();
	RejectedSessionIds.Empty();
	CurrentSearchAttempt = 0;
	SelectedResultIndex = INDEX_NONE;

	OnMatchmakingStatusChanged.Broadcast(FString::Printf(TEXT("Searching for match in queue '%s'..."), *QueueName));
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::StartMatchmaking — Queue='%s', Attributes=%d — Searching..."), *FEEOSNativeOperationLease::SafeField(QueueName), Attributes.Num());

	if (!bShuttingDown && bIsMatchmaking && CycleRequestId == StartedCycle) IssueMatchmakingSearch();
	return true;
}

void UEEOSMatchmakingSubsystem::IssueMatchmakingSearch()
{
	if (bShuttingDown) return;
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
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("IssueMatchmakingSearch"), TEXT("CapabilityUnavailable"));
		bIsMatchmaking = false;
		CurrentQueueName.Empty();
		CurrentAttributes.Empty();
		CachedSearchSettings.Reset();
		SelectedResultIndex = INDEX_NONE;
		const auto Outcome = CompleteCycle(false, EEOSOperationCode::UnsupportedCapability, TEXT("Session interface not available"));
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnMatchmakingComplete.Broadcast(false, Outcome.Message); OnOperationCompleted.Broadcast(Outcome);
		return;
	}

	if (!IsEOSContextCurrent(CycleContext))
	{
		const auto Outcome = CompleteCycle(false, EEOSOperationCode::Canceled, TEXT("Original matchmaking identity/platform retired."));
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnMatchmakingComplete.Broadcast(false, Outcome.Message); OnOperationCompleted.Broadcast(Outcome); return;
	}
	++CurrentSearchAttempt;
	SelectedResultIndex = INDEX_NONE;

	// The engine cannot run concurrent searches (see UEEOSSearchCoordinator). A sibling
	// search in flight (server browser, lobby list) is transient — count this as a failed
	// attempt and let the retry loop try again after the sibling finishes.
	if (!TryAcquireSearchSlot())
	{
		RejectOperation(TEXT("FindMatch"), EEOSOperationCode::Busy, TEXT("Another search owns the native interface."));
		RetryOrEndCycle(TEXT("SearchBusy"));
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
		UE_LOG(LogExtendedEOS, Verbose, TEXT("EOSMatchmaking Filter=%s ValueOmitted=1"), *FEEOSNativeOperationLease::SafeField(Attr.Key));
	}

	// Store the search ref for later use (accept/reject)
	CachedSearchSettings = SearchSettings; SearchSessions = SessionInterface;
	BeginOperation(TEXT("FindMatch"), FString(), GetSearchCoordinator()->GetRequestId()); TagOperationContext(TEXT("FindMatch"), FString(), 0, CurrentSearchAttempt, CycleRequestId);

	// Register the completion delegate
	MatchmakingCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UEEOSMatchmakingSubsystem::HandleFindSessionsComplete));

	// Start the session search (this is the EOS matchmaking call). A synchronous false
	// return means the engine fires NO delegate at all — clean up and feed the retry loop
	// here, or the cycle wedges forever with IsMatchmaking() stuck true.
	const FDelegateHandle SubmittedHandle = MatchmakingCompleteDelegateHandle;
	if (!SessionInterface->FindSessions(0, SearchSettings) && MatchmakingCompleteDelegateHandle == SubmittedHandle)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMatchmakingSubsystem::IssueMatchmakingSearch — FindSessions failed to start (synchronous failure; no delegate will fire from the engine)"));
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(MatchmakingCompleteDelegateHandle);
		MatchmakingCompleteDelegateHandle.Reset();
		CachedSearchSettings.Reset();
		ReleaseSearchSlot();
		const auto Outcome = CompleteOperation(TEXT("FindMatch"), false, EEOSOperationCode::NativeStartRejected, TEXT("Native matchmaking search refused submission."));
		OnOperationCompleted.Broadcast(Outcome);
		if (bIsMatchmaking && CycleRequestId == Outcome.ParentRequestId) RetryOrEndCycle(TEXT("FindSessions failed to start"));
		return;
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::IssueMatchmakingSearch — Queue='%s', attempt %d/%d"), *FEEOSNativeOperationLease::SafeField(CurrentQueueName), CurrentSearchAttempt, MaxSearchAttempts);
}

void UEEOSMatchmakingSubsystem::RetryOrEndCycle(const FString& AttemptOutcome)
{
	const int64 RetryingCycle = CycleRequestId;
	if (!bIsMatchmaking || !RetryingCycle) return;
	// Retry after RetryDelaySeconds while attempts remain.
	if (CurrentSearchAttempt < MaxSearchAttempts)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem: No match on attempt %d/%d (%s) — retrying in %.1f seconds"),
			CurrentSearchAttempt, MaxSearchAttempts, *FEEOSNativeOperationLease::SafeField(AttemptOutcome), RetryDelaySeconds);
		OnMatchmakingStatusChanged.Broadcast(FString::Printf(TEXT("No match yet (attempt %d/%d), retrying..."), CurrentSearchAttempt, MaxSearchAttempts));

		if (!bIsMatchmaking || bShuttingDown || CycleRequestId != RetryingCycle) return;
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
		MaxSearchAttempts, *FEEOSNativeOperationLease::SafeField(AttemptOutcome));
	const auto Outcome = CompleteCycle(false, EEOSOperationCode::NativeFailure, TEXT("No match found within the configured attempt budget."));
	FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
	OnMatchmakingComplete.Broadcast(false, Outcome.Message); OnOperationCompleted.Broadcast(Outcome);
}

void UEEOSMatchmakingSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (!MatchmakingCompleteDelegateHandle.IsValid() || bShuttingDown)
	{ LogCallbackDisposition(TEXT("FindMatch"), CycleRequestId, bShuttingDown ? TEXT("ShutdownInternalOnly") : TEXT("Duplicate"), CycleContext.Generation); return; }
	LogCallbackDisposition(TEXT("FindMatch"), CycleRequestId, IsEOSContextCurrent(CycleContext) ? TEXT("Consumed") : TEXT("StaleGeneration"), CycleContext.Generation);
	if (SearchSessions.IsValid()) SearchSessions->ClearOnFindSessionsCompleteDelegate_Handle(MatchmakingCompleteDelegateHandle);
	SearchSessions.Reset();
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
	const int64 CompletingCycle = CycleRequestId;
	const bool bContextCurrent = IsEOSContextCurrent(CycleContext);
	const int32 NativeSeen = CachedSearchSettings.IsValid() ? CachedSearchSettings->SearchResults.Num() : 0;
	int32 Valid = 0, InvalidDetails = 0, Rejected = 0;
	if (CachedSearchSettings.IsValid()) for (const auto& Entry : CachedSearchSettings->SearchResults)
	{
		if (!Entry.IsValid() || !Entry.Session.SessionInfo.IsValid() || !Entry.Session.SessionInfo->IsValid()) ++InvalidDetails;
		else if (RejectedSessionIds.Contains(Entry.GetSessionIdStr())) ++Rejected;
		else ++Valid;
	}
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSMatchSearch ParentRequest=%lld NativeSeen=%d ValidCandidates=%d DroppedInvalidTargetOrDetails=%d RejectedByCaller=%d HiddenNativeResultsKnown=0"), CompletingCycle, NativeSeen, bWasSuccessful && bContextCurrent ? Valid : 0, InvalidDetails, Rejected);
	const bool bSearchSucceeded = bWasSuccessful && CachedSearchSettings.IsValid() && IsEOSContextCurrent(CycleContext);
	const auto SearchOutcome = CompleteOperation(TEXT("FindMatch"), bSearchSucceeded, !bContextCurrent ? EEOSOperationCode::Canceled : bSearchSucceeded ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure,
		bSearchSucceeded ? TEXT("Match search completed.") : TEXT("Match search failed or its identity retired."), FString(), bWasSuccessful ? TEXT("Success") : TEXT("Failure"), EEOSResultSource::NativeCallback);
	OnOperationCompleted.Broadcast(SearchOutcome);
	if (!bIsMatchmaking || CycleRequestId != CompletingCycle || bShuttingDown) return;

	// Pick the first result the player hasn't rejected this cycle.
	int32 FoundIndex = INDEX_NONE;
	if (bSearchSucceeded)
	{
		for (int32 Index = 0; Index < CachedSearchSettings->SearchResults.Num(); ++Index)
		{
			if (CachedSearchSettings->SearchResults[Index].IsValid() && CachedSearchSettings->SearchResults[Index].Session.SessionInfo.IsValid()
				&& !RejectedSessionIds.Contains(CachedSearchSettings->SearchResults[Index].Session.GetSessionIdStr()))
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

		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem: Match found — Session ID: %s"), *FEEOSNativeOperationLease::SafeField(SessionId));
		OfferRequestId = BeginOperation(TEXT("MatchOffer"), SessionId);
		TagOperationContext(TEXT("MatchOffer"), FString(), 0, CurrentSearchAttempt, CycleRequestId);
		const auto Offer = CompleteOperation(TEXT("MatchOffer"), true, EEOSOperationCode::Succeeded, TEXT("Retained match offered; join requires explicit acceptance."), FString(), TEXT("SearchResult"), EEOSResultSource::NativeCallback);
		FEEOSOutcomeDispatchScope Dispatch(this, Offer);
		OnOperationCompleted.Broadcast(Offer);
		if (!bIsMatchmaking || CycleRequestId != CompletingCycle || SelectedResultIndex != FoundIndex) return;
		OnMatchmakingStatusChanged.Broadcast(TEXT("Match found!"));
		if (!bIsMatchmaking || CycleRequestId != CompletingCycle || SelectedResultIndex != FoundIndex) return;
		OnMatchFound.Broadcast(SessionId);
		return;
	}

	// No acceptable match this attempt (search failed, empty, or every result was rejected).
	RetryOrEndCycle(bSearchSucceeded ? TEXT("no acceptable sessions") : TEXT("search failed"));
}

bool UEEOSMatchmakingSubsystem::CancelMatchmaking()
{
	if (!bShuttingDown && JoinLease.IsValid())
	{
		RejectOperation(TEXT("CancelMatchmaking"), EEOSOperationCode::Busy, TEXT("A match join has already been submitted.")); return false;
	}

	if (!bIsMatchmaking)
	{
		// Nothing to cancel — log-only rejection. Broadcasting OnMatchmakingCancelled here
		// would fake a terminal event for a cycle that never existed.
		UE_LOG(LogExtendedEOS, Verbose, TEXT("EEOSMatchmakingSubsystem::CancelMatchmaking — Not currently matchmaking (no delegate will fire)"));
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

	// Detach this cycle without canceling the SDK search: its ordinary callback can
	// still arrive after native cancellation. Keep admission leased until it retires.
	if (MatchmakingCompleteDelegateHandle.IsValid())
	{
		if (UEEOSSearchCoordinator* Coordinator = GetSearchCoordinator()) Coordinator->Retire(MatchmakingSearchOwner);
	}
	if (SearchSessions.IsValid()) SearchSessions->ClearOnFindSessionsCompleteDelegate_Handle(MatchmakingCompleteDelegateHandle);
	MatchmakingCompleteDelegateHandle.Reset(); SearchSessions.Reset();
	ReleaseSearchSlot();

	bIsMatchmaking = false;
	CurrentQueueName.Empty();
	CurrentAttributes.Empty();
	RejectedSessionIds.Empty();
	CurrentSearchAttempt = 0;
	SelectedResultIndex = INDEX_NONE;
	CachedSearchSettings.Reset();

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::CancelMatchmaking — Cancelled after %.1f seconds"), ElapsedTime);
	const auto Outcome = CompleteCycle(false, EEOSOperationCode::Canceled, TEXT("Matchmaking canceled; native search retirement remains independent."));
	FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
	if (!bShuttingDown) { OnMatchmakingCancelled.Broadcast(); OnOperationCompleted.Broadcast(Outcome); }
	return true;
}

bool UEEOSMatchmakingSubsystem::AcceptMatch()
{
	if (bShuttingDown || JoinLease.IsValid()) { RejectOperation(TEXT("AcceptMatch"), EEOSOperationCode::Busy, TEXT("A match join is pending or shutting down.")); return false; }
	// ALL Accept guards are non-terminal: the matchmaking cycle stays alive, so none of them
	// may broadcast the cycle-terminal OnMatchmakingComplete — log and return false instead
	// (a rejected call emits only its detailed rejection).
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("AcceptMatch"));
		return false;
	}

	// SelectedResultIndex is only valid while a found match is awaiting accept/reject.
	if (!CachedSearchSettings.IsValid() || !CachedSearchSettings->SearchResults.IsValidIndex(SelectedResultIndex))
	{
		RejectOperation(TEXT("AcceptMatch"), EEOSOperationCode::InvalidTarget, TEXT("No retained match offer is available to accept."));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		RejectOperation(TEXT("AcceptMatch"), EEOSOperationCode::UnsupportedCapability, TEXT("The native session interface is unavailable."));
		return false;
	}

	if (JoinSessionDelegateHandle.IsValid())
	{
		// A previous AcceptMatch join is still in flight; rebinding here would orphan its
		// completion.
		RejectOperation(TEXT("AcceptMatch"), EEOSOperationCode::Busy, TEXT("Another admitted request already owns this operation."));
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
		RejectOperation(TEXT("AcceptMatch"), EEOSOperationCode::Busy, TEXT("GameSession must be absent before accepting this match."), TEXT("GameSession"));
		return false;
	}

	const auto Identity = EOSSub->GetIdentityInterface();
	const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	if (!Local.IsValid() || !Local->IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn)
	{
		RejectOperation(TEXT("AcceptMatch"), EEOSOperationCode::IdentityUnavailable, TEXT("Match join requires a logged-in native identity.")); return false;
	}
	if (!JoinLease.TryAcquire(SessionInterface.Get(), GameSessionName, this, TEXT("AcceptMatch")))
	{
		RejectOperation(TEXT("AcceptMatch"), EEOSOperationCode::Busy, TEXT("Another plugin operation owns GameSession.")); return false;
	}
	JoinSessions = SessionInterface; MatchJoinContext = CaptureEOSContext();
	const int64 Request = BeginOperation(TEXT("AcceptMatch"), CachedSearchSettings->SearchResults[SelectedResultIndex].GetSessionIdStr(), JoinLease.GetRequestId());
	// Join the found session (the reject-filtered pick, not necessarily index 0)
	TagOperationContext(TEXT("AcceptMatch"), FString(), 0, CurrentSearchAttempt, OfferRequestId);
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

	const bool bStarted = SessionInterface->JoinSession(0, PendingAcceptSessionName, SearchResultCopy);
	if (!bStarted && JoinLease.GetRequestId() == Request)
	{
		bJoinSubmissionRejected = true;
		HandleAcceptMatchJoinComplete(GameSessionName, EOnJoinSessionCompleteResult::UnknownError);
		return false;
	}
	if (!JoinSessionDelegateHandle.IsValid() || JoinLease.GetRequestId() != Request) return bStarted;

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::AcceptMatch — Joining session '%s'..."), *FEEOSNativeOperationLease::SafeField(PendingAcceptSessionId));
	OnMatchmakingStatusChanged.Broadcast(TEXT("Match accepted, joining session..."));
	return true;
}

void UEEOSMatchmakingSubsystem::HandleAcceptMatchJoinComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result)
{
	// OnJoinSessionComplete is interface-wide: a lobby (or other) join completing also lands
	// here. Ignore completions that aren't our pending match join — without clearing the
	// handle or broadcasting.
	if (!JoinSessionDelegateHandle.IsValid() || InSessionName != PendingAcceptSessionName)
	{
		LogCallbackDisposition(TEXT("AcceptMatch"), JoinLease.GetRequestId(), InSessionName != PendingAcceptSessionName ? TEXT("DifferentOwner") : TEXT("Duplicate"), MatchJoinContext.Generation);
		return;
	}

	const bool bContextCurrent = IsEOSContextCurrent(MatchJoinContext);
	LogCallbackDisposition(TEXT("AcceptMatch"), JoinLease.GetRequestId(), bShuttingDown ? TEXT("ShutdownInternalOnly") : bContextCurrent ? TEXT("Consumed") : TEXT("StaleGeneration"), MatchJoinContext.Generation);
	if (JoinSessions.IsValid()) JoinSessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
	JoinSessionDelegateHandle.Reset();
	PendingAcceptSessionName = NAME_None;

	const FString SessionId = PendingAcceptSessionId;
	PendingAcceptSessionId.Empty();

	const auto* Native = JoinSessions.IsValid() ? JoinSessions->GetNamedSession(InSessionName) : nullptr;
	const bool bJoinSuccess = bContextCurrent && Result == EOnJoinSessionCompleteResult::Success && Native && Native->SessionInfo.IsValid()
		&& Native->SessionInfo->IsValid() && Native->GetSessionIdStr() == SessionId;
	const bool bSubmissionRejected = bJoinSubmissionRejected; bJoinSubmissionRejected = false;
	JoinLease.Reset(); JoinSessions.Reset();
	const auto Outcome = CompleteOperation(TEXT("AcceptMatch"), bJoinSuccess, bJoinSuccess ? EEOSOperationCode::Succeeded : !bContextCurrent ? EEOSOperationCode::Canceled : bSubmissionRejected ? EEOSOperationCode::NativeStartRejected : EEOSOperationCode::NativeFailure,
		!bContextCurrent ? TEXT("Match join identity/platform retired before completion.") : bSubmissionRejected ? TEXT("Native match join refused submission.") : FString::Printf(TEXT("Match join returned %s; the SDK result is unavailable through the native callback."), LexToString(Result)),
		bJoinSuccess ? SessionId : FString(), bSubmissionRejected ? FString() : FString(LexToString(Result)), bSubmissionRejected ? EEOSResultSource::Plugin : EEOSResultSource::NativeCallback);
	FEEOSOutcomeDispatchScope Dispatch(this, Outcome);

	// The matchmaking cycle ends with this join (success or not) — reset cycle state.
	bIsMatchmaking = false;
	CurrentQueueName.Empty();
	CurrentAttributes.Empty();
	RejectedSessionIds.Empty();
	CurrentSearchAttempt = 0;
	SelectedResultIndex = INDEX_NONE;
	CachedSearchSettings.Reset();

	const auto CycleOutcome = CompleteCycle(bJoinSuccess, bJoinSuccess ? EEOSOperationCode::Succeeded : Outcome.Code, Outcome.Message);
	FEEOSOutcomeDispatchScope CycleDispatch(this, CycleOutcome);
	if (bShuttingDown) return;
	if (bJoinSuccess)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::AcceptMatch — Joined session '%s' successfully"), *FEEOSNativeOperationLease::SafeField(SessionId));
		OnMatchmakingStatusChanged.Broadcast(TEXT("Match accepted, joined session"));
		OnMatchmakingComplete.Broadcast(true, TEXT(""));
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMatchmakingSubsystem::AcceptMatch — Failed to join session '%s'"), *FEEOSNativeOperationLease::SafeField(SessionId));
		OnMatchmakingComplete.Broadcast(false, Outcome.Message);
	}
	OnOperationCompleted.Broadcast(Outcome); OnOperationCompleted.Broadcast(CycleOutcome);
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
		RejectOperation(TEXT("RejectMatch"), EEOSOperationCode::Busy, TEXT("Another admitted request already owns this operation."));
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
	const int64 RejectingCycle = CycleRequestId;
	BeginOperation(TEXT("RejectMatch"), RejectedSessionId); TagOperationContext(TEXT("RejectMatch"), FString(), 0, CurrentSearchAttempt, OfferRequestId);
	const auto Outcome = CompleteOperation(TEXT("RejectMatch"), true, EEOSOperationCode::Succeeded, TEXT("Offer rejected for this matchmaking cycle."));
	RejectedSessionIds.Add(RejectedSessionId);
	SelectedResultIndex = INDEX_NONE;
	CachedSearchSettings.Reset();

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::RejectMatch — Rejected session '%s', re-queuing"), *FEEOSNativeOperationLease::SafeField(RejectedSessionId));

	// Re-queue directly, NOT via StartMatchmaking (which would clear RejectedSessionIds as a
	// fresh cycle). Keeps the queue name, attributes, start timestamp and reject-exclusion
	// set; grants a fresh search-attempt budget.
	CurrentSearchAttempt = 0;
	OnOperationCompleted.Broadcast(Outcome);
	if (!bIsMatchmaking || CycleRequestId != RejectingCycle || bShuttingDown) return true;
	OnMatchmakingStatusChanged.Broadcast(TEXT("Match rejected, searching again..."));
	if (bIsMatchmaking && CycleRequestId == RejectingCycle && !bShuttingDown) IssueMatchmakingSearch();
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

FEEOSOperationOutcome UEEOSMatchmakingSubsystem::CompleteCycle(bool bSuccess, EEOSOperationCode Code, const FString& Message)
{
	const auto Outcome = CompleteOperation(TEXT("Matchmaking"), bSuccess, Code, Message);
	CycleRequestId = 0; OfferRequestId = 0; bIsMatchmaking = false; return Outcome;
}
