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
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSFriendsSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Read (refresh) the friends list from EOS */
	UFUNCTION(BlueprintCallable, Category = "EOS|Friends")
	void ReadFriendsList();

	/** Send a friend invite to a user */
	UFUNCTION(BlueprintCallable, Category = "EOS|Friends")
	void SendFriendInvite(const FString& FriendUserId);

	/** Accept a pending friend invite */
	UFUNCTION(BlueprintCallable, Category = "EOS|Friends")
	void AcceptFriendInvite(const FString& FriendUserId);

	/** Reject a pending friend invite */
	UFUNCTION(BlueprintCallable, Category = "EOS|Friends")
	void RejectFriendInvite(const FString& FriendUserId);

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
