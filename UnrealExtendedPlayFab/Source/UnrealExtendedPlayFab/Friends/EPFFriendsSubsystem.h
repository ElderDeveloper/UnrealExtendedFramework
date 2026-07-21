// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFFriendsSubsystem.generated.h"


UENUM(BlueprintType)
enum class EEPFFriendSource : uint8
{
	/** All friends from any source */
	All,
	/** PlayFab friends only */
	PlayFab,
	/** Steam friends (if linked) */
	Steam,
	/** Facebook friends (if linked) */
	Facebook
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFFriend
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Friends")
	FString PlayFabId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Friends")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Friends")
	FString SteamId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Friends")
	TArray<FString> Tags;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFFriendsReceived, const FEPFResult&, Result, const TArray<FEPFFriend>&, Friends);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFFriendModified, const FEPFResult&, Result);

/**
 * Friends management — list, add, remove, tag friends.
 * Supports cross-platform friend sources (PlayFab, Steam, Facebook).
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFFriendsSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Fetch the player's friends list */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Friends")
	void GetFriendsList(bool bIncludeSteamFriends = true, bool bIncludeFacebookFriends = false);

	/** Add a friend by PlayFab ID */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Friends")
	void AddFriend(const FString& FriendPlayFabId);

	/** Add a friend by display name */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Friends")
	void AddFriendByDisplayName(const FString& DisplayName);

	/** Remove a friend */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Friends")
	void RemoveFriend(const FString& FriendPlayFabId);

	/** Set tags on a friend (overwrites existing tags) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Friends")
	void SetFriendTags(const FString& FriendPlayFabId, const TArray<FString>& Tags);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get cached friends list */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Friends")
	TArray<FEPFFriend> GetCachedFriends() const;

	/** Check if a player is in the cached friends list */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Friends")
	bool IsFriend(const FString& PlayFabId) const;

	/** Get friend count */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Friends")
	int32 GetFriendCount() const;

	/** Find a friend by PlayFab ID (returns false if not found) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Friends")
	bool FindFriend(const FString& PlayFabId, FEPFFriend& OutFriend) const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Friends")
	FOnEPFFriendsReceived OnFriendsReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Friends")
	FOnEPFFriendModified OnFriendAdded;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Friends")
	FOnEPFFriendModified OnFriendRemoved;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Friends")
	FOnEPFFriendModified OnFriendTagsUpdated;

private:

	TArray<FEPFFriend> CachedFriends;
};
