// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "EEOSLobbySubsystem.generated.h"

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
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSLobbySubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Create / Join / Leave ────────────────────────────────────────────────

	/** Create a new lobby */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	void CreateLobby(int32 MaxMembers = 4, bool bIsPublic = true, bool bUseVoiceChat = false);

	/** Search for available lobbies */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	void FindLobbies(int32 MaxResults = 20);

	/** Search for lobbies with custom attribute filters */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	void FindLobbiesFiltered(int32 MaxResults, const TMap<FString, FString>& SearchFilters);

	/** Join a lobby from search results */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	void JoinLobby(int32 SearchResultIndex);

	/** Leave the current lobby */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	void LeaveLobby();

	/** Destroy the current lobby (owner only) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	void DestroyLobby();

	// ── Lobby Attributes ─────────────────────────────────────────────────────

	/** Set a lobby-level attribute (synced to all members) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	void SetLobbyAttribute(const FString& Key, const FString& Value);

	/** Get a lobby-level attribute by key */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	FString GetLobbyAttribute(const FString& Key) const;

	/** Get all lobby attributes as key-value pairs */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	TMap<FString, FString> GetAllLobbyAttributes() const;

	// ── Member Attributes ────────────────────────────────────────────────────

	/** Set a per-member attribute (e.g., ready status, character selection) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	void SetMemberAttribute(const FString& Key, const FString& Value);

	/** Get a member attribute by user ID and key */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	FString GetMemberAttribute(const FString& MemberId, const FString& Key) const;

	// ── Member Management ────────────────────────────────────────────────────

	/** Get list of all member IDs in the current lobby */
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

	/** Kick a member from the lobby (owner only) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	void KickMember(const FString& MemberId);

	/** Promote a member to lobby owner (owner only) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	void PromoteMember(const FString& MemberId);

	// ── Lobby Settings ───────────────────────────────────────────────────────

	/** Change lobby joinability (public, friends-only, invite-only) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	void SetLobbyJoinable(bool bIsPublic);

	/** Send a lobby invite to a specific user */
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobbies")
	void InviteToLobby(const FString& UserId);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Check if currently in a lobby */
	UFUNCTION(BlueprintPure, Category = "EOS|Lobbies")
	bool IsInLobby() const;

	/** Get the current lobby ID */
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

	void HandleCreateSessionComplete(FName InSessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName InSessionName, bool bWasSuccessful);
};
