// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/ESteamAsyncActionBase.h"
#include "Social/ESteamFriendsSubsystem.h"
#include "ESteamFriendsAsyncActions.generated.h"

/** Completion pin for the request-user-information node (User echoes the request). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncUserInfoPin, FESteamId, User);

/** Completion pin for the follower-count node (Count is 0 on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamAsyncFollowerCountPin, FESteamId, User, int32, Count);

/** Completion pin for the is-following node (bIsFollowing is false on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamAsyncIsFollowingPin, FESteamId, User, bool, bIsFollowing);

/** Completion pin for the enumerate-following-list node (Users is a page, TotalCount the grand total). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamAsyncFollowingListPin, const TArray<FESteamId>&, Users, int32, TotalCount);

/**
 * Starts downloading persona information for a user and completes when Steam signals the update
 * (OnPersonaStateChanged for the id), or immediately when the data is already available locally.
 */
UCLASS()
class USteamAsyncRequestUserInformation : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Requests persona (and optionally full) information for a user the client does not know yet.
	 * @param bRequireNameOnly Fetch only the persona name (cheaper) rather than the avatar too.
	 * @param Timeout Seconds before the node fails with OnFailure if no update arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Friends", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Request User Information"))
	static USteamAsyncRequestUserInformation* RequestUserInformation(UObject* WorldContext, FESteamId User, bool bRequireNameOnly, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** Persona information for User is available. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncUserInfoPin OnSuccess;

	/** Steam is unavailable or the information never arrived; User echoes the request. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncUserInfoPin OnFailure;

private:
	UFUNCTION()
	void HandlePersonaStateChanged(FESteamId SteamId, int32 ChangeFlags);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamFriendsSubsystem> FriendsSubsystem;
	FESteamId User;
	bool bRequireNameOnly = false;
};

/**
 * Requests how many followers a user has and completes when the matching result arrives from
 * UESteamFriendsSubsystem.
 */
UCLASS()
class USteamAsyncGetFollowerCount : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Requests a user's follower count.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Friends", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Get Follower Count"))
	static USteamAsyncGetFollowerCount* GetFollowerCount(UObject* WorldContext, FESteamId User, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The follower count is ready; Count holds it. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncFollowerCountPin OnSuccess;

	/** Steam is unavailable or the request failed; Count is 0. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncFollowerCountPin OnFailure;

private:
	UFUNCTION()
	void HandleFollowerCount(bool bSuccess, FESteamId InUser, int32 Count);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, int32 Count);

	TWeakObjectPtr<UESteamFriendsSubsystem> FriendsSubsystem;
	FESteamId User;
};

/**
 * Requests whether the local user follows another user and completes when the matching result
 * arrives from UESteamFriendsSubsystem.
 */
UCLASS()
class USteamAsyncIsFollowing : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Requests whether the local user follows User.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Friends", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Is Following"))
	static USteamAsyncIsFollowing* IsFollowing(UObject* WorldContext, FESteamId User, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The result is ready; bIsFollowing tells whether the local user follows User. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncIsFollowingPin OnSuccess;

	/** Steam is unavailable or the request failed; bIsFollowing is false. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncIsFollowingPin OnFailure;

private:
	UFUNCTION()
	void HandleIsFollowing(bool bSuccess, FESteamId InUser, bool bIsFollowing);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, bool bIsFollowing);

	TWeakObjectPtr<UESteamFriendsSubsystem> FriendsSubsystem;
	FESteamId User;
};

/**
 * Enumerates the users the local user follows (one page at a time) and completes when the matching
 * result arrives from UESteamFriendsSubsystem.
 */
UCLASS()
class USteamAsyncEnumerateFollowingList : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Enumerates followed users starting at StartIndex (Steam returns up to 50 per call).
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Friends", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Enumerate Following List"))
	static USteamAsyncEnumerateFollowingList* EnumerateFollowingList(UObject* WorldContext, int32 StartIndex, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The page is ready; Users holds this page, TotalCount the grand total. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncFollowingListPin OnSuccess;

	/** Steam is unavailable or the request failed; Users is empty. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncFollowingListPin OnFailure;

private:
	UFUNCTION()
	void HandleFollowingListEnumerated(bool bSuccess, const TArray<FESteamId>& Users, int32 TotalCount);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const TArray<FESteamId>& Users, int32 TotalCount);

	TWeakObjectPtr<UESteamFriendsSubsystem> FriendsSubsystem;
	int32 StartIndex = 0;
};
