// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Containers/Ticker.h"
#include "EEOSSessionSubsystem.generated.h"

class UEEOSSearchCoordinator;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSSessionCreated, bool, bSuccess, const FString&, SessionName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSSessionsFound, const TArray<FEEOSSessionSearchResult>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSSessionJoined, bool, bSuccess, const FString&, SessionName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSSessionDestroyed, bool, bSuccess, const FString&, SessionName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSSessionInviteAccepted, bool, bSuccess, const FString&, SessionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSSessionStarted, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSSessionEnded, bool, bSuccess, const FString&, SessionName);

/**
 * Manages EOS game sessions — create, find, join, leave, and destroy.
 *
 * Return-value contract (all async actions): true means the operation really started and its
 * completion delegate will fire exactly once. false means the call was rejected (an operation
 * of the same kind — or one sharing its state — is already in flight; logged, and NO completion
 * delegate fires for the rejected call) or failed pre-flight. Pre-flight failures that occur
 * with no same-kind operation in flight (EOS unavailable, session interface missing, invalid
 * search index) DO broadcast the operation's failure delegate — that is safe because no
 * legitimate waiter of that kind exists; each method documents its exceptions.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSSessionSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Create a new game session (basic).
	 *  @return false if rejected (a create or destroy is already in flight — no delegate will
	 *  fire) or failed pre-flight (EOS unavailable / interface missing — these DO broadcast
	 *  OnSessionCreated(false)); true if the create started (OnSessionCreated fires once). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	bool CreateSession(int32 MaxPlayers = 4, bool bIsLAN = false, bool bIsPresence = true, const FString& SessionName = TEXT("GameSession"));

	/** Create a session with full configuration.
	 *  @return same contract as CreateSession. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	bool CreateSessionAdvanced(const FEEOSSessionSettings& Settings, const FString& SessionName = TEXT("GameSession"));

	/** Search for available sessions.
	 *  @return false if rejected (our own search, or any sibling subsystem's session/lobby
	 *  search, is already in flight — no delegate will fire) or failed pre-flight (EOS
	 *  unavailable / interface missing, and the synchronous engine FindSessions failure —
	 *  these DO broadcast OnSessionsFound with empty results); true if the search started
	 *  (OnSessionsFound fires once). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	bool FindSessions(int32 MaxResults = 20);

	/** Search for sessions with custom attribute filters (e.g. "REGION" → "Europe" to match sessions advertising FEEOSSessionSettings::Region).
	 *  @return same contract as FindSessions. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	bool FindSessionsFiltered(int32 MaxResults, const TMap<FString, FString>& SearchFilters);

	/** Join a session from search results by index.
	 *  @return false if rejected (a join is already in flight — no delegate will fire) or
	 *  failed pre-flight (EOS unavailable / invalid index / interface missing — these DO
	 *  broadcast OnSessionJoined(false)); true if the join started (OnSessionJoined fires once). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	bool JoinSession(int32 SearchResultIndex, const FString& SessionName = TEXT("GameSession"));

	/** Destroy the current session.
	 *  @return false if rejected (a destroy, or a create chain that owns the session state, is
	 *  already in flight — no delegate will fire) or failed pre-flight (EOS unavailable /
	 *  interface missing — these DO broadcast OnSessionDestroyed(false)); true if the destroy
	 *  started (OnSessionDestroyed fires once). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	bool DestroySession(const FString& SessionName = TEXT("GameSession"));

	/** Start the session (marks it InProgress, disables join if not AllowJoinInProgress).
	 *  @return false if rejected (a start is already in flight — no delegate will fire) or
	 *  failed pre-flight (EOS unavailable / interface missing — these DO broadcast
	 *  OnSessionStarted(false)); true if the start began (OnSessionStarted fires once). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	bool StartSession(const FString& SessionName = TEXT("GameSession"));

	/** End the session (marks it Ended).
	 *  @return false if rejected (an end is already in flight — no delegate will fire) or
	 *  failed pre-flight (EOS unavailable / interface missing — these DO broadcast
	 *  OnSessionEnded(false)); true if the end began (OnSessionEnded fires once). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	bool EndSession(const FString& SessionName = TEXT("GameSession"));

	/** Register a player into the session.
	 *  @return false if the call could not be issued (EOS unavailable, interfaces missing, or
	 *  the player id could not be parsed); no delegate fires either way. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	bool RegisterPlayer(const FString& SessionName, const FString& PlayerId, bool bWasInvited = false);

	/** Unregister a player from the session.
	 *  @return same contract as RegisterPlayer. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	bool UnRegisterPlayer(const FString& SessionName, const FString& PlayerId);

	// ── Travel Helpers ───────────────────────────────────────────────────────

	/** Trigger server travel to the given map (appends ?listen for listen server).
	 *  @return false if travel could not be initiated (invalid world context). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions", meta = (WorldContext = "WorldContextObject"))
	bool ServerTravel(const UObject* WorldContextObject, const FString& MapPath);

	/** Resolve the connection string and travel the local player to the session host.
	 *  @return false if travel could not be initiated (invalid context, unresolvable connect
	 *  string, or no player controller). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions", meta = (WorldContext = "WorldContextObject"))
	bool ClientTravel(const UObject* WorldContextObject, const FString& SessionName = TEXT("GameSession"));

	/** Get the resolved connection string for a session */
	UFUNCTION(BlueprintPure, Category = "EOS|Sessions")
	FString GetResolvedConnectString(const FString& SessionName = TEXT("GameSession")) const;

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the cached search results */
	UFUNCTION(BlueprintPure, Category = "EOS|Sessions")
	TArray<FEEOSSessionSearchResult> GetSearchResults() const;

	/** Check if currently in a session */
	UFUNCTION(BlueprintPure, Category = "EOS|Sessions")
	bool IsInSession() const;

	/** Get the current session state */
	UFUNCTION(BlueprintPure, Category = "EOS|Sessions")
	EEOSSessionState GetSessionState(const FString& SessionName = TEXT("GameSession")) const;

	/** Generate a random alphanumeric session code */
	UFUNCTION(BlueprintPure, Category = "EOS|Sessions")
	static FString GenerateSessionCode(int32 CodeLength = 6);

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|Sessions")
	FOnEOSSessionCreated OnSessionCreated;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Sessions")
	FOnEOSSessionsFound OnSessionsFound;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Sessions")
	FOnEOSSessionJoined OnSessionJoined;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Sessions")
	FOnEOSSessionDestroyed OnSessionDestroyed;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Sessions")
	FOnEOSSessionInviteAccepted OnSessionInviteAccepted;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Sessions")
	FOnEOSSessionStarted OnSessionStarted;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Sessions")
	FOnEOSSessionEnded OnSessionEnded;

private:

	TArray<FEEOSSessionSearchResult> CachedSearchResults;
	TSharedPtr<class FOnlineSessionSearch> SessionSearch;
	bool bInSession = false;

	// ── Per-operation delegate scoping ───────────────────────────────────────
	// The engine's IOnlineSession delegate lists are interface-wide: every subsystem
	// (Sessions, Lobby, Matchmaking) binds to the SAME lists. Each pending operation
	// therefore stores its own delegate handle (a valid handle == operation in flight;
	// new calls are rejected while one is pending) plus the session name it was started
	// with, so its handler can ignore completions that belong to another operation.
	//
	// Find is special: the completion carries no session name, so it is correlated by the
	// shared UEEOSSearchCoordinator instead — while we hold the search slot no sibling
	// subsystem search can be in flight, and ANY find-completion received while our find
	// handle is bound is OUR search's terminal event. (Do NOT gate on the search object's
	// SearchState: the engine's zero-result path fires the completion without ever setting
	// it — OnlineSessionEOS.cpp:2675-2679.)

	FDelegateHandle CreateSessionCompleteHandle;
	FDelegateHandle DestroyForCreateHandle;
	FDelegateHandle FindSessionsCompleteHandle;
	FDelegateHandle JoinSessionCompleteHandle;
	FDelegateHandle DestroySessionCompleteHandle;
	FDelegateHandle StartSessionCompleteHandle;
	FDelegateHandle EndSessionCompleteHandle;

	FName PendingCreateSessionName;
	FName PendingJoinSessionName;
	FName PendingDestroySessionName;
	FName PendingStartSessionName;
	FName PendingEndSessionName;

	/** Settings staged by CreateSession/CreateSessionAdvanced for the shared destroy-then-create chain */
	FOnlineSessionSettings PendingCreateSettings;

	// ── Subsystem-lifetime notifications ─────────────────────────────────────
	// The OSS may not be loaded yet when Initialize runs (the Shared base documents this
	// race); if the first registration attempt fails, a 1 Hz ticker retries until the OSS
	// appears so invite notifications aren't silently dead for the whole session.

	FDelegateHandle SessionInviteAcceptedHandle;
	FTSTicker::FDelegateHandle NotificationRetryTickerHandle;

	/** Register the invite-accepted notification; returns true once registered. */
	bool TryRegisterLifetimeNotifications();
	/** Ticker body for the lazy registration retry; stops ticking on success. */
	bool TickRetryRegisterNotifications(float DeltaTime);

	// ── Search coordination ──────────────────────────────────────────────────

	/** The shared search coordinator (may be null during GameInstance teardown). */
	UEEOSSearchCoordinator* GetSearchCoordinator() const;
	/** Acquire the cross-subsystem search slot; false while any session/lobby search is in flight. */
	bool TryAcquireSearchSlot();
	/** Release the search slot if this subsystem holds it (safe to call on every terminal path). */
	void ReleaseSearchSlot();

	void HandleCreateSessionComplete(FName InSessionName, bool bWasSuccessful);
	void HandleDestroyThenCreateComplete(FName InSessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName InSessionName, bool bWasSuccessful);
	void HandleStartSessionComplete(FName InSessionName, bool bWasSuccessful);
	void HandleEndSessionComplete(FName InSessionName, bool bWasSuccessful);
	void HandleSessionInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
};
