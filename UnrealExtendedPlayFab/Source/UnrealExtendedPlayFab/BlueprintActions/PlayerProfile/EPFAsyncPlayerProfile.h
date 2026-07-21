// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "PlayerProfile/EPFPlayerProfileSubsystem.h"
#include "EPFAsyncPlayerProfile.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncProfileSuccess, const FEPFPlayerProfile&, Profile);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncAccountInfoSuccess, const FEPFAccountInfo&, AccountInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncAvatarSuccess);

// ── Get Profile ──────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Get Player Profile"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetProfile : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	/**
	 * Fetch a player's profile from PlayFab.
	 *
	 * Which fields are returned is configured globally in
	 * Project Settings → Extended Framework → Extended PlayFab → Profile Constraints.
	 *
	 * @param PlayFabId  Leave empty to fetch the current player's own profile.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Player Profile"), Category = "PlayFab|Async|Profile")
	static UEPFAsyncGetProfile* GetPlayerProfile(UObject* WorldContext, const FString& PlayFabId = TEXT(""));
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncProfileSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString TargetId; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FEPFPlayerProfile& Profile);
};

// ── Get Account Info ─────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Get Account Info"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetAccountInfo : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	/**
	 * Fetch full account info for a player — includes Steam persona name,
	 * linked platforms, title display name, and creation/login timestamps.
	 *
	 * Prefer this over GetPlayerProfile when you need the Steam username,
	 * since PlayFab's profile DisplayName is empty until explicitly set.
	 *
	 * @param PlayFabId  Leave empty to fetch the current player's own account info.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Account Info"), Category = "PlayFab|Async|Profile")
	static UEPFAsyncGetAccountInfo* GetAccountInfo(UObject* WorldContext, const FString& PlayFabId = TEXT(""));
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncAccountInfoSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString TargetId; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FEPFAccountInfo& AccountInfo);
};

// ── Get Combined Info ────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Get Combined Info"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetCombinedInfo : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Combined Info"), Category = "PlayFab|Async|Profile")
	static UEPFAsyncGetCombinedInfo* GetCombinedInfo(UObject* WorldContext, bool bGetStats = true, bool bGetPlayerData = true, bool bGetCurrency = true);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncProfileSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	bool bStats; bool bData; bool bCurrency; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FEPFPlayerProfile& Profile);
};

// ── Update Avatar ────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Update Avatar URL"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncUpdateAvatar : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Update Avatar URL"), Category = "PlayFab|Async|Profile")
	static UEPFAsyncUpdateAvatar* UpdateAvatarUrl(UObject* WorldContext, const FString& AvatarUrl);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncAvatarSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString Url; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};
