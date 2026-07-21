// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSFriendsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSFriendsListReady, const TArray<FEEOSFriendInfo>&, Friends);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEOSFriendsListChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSFriendInviteSent, bool, bSuccess, const FString&, FriendUserId);

/**
 * Manages the EOS friends list, invitations, and friend queries.
 *
 * Entry points return bool: true → the operation started (its completion delegate, where one
 * exists, will broadcast exactly once with the real backend result); false → pre-flight or
 * synchronous failure (delegate-carrying operations have already broadcast the failure).
 * AcceptFriendInvite/RejectFriendInvite have no completion delegates — failures are logged.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSFriendsSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Read (refresh) the friends list from EOS. Completion arrives on OnFriendsListReady. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Friends")
	bool ReadFriendsList();

	/** Send a friend invite to a user. OnFriendInviteSent broadcasts the REAL backend result
	 *  (exactly once), not merely whether the request started. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Friends")
	bool SendFriendInvite(const FString& FriendUserId);

	/** Accept a pending friend invite. Returns false (with a clear log) on failure — there is
	 *  no completion delegate for this operation; watch OnFriendsListChanged for the result. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Friends")
	bool AcceptFriendInvite(const FString& FriendUserId);

	/** Reject a pending friend invite. Returns false (with a clear log) on failure — there is
	 *  no completion delegate for this operation. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Friends")
	bool RejectFriendInvite(const FString& FriendUserId);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the cached friends list (call ReadFriendsList first) */
	UFUNCTION(BlueprintPure, Category = "EOS|Friends")
	TArray<FEEOSFriendInfo> GetFriendsList() const;

	/** Get the number of online friends */
	UFUNCTION(BlueprintPure, Category = "EOS|Friends")
	int32 GetOnlineFriendCount() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|Friends")
	FOnEOSFriendsListReady OnFriendsListReady;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Friends")
	FOnEOSFriendsListChanged OnFriendsListChanged;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Friends")
	FOnEOSFriendInviteSent OnFriendInviteSent;

private:

	TArray<FEEOSFriendInfo> CachedFriends;

	void HandleReadFriendsListComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr);
};
