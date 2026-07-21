// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "Friends/EPFFriendsSubsystem.h"
#include "EPFAsyncFriends.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncFriendsSuccess, const TArray<FEPFFriend>&, Friends);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncFriendAction);

// ── Get Friends ──────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Get Friends List"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetFriends : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Friends List"), Category = "PlayFab|Async|Friends")
	static UEPFAsyncGetFriends* GetFriendsList(UObject* WorldContext, bool bIncludeSteamFriends = true);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFriendsSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	bool bSteam; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFFriend>& Friends);
};

// ── Add Friend ───────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Add Friend"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncAddFriend : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Add Friend"), Category = "PlayFab|Async|Friends")
	static UEPFAsyncAddFriend* AddFriend(UObject* WorldContext, const FString& FriendPlayFabId);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFriendAction OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString FriendId; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};

// ── Remove Friend ────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Remove Friend"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncRemoveFriend : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Remove Friend"), Category = "PlayFab|Async|Friends")
	static UEPFAsyncRemoveFriend* RemoveFriend(UObject* WorldContext, const FString& FriendPlayFabId);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFriendAction OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString FriendId; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};

// ── Set Friend Tags ──────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Set Friend Tags"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncSetFriendTags : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Set Friend Tags"), Category = "PlayFab|Async|Friends")
	static UEPFAsyncSetFriendTags* SetFriendTags(UObject* WorldContext, const FString& FriendPlayFabId, const TArray<FString>& Tags);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFriendAction OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString FriendId; TArray<FString> Tags; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};
