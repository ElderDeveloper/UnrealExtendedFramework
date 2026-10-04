// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "EEOSLobbyJoinRequest.h"
#include "EEOSLobbyExitRequest.h"
#include "Shared/EEOSNativeOperation.h"
#include "EEOSLobbySubsystem.generated.h"

class UEEOSSearchCoordinator;
class UNetDriver;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSLobbyCreated, bool, bSuccess, const FString&, LobbyId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSLobbiesFound, const TArray<FEEOSSessionSearchResult>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSLobbyJoined, bool, bSuccess, const FString&, LobbyId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSLobbyMemberJoined, const FString&, MemberId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSLobbyMemberLeft, const FString&, MemberId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSLobbyAttributeChanged, const FString&, Key, const FString&, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSLobbyAttributeRemoved, const FString&, Key);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSLobbyRemoved, const FString&, LobbyId, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSLobbyOwnerChanged, const FString&, NewOwnerId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSLobbyPromotionComplete, bool, bSuccess, const FString&, MemberId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSLobbyDestroyed, bool, bSuccess, const FString&, LobbyId);

/**
 * Manages EOS lobbies with member management, attribute syncing, and lobby discovery.
 *
 * Boolean acceptance and completion are separate. Busy/shared ownership rejection emits
 * OnOperationRejected without a legacy completion. Idle preflight failures retain their
 * method's legacy failure event; immediate attribute setters and submission/travel helpers
 * are exceptions documented below. Accepted membership/search work has a detailed terminal
 * outcome. During shutdown terminal records are internal and gameplay events are suppressed.
 * Docs/EOSOperations.md describes admission, retained updates, and native result limits.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSLobbySubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Cancel continuations and drain lobby exit before GameInstance teardown. C++ shutdown only. */
	bool ShutdownLobby(float MaxSeconds = 2.0f);

	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	bool IsLobbyOperationInFlight() const { return IsMembershipOperationInFlight(); }

	// ── Create / Join / Leave ────────────────────────────────────────────────

	/** Create a new lobby. If a lobby already exists its owner deletes it (other members leave) first and the create
	 *  runs from the destroy completion. Completion: OnLobbyCreated (exactly once).
	 *  @return false if rejected (a lobby membership operation is already in flight — no legacy completion
	 *  will fire) or failed pre-flight (EOS unavailable / interface missing — these DO
	 *  broadcast OnLobbyCreated(false)); true if the create started. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool CreateLobby(int32 MaxMembers = 4, bool bIsPublic = true, bool bUseVoiceChat = false);

	/** Search for available lobbies.
	 *  @return false if rejected (our own lobby search, or any sibling subsystem's
	 *  session/lobby search, is already in flight — no legacy completion will fire) or failed
	 *  pre-flight (EOS unavailable / interface missing, and the synchronous engine
	 *  FindSessions failure — these DO broadcast OnLobbiesFound with empty results);
	 *  true if the search started (OnLobbiesFound fires once). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool FindLobbies(int32 MaxResults = 20);

	/** Search for lobbies with custom attribute filters.
	 *  @return same contract as FindLobbies. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool FindLobbiesFiltered(int32 MaxResults, const TMap<FString, FString>& SearchFilters);

	/** Join a lobby from search results. An existing owned lobby is deleted; other members leave first. The target
	 *  result is retained until leaving finishes. Membership operations cannot overlap.
	 *  @return false if rejected (a membership operation is already in flight — no legacy completion will fire)
	 *  or failed pre-flight (EOS unavailable / invalid index / interface missing — these DO
	 *  broadcast OnLobbyJoined(false)); true if the join started (OnLobbyJoined fires once). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool JoinLobby(int32 SearchResultIndex);

	/** Join a lobby from the last search by its id rather than its position. An index stays valid only while the
	 *  search it came from is the last one; once another search replaces the results it names a different lobby,
	 *  where an id that is no longer there fails instead. Same completion contract as JoinLobby. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool JoinLobbyById(const FString& LobbyId);

	/**
	 * Join a lobby from an invite or other result that is not in the last search list.
	 * Same leave-then-join path and completion contract as JoinLobby.
	 */
	bool JoinLobbyResult(const FOnlineSessionSearchResult& SearchResult);

	/** Leave the current lobby (any member). Completion: OnLobbyDestroyed (exactly once).
	 *  @return false if rejected (a lobby membership operation is already in flight — no legacy completion
	 *  will fire) or failed pre-flight (not in a lobby / EOS unavailable / interface missing —
	 *  these DO broadcast OnLobbyDestroyed(false)); true if the leave started. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool LeaveLobby();

	/** Destroy the current lobby (owner only — non-owners should call LeaveLobby).
	 *  Uses EOS_Lobby_DestroyLobby, removing it for every member regardless of host migration.
	 *  Completion: OnLobbyDestroyed after local session/voice cleanup (exactly once).
	 *  @return false if rejected (a lobby membership operation is already in flight — no legacy completion
	 *  will fire) or failed pre-flight (not in a lobby / not the owner / EOS unavailable /
	 *  interface missing — these DO broadcast OnLobbyDestroyed(false)); true if the destroy
	 *  started. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool DestroyLobby();

	// ── Lobby Attributes ─────────────────────────────────────────────────────

	/** Set a lobby-level attribute (owner only; synced to all members).
	 *  OnLobbyAttributeChanged broadcasts from the update completion on success.
	 *  @return false if rejected (another lobby update is in flight) or failed pre-flight
	 *  (EOS unavailable / not the owner / interface or settings missing); no legacy change event fires
	 *  for a false return. Detailed rejection/completion records remain separate. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool SetLobbyAttribute(const FString& Key, const FString& Value);

	/** Get a lobby-level attribute by key */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	FString GetLobbyAttribute(const FString& Key) const;

	/** Get all lobby attributes as key-value pairs */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	TMap<FString, FString> GetAllLobbyAttributes() const;

	// ── Member Attributes ────────────────────────────────────────────────────

	/** Set a per-member attribute for the LOCAL member (e.g., ready status, character selection).
	 *  Any member may call this; it is published to the lobby via the engine's MemberSettings path.
	 *  @return false if rejected (another lobby update is in flight) or failed pre-flight;
	 *  no legacy completion fires for a false return. Detailed records are separate. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool SetMemberAttribute(const FString& Key, const FString& Value);

	/** Retained writes. A nonzero request ID is accepted work; follow OnLobbyUpdateCompleted.
	 * Queued writes are bounded and generation-scoped. Replaced pending writes are Superseded. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	int64 QueueLobbyAttribute(const FString& Key, const FString& Value);
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	int64 QueueMemberAttribute(const FString& Key, const FString& Value);
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	int64 QueueLobbyVisibility(bool bIsPublic);
	/** Owner-only capacity update; rejects shrinking below current membership. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	int64 QueueLobbyCapacity(int32 MaxMembers);
	UPROPERTY(BlueprintAssignable, Category = "EOS|Lobbies")
	FOnEEOSOperationOutcome OnLobbyUpdateCompleted;
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	FEEOSOperationOutcome GetLobbyUpdateOutcome(int64 RequestId) const;
	UPROPERTY(BlueprintAssignable, Category = "EOS|Lobbies")
	FOnEOSLobbyAttributeRemoved OnLobbyAttributeRemoved;

	/** Get a member attribute by user ID (composite net-id string or bare Product User ID) and key */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	FString GetMemberAttribute(const FString& MemberId, const FString& Key) const;

	// ── Member Management ────────────────────────────────────────────────────

	/** Get list of all member IDs in the current lobby (from the engine's MemberSettings —
	 *  the EOS lobby flow never populates RegisteredPlayers). */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	TArray<FString> GetLobbyMembers() const;

	/** Current SDK/native member-count observation, or -1 when unavailable. */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	int32 GetLobbyMemberCount() const;
	/** Cached SDK/native observation; unknown values remain unknown and do not guarantee a join slot. */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	FEEOSCapacitySnapshot GetLobbyCapacity() const;
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	FEEOSLobbyVisibilitySnapshot GetConfirmedLobbyVisibility() const { return ConfirmedVisibility; }
	UFUNCTION(BlueprintPure, Category = "EOS|Diagnostics")
	int64 GetLobbyGeneration() const { return int64(LobbyGeneration); }
	/** Observed local removal reason for this exact lobby; empty means no observed reason. */
	UFUNCTION(BlueprintPure, Category = "EOS|Diagnostics")
	FString GetObservedRemovalReason(const FString& LobbyId) const { return LobbyId == LastRemovedLobbyId ? LastRemovalReason : FString(); }
	/** Root request for the latest membership transition; useful for voice/travel correlation. */
	UFUNCTION(BlueprintPure, Category = "EOS|Diagnostics")
	int64 GetLobbyCorrelationId() const { return MembershipLease.IsValid() ? MembershipLease.GetRequestId() : LastMembershipRequestId; }
	/** Involuntary local removal, distinct from owner-requested backend deletion. */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Lobbies")
	FOnEOSLobbyRemoved OnLobbyRemoved;

	/** Get the lobby owner's user ID */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	FString GetLobbyOwner() const;

	/** Check if the local player is the lobby owner */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	bool IsLobbyOwner() const;

	/** Kick a member from the lobby (owner only; EOS_Lobby_KickMember).
	 *  OnLobbyMemberLeft broadcasts on SDK success.
	 *  @return false if rejected (a kick for the same member is already in flight) or failed
	 *  pre-flight (EOS unavailable / not in a lobby / not the owner / unparsable ids); no
	 *  delegate fires for a false return. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool KickMember(const FString& MemberId);

	/** Promote a member to lobby owner (owner only; EOS_Lobby_PromoteMember).
	 *  Transfers EOS ownership only; it does not migrate a gameplay server.
	 *  OnLobbyPromotionComplete reports SDK success/failure once. OnLobbyOwnerChanged fires
	 *  when the native owner changes, including remote/automatic promotion, without duplicates.
	 *  @return false if the request could not be issued (EOS unavailable / not in a lobby /
	 *  not the owner / transfer disabled / unparsable ids); no legacy completion fires for a false return. Detailed records are separate. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool PromoteMember(const FString& MemberId);

	// ── Lobby Settings ───────────────────────────────────────────────────────

	/** Change lobby joinability (public, friends-only, invite-only). Owner only — routed
	 *  through UpdateSession, so it shares the single in-flight lobby-update slot with
	 *  SetLobbyAttribute/SetMemberAttribute.
	 *  @return false if rejected (another lobby update is in flight) or failed pre-flight;
	 *  no legacy completion fires for a false return. Detailed records are separate. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool SetLobbyJoinable(bool bIsPublic);

	/** Send a lobby invite to a specific user.
	 *  @return false if the invite could not be issued (EOS unavailable / interface missing /
	 *  unparsable user id). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool InviteToLobby(const FString& UserId);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Check if currently in a lobby */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	bool IsInLobby() const;

	/** Whether local leave/backend closure is still draining callbacks. */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	bool IsLobbyExitInProgress() const;

	/** Get the current lobby's backend id (the real EOS lobby id) */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	FString GetCurrentLobbyId() const;

	/** The engine session name this subsystem's lobby lives under. Game code needs it to
	 *  resolve a connect string for the lobby host: GetResolvedConnectString defaults to
	 *  "GameSession", which is the sessions flow, not this one. */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	static FName GetLobbySessionName();

	/** Last join error, including pre-flight and leave-before-join failures. */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	FString GetLastLobbyJoinError() const { return LastLobbyJoinError; }

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|Lobbies")
	FOnEOSLobbyCreated OnLobbyCreated;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Lobbies")
	FOnEOSLobbiesFound OnLobbiesFound;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Lobbies")
	FOnEOSLobbyJoined OnLobbyJoined;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Lobbies")
	FOnEOSLobbyMemberJoined OnLobbyMemberJoined;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Lobbies")
	FOnEOSLobbyMemberLeft OnLobbyMemberLeft;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Lobbies")
	FOnEOSLobbyAttributeChanged OnLobbyAttributeChanged;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Lobbies")
	FOnEOSLobbyOwnerChanged OnLobbyOwnerChanged;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Lobbies")
	FOnEOSLobbyPromotionComplete OnLobbyPromotionComplete;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Lobbies")
	FOnEOSLobbyDestroyed OnLobbyDestroyed;

private:

	FString CurrentLobbyId;
	int64 LastMembershipRequestId = 0;
	int64 ExitChildRequestId = 0;
	uint64 LobbyGeneration = 0;
	int64 LobbySearchGeneration = 0;
	double LobbySearchTime = 0;
	FEEOSNativeOperationLease MembershipLease;
	FEEOSNativeOperationLease UpdateLease;
	IOnlineSessionPtr OperationSessions;
	IOnlineSessionPtr SearchSessions;
	FEEOSRequestContext SearchContext;
	IOnlineSessionPtr NotificationSessions;
	FEEOSRequestContext MembershipContext;
	FEEOSRequestContext LobbyContext;
	FEEOSRequestContext UpdateContext;
	FEEOSLobbyVisibilitySnapshot ConfirmedVisibility;
	FString LastRemovedLobbyId;
	FString LastRemovalReason;
	bool bJoinTargetAuthorizedDetails = false;
	bool CanDeliverLobbyNotification(FName SessionName, bool bAllowRemoval = false) const;
	void UnbindLifetimeNotifications();
	void RetireActiveLobbyUpdate(const FString& Reason);
	bool bInLobby = false;
	/** Lobby the engine has reported joining whose member list it is still resolving.
	 *  FOnlineSessionEOS fires the join completion before it fills MemberSettings (member ids
	 *  resolve asynchronously), so for this lobby alone a successful native join stands in for
	 *  the member-list check. Cleared once MemberSettings lists the local user, or when the
	 *  named lobby is gone or replaced. */
	FString JoinedLobbyAwaitingMemberList;
	/** Owner, listed member, or the lobby the engine just reported joining (see above). */
	bool IsLocalLobbyMember(const FNamedOnlineSession* Session, const FUniqueNetIdPtr& Local) const;
	TMap<FString, FString> CachedLobbyAttributes;
	TSharedPtr<class FOnlineSessionSearch> LobbySearch;

	// ── Per-operation delegate scoping ───────────────────────────────────────
	// The engine's IOnlineSession delegate lists are interface-wide and shared with the
	// Sessions and Matchmaking subsystems. Each pending lobby operation stores its own
	// delegate handle (a valid handle == operation in flight; new calls are rejected),
	// and every handler filters on LOBBY_SESSION_NAME before clearing its handle or
	// broadcasting. Find is the exception: its completion carries no session name, so it
	// is serialized across plugin callers by UEEOSSearchCoordinator. An external native
	// caller bypassing that coordinator cannot be attributed by a name-less completion.

	FDelegateHandle CreateLobbyCompleteHandle;
	FDelegateHandle FindLobbiesCompleteHandle;
	FDelegateHandle JoinLobbyCompleteHandle;
	FEEOSLobbyJoinRequest JoinRequest;
	FEEOSLobbyExitRequest ExitRequest;
	FString ExitSDKResult;
	EOS_HPlatform ExitPlatform = nullptr;
	TSharedPtr<IEOSPlatformHandle, ESPMode::ThreadSafe> ExitPlatformOwner;
	TSharedPtr<struct FEEOSLateLobbyCleanup> ExitDrain;
	uint64 ExitClosedNotificationId = 0;
	int32 ExpectedNativeExitCompletions = 0;
	int32 NativeExitCompletions = 0;
	FString CachedLobbyOwnerId;
	uint64 PromotionToken = 0;
	FString PromotionLobbyId;
	bool bPromotionPending = false;
	bool bShutdownFlushed = false;
	bool bDeinitialized = false;
	FTSTicker::FDelegateHandle LobbyOwnerTickerHandle;
	FString LastLobbyJoinError;
	int64 LastStallWarningRequest = 0;
	bool bShuttingDown = false;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle DestroyLobbyCompleteHandle;
	FDelegateHandle UpdateLobbyCompleteHandle;

	// ── Subsystem-lifetime notification handles ──────────────────────────────
	// Bound once (in Initialize, or lazily via a 1 Hz retry ticker when the OSS isn't
	// loaded yet — losing the race must not leave member events dead all session), cleared
	// in Deinitialize. Handlers filter on LOBBY_SESSION_NAME (other named sessions also
	// raise these notifications).

	FDelegateHandle ParticipantJoinedHandle;
	FDelegateHandle ParticipantLeftHandle;
	FDelegateHandle SessionSettingsUpdatedHandle;

	/** Lifetime destroy listener for REMOTE lobby closure (owner destroyed the lobby /
	 *  backend closed it). Consumed only when no own leave/destroy op is in flight — the
	 *  handle-scoped listener takes precedence for our own operations. */
	FDelegateHandle LifetimeDestroyHandle;

	FTSTicker::FDelegateHandle NotificationRetryTickerHandle;

	/** Register all subsystem-lifetime notifications; returns true once registered. */
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

	/** Settings staged for CreateLobby's destroy-then-create chain. */
	FOnlineSessionSettings PendingCreateLobbySettings;

	/** Which UpdateSession-driven operation is in flight — the engine's update completion
	 *  carries only the session name, so a single in-flight update is correlated by kind. */
	enum class EPendingLobbyUpdate : uint8 { None, LobbyAttribute, MemberAttribute, Joinability, Capacity };
	EPendingLobbyUpdate PendingUpdateKind = EPendingLobbyUpdate::None;
	FString PendingAttributeKey;
	FString PendingAttributeValue;
	FString PendingUpdateLobbyId;
	uint64 PendingUpdateGeneration = 0;
	int64 PendingUpdateRequestId = 0;
	bool bUpdateSubmissionRejected = false;
	TOptional<FOnlineSessionSetting> PreviousUpdateSetting;
	bool bPreviousVisibility = false;
	int32 PreviousPublicConnections = 0;
	int32 PreviousPrivateConnections = 0;
	bool bCreatedMemberEntry = false;
	FUniqueNetIdPtr PendingUpdateMember;
	IOnlineSessionPtr UpdateSessions;
	struct FQueuedLobbyUpdate
	{
		int64 RequestId = 0;
		FString LobbyId;
		uint64 Generation = 0;
		EPendingLobbyUpdate Kind = EPendingLobbyUpdate::None;
		FString Key, Value;
	};
	TArray<FQueuedLobbyUpdate> QueuedUpdates;
	TArray<int64> UpdateFollowers;
	TMap<int64, FEEOSOperationOutcome> UpdateOutcomeHistory;
	TMap<int64, FEEOSOperationOutcome> AcceptedUpdateMetadata;
	TMap<int64, double> AcceptedUpdateStartSeconds;
	TArray<int64> UpdateOutcomeOrder;
	void PublishLobbyUpdateOutcome(const FEEOSOperationOutcome& Outcome);
	bool AcquireLobbyMembership(FName Operation, const FString& TargetId = FString());
	bool SubmitLobbyCreation();
	void FinishLobbyCreation(bool bSuccess, EEOSOperationCode Code, const FString& Message, EEOSResultSource Source = EEOSResultSource::Plugin, const FString& NativeResult = FString());
	bool StartLobbyUpdate(EPendingLobbyUpdate Kind, const FString& Key, const FString& Value, int64 RequestId = 0);
	int64 EnqueueLobbyUpdate(EPendingLobbyUpdate Kind, const FString& Key, const FString& Value);
	void PumpLobbyUpdates();
	void CancelQueuedLobbyUpdates();
	void RollbackStagedLobbyUpdate();
	void EmitQueuedUpdateOutcome(const FQueuedLobbyUpdate& Request, EEOSOperationCode Code, const FString& Message);

	/** Bare PUIDs kicked by our own EOS_Lobby_KickMember call. The SDK completion is the single
	 *  OnLobbyMemberLeft source for those members; the engine's participant-left notification is
	 *  suppressed for the same PUID ONLY when its Reason is Kicked (a voluntary leave racing a
	 *  kick must still broadcast). Entries are removed by whichever of the two fires second, on
	 *  kick failure (only when still present — a consumed suppression is never "un-consumed"),
	 *  on member rejoin, and on lobby teardown. */
	TSet<FString> PendingKickedPuids;

	/** Bare PUIDs with an EOS_Lobby_KickMember call currently in flight (per-target guard —
	 *  double-kicking the same member would double-broadcast OnLobbyMemberLeft). */
	TSet<FString> InFlightKickPuids;

	/** Reset all local lobby state (does not touch delegate handles). Returns the lobby id
	 *  that was current before the reset. */
	FString ResetLobbyState();

	void HandleCreateSessionComplete(FName InSessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	bool IsMembershipOperationInFlight() const;
	bool StartLobbyExit(const IOnlineSessionPtr& Sessions, bool bDeleteBackend, FEEOSLobbyExitRequest::EContinuation Continuation);
	void HandleBackendLobbyDeleted(uint64 Token, const FString& LobbyId, bool bDeleted, const FString& SDKResult);
	void FinishLobbyExit(bool bNativeSuccess);
	void ObserveBackendLobbyClosed(const FString& LobbyId);
	void RemoveExitCloseNotification();
	void StartNativeLobbyCleanup(const IOnlineSessionPtr& Sessions);
	void HandlePromotionComplete(uint64 Token, uint64 Generation, const FString& LobbyId, const FString& MemberId, bool bSuccess);
	bool TickLobbyOwner(float DeltaTime);
	void RefreshLobbyOwner();
	bool UpdateCachedLobbyOwner(const FString& OwnerId);
	bool ShouldHandleLocalMemberRemoval() const;
	void ReconcileRemoteLobbyExit(const FNamedOnlineSession* Session, bool bWasSuccessful);
	bool BeginJoinLobby(const FOnlineSessionSearchResult& SearchResult);
	bool StartJoiningLobby();
	bool RefreshJoinTargetCapacity(const FString& ExistingLobbyId);
	void HandleJoinCapacityPreflight(int64 RequestId, const FString& ExistingLobbyId, const FEEOSCapacitySnapshot& Capacity, const FString& SDKResult);
	void FinishJoiningLobby(bool bSuccess, const FString& Error, EEOSOperationCode Code = EEOSOperationCode::NativeFailure,
		const FString& NativeResult = FString(), EEOSResultSource Source = EEOSResultSource::Plugin, const FString& SDKResult = FString());
	void RefreshLobbyState(const FNamedOnlineSession* Session);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);
	bool ShouldLeaveAfterConnectionFailure(const UGameInstance* FailureGameInstance) const;
	friend struct FEEOSLobbyTestAccess;

	void HandleJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName InSessionName, bool bWasSuccessful);
	void HandleLifetimeSessionDestroyed(FName InSessionName, bool bWasSuccessful);
	void HandleUpdateLobbySessionComplete(FName InSessionName, bool bWasSuccessful);
	void HandleSessionParticipantJoined(FName InSessionName, const FUniqueNetId& UniqueId);
	void HandleSessionParticipantLeft(FName InSessionName, const FUniqueNetId& UniqueId, EOnSessionParticipantLeftReason Reason);
	void HandleSessionSettingsUpdated(FName InSessionName, const FOnlineSessionSettings& UpdatedSettings);

	/** Rebuild CachedLobbyAttributes from the given settings; optionally broadcast
	 *  OnLobbyAttributeChanged for keys whose value actually changed. */
	void RefreshCachedLobbyAttributes(const FOnlineSessionSettings& InSettings, bool bBroadcastChanges);
};
