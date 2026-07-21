// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Containers/Ticker.h"
#include "EEOSLobbySubsystem.generated.h"

class UEEOSSearchCoordinator;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSLobbyCreated, bool, bSuccess, const FString&, LobbyId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSLobbiesFound, const TArray<FEEOSSessionSearchResult>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSLobbyJoined, bool, bSuccess, const FString&, LobbyId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSLobbyMemberJoined, const FString&, MemberId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSLobbyMemberLeft, const FString&, MemberId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSLobbyAttributeChanged, const FString&, Key, const FString&, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSLobbyOwnerChanged, const FString&, NewOwnerId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSLobbyDestroyed, bool, bSuccess, const FString&, LobbyId);

/**
 * Manages EOS lobbies with member management, attribute syncing, and lobby discovery.
 *
 * Return-value contract (all async actions): true means the operation really started and its
 * completion delegate will fire exactly once. false means the call was rejected (an operation
 * of the same kind is already in flight — logged, and NO completion delegate fires for the
 * rejected call) or failed pre-flight. Pre-flight failures that occur with no same-kind
 * operation in flight (EOS unavailable, interface missing, invalid index, not in a lobby)
 * DO broadcast the operation's failure delegate where the method historically did — each
 * method documents its exceptions.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSLobbySubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Create / Join / Leave ────────────────────────────────────────────────

	/** Create a new lobby. If a lobby already exists it is destroyed first and the create
	 *  runs from the destroy completion. Completion: OnLobbyCreated (exactly once).
	 *  @return false if rejected (a lobby create/destroy is already in flight — no delegate
	 *  will fire) or failed pre-flight (EOS unavailable / interface missing — these DO
	 *  broadcast OnLobbyCreated(false)); true if the create started. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool CreateLobby(int32 MaxMembers = 4, bool bIsPublic = true, bool bUseVoiceChat = false);

	/** Search for available lobbies.
	 *  @return false if rejected (our own lobby search, or any sibling subsystem's
	 *  session/lobby search, is already in flight — no delegate will fire) or failed
	 *  pre-flight (EOS unavailable / interface missing, and the synchronous engine
	 *  FindSessions failure — these DO broadcast OnLobbiesFound with empty results);
	 *  true if the search started (OnLobbiesFound fires once). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool FindLobbies(int32 MaxResults = 20);

	/** Search for lobbies with custom attribute filters.
	 *  @return same contract as FindLobbies. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool FindLobbiesFiltered(int32 MaxResults, const TMap<FString, FString>& SearchFilters);

	/** Join a lobby from search results.
	 *  @return false if rejected (a join-lobby is already in flight — no delegate will fire)
	 *  or failed pre-flight (EOS unavailable / invalid index / interface missing — these DO
	 *  broadcast OnLobbyJoined(false)); true if the join started (OnLobbyJoined fires once). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool JoinLobby(int32 SearchResultIndex);

	/** Leave the current lobby (any member). Completion: OnLobbyDestroyed (exactly once).
	 *  @return false if rejected (a lobby create/destroy is already in flight — no delegate
	 *  will fire) or failed pre-flight (not in a lobby / EOS unavailable / interface missing —
	 *  these DO broadcast OnLobbyDestroyed(false)); true if the leave started. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool LeaveLobby();

	/** Destroy the current lobby (owner only — non-owners should call LeaveLobby).
	 *  Completion: OnLobbyDestroyed (exactly once).
	 *  @return false if rejected (a lobby create/destroy is already in flight — no delegate
	 *  will fire) or failed pre-flight (not in a lobby / not the owner / EOS unavailable /
	 *  interface missing — these DO broadcast OnLobbyDestroyed(false)); true if the destroy
	 *  started. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool DestroyLobby();

	// ── Lobby Attributes ─────────────────────────────────────────────────────

	/** Set a lobby-level attribute (owner only; synced to all members).
	 *  OnLobbyAttributeChanged broadcasts from the update completion on success.
	 *  @return false if rejected (another lobby update is in flight) or failed pre-flight
	 *  (EOS unavailable / not the owner / interface or settings missing); no delegate fires
	 *  for a false return. */
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
	 *  no delegate fires for a false return. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool SetMemberAttribute(const FString& Key, const FString& Value);

	/** Get a member attribute by user ID (composite net-id string or bare Product User ID) and key */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	FString GetMemberAttribute(const FString& MemberId, const FString& Key) const;

	// ── Member Management ────────────────────────────────────────────────────

	/** Get list of all member IDs in the current lobby (from the engine's MemberSettings —
	 *  the EOS lobby flow never populates RegisteredPlayers). */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	TArray<FString> GetLobbyMembers() const;

	/** Get the current member count */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	int32 GetLobbyMemberCount() const;

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
	 *  OnLobbyOwnerChanged broadcasts on SDK success.
	 *  @return false if the request could not be issued (EOS unavailable / not in a lobby /
	 *  not the owner / unparsable ids); no delegate fires for a false return. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	bool PromoteMember(const FString& MemberId);

	// ── Lobby Settings ───────────────────────────────────────────────────────

	/** Change lobby joinability (public, friends-only, invite-only). Owner only — routed
	 *  through UpdateSession, so it shares the single in-flight lobby-update slot with
	 *  SetLobbyAttribute/SetMemberAttribute.
	 *  @return false if rejected (another lobby update is in flight) or failed pre-flight;
	 *  no delegate fires for a false return. */
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

	/** Get the current lobby's backend id (the real EOS lobby id) */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	FString GetCurrentLobbyId() const;

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
	FOnEOSLobbyDestroyed OnLobbyDestroyed;

private:

	FString CurrentLobbyId;
	bool bInLobby = false;
	TMap<FString, FString> CachedLobbyAttributes;
	TSharedPtr<class FOnlineSessionSearch> LobbySearch;

	// ── Per-operation delegate scoping ───────────────────────────────────────
	// The engine's IOnlineSession delegate lists are interface-wide and shared with the
	// Sessions and Matchmaking subsystems. Each pending lobby operation stores its own
	// delegate handle (a valid handle == operation in flight; new calls are rejected),
	// and every handler filters on LOBBY_SESSION_NAME before clearing its handle or
	// broadcasting. Find is the exception: its completion carries no session name, so it
	// is correlated by the shared UEEOSSearchCoordinator — while we hold the search slot,
	// ANY find-completion received while our find handle is bound is OUR terminal event.

	FDelegateHandle CreateLobbyCompleteHandle;
	FDelegateHandle DestroyForCreateLobbyHandle;
	FDelegateHandle FindLobbiesCompleteHandle;
	FDelegateHandle JoinLobbyCompleteHandle;
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
	enum class EPendingLobbyUpdate : uint8 { None, LobbyAttribute, MemberAttribute, Joinability };
	EPendingLobbyUpdate PendingUpdateKind = EPendingLobbyUpdate::None;
	FString PendingAttributeKey;
	FString PendingAttributeValue;

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
	void HandleDestroyThenCreateLobbyComplete(FName InSessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
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
