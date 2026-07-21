// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFPlayerProfileSubsystem.generated.h"


USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFPlayerProfile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Profile")
	FString PlayFabId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Profile")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Profile")
	FString AvatarUrl;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Profile")
	FString Created;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Profile")
	FString LastLogin;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Profile")
	TMap<FString, int32> Statistics;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Profile")
	TMap<FString, FString> PlayerData;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Profile")
	TArray<FString> LinkedAccounts;
};


/**
 * Controls which profile fields PlayFab returns in GetPlayerProfile.
 * Only set fields to true that your title has enabled in the PlayFab dashboard
 * under Settings > Client Profile Constraints — otherwise the API returns
 * RequestViewConstraintParamsNotAllowed (error 1303).
 *
 * All fields default to false so callers opt-in to only what they need.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFProfileConstraints
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayFab|Profile")
	bool bShowDisplayName = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayFab|Profile")
	bool bShowAvatarUrl = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayFab|Profile")
	bool bShowCreated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayFab|Profile")
	bool bShowLastLogin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayFab|Profile")
	bool bShowStatistics = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayFab|Profile")
	bool bShowLinkedAccounts = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayFab|Profile")
	bool bShowOrigination = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayFab|Profile")
	bool bShowBannedUntil = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayFab|Profile")
	bool bShowCampaignAttributions = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayFab|Profile")
	bool bShowPushNotificationRegistrations = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayFab|Profile")
	bool bShowContactEmailAddresses = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayFab|Profile")
	bool bShowTags = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayFab|Profile")
	bool bShowLocations = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayFab|Profile")
	bool bShowMemberships = false;
};


USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFAccountInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Profile")
	FString PlayFabId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Profile")
	FString TitleDisplayName;

	/** Steam persona name (SteamInfo.SteamPersonaName), if this account is Steam-linked.
	 *  Populated by GetAccountInfo. Empty for non-Steam accounts. */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Profile")
	FString SteamName;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Profile")
	FString Created;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Profile")
	FString LastLogin;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Profile")
	FString Origination;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Profile")
	TArray<FString> LinkedAccounts;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFProfileReceived, const FEPFResult&, Result, const FEPFPlayerProfile&, Profile);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFCombinedInfoReceived, const FEPFResult&, Result, const FEPFPlayerProfile&, Profile);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAvatarUpdated, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFAccountInfoReceived, const FEPFResult&, Result, const FEPFAccountInfo&, AccountInfo);

/**
 * Player Profiles — retrieve detailed player info and combined data in a single call.
 * GetCombinedInfo is particularly useful for loading screens where you need
 * stats + data + currency + inventory all at once.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFPlayerProfileSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Get the current player's profile.
	 *  The fields returned are controlled by the Profile Constraints configured
	 *  in Project Settings → Extended Framework → Extended PlayFab.
	 *  Leave PlayFabId empty to fetch the current player's own profile.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Profile")
	void GetPlayerProfile(const FString& PlayFabId = TEXT(""));

	/**
	 * Get combined player info in a single API call — stats, data, currency, etc.
	 * Much more efficient than calling each subsystem individually on login.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Profile")
	void GetPlayerCombinedInfo(bool bGetStats = true, bool bGetPlayerData = true, bool bGetVirtualCurrency = true, bool bGetInventory = false);

	/** Update the player's avatar URL */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Profile")
	void UpdateAvatarUrl(const FString& AvatarUrl);

	/** Get full account info (creation date, origination, linked accounts) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Profile")
	void GetAccountInfo(const FString& PlayFabId = TEXT(""));

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the cached profile */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Profile")
	FEPFPlayerProfile GetCachedProfile() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Profile")
	FOnEPFProfileReceived OnProfileReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Profile")
	FOnEPFCombinedInfoReceived OnCombinedInfoReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Profile")
	FOnEPFAvatarUpdated OnAvatarUpdated;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Profile")
	FOnEPFAccountInfoReceived OnAccountInfoReceived;

private:

	FEPFPlayerProfile CachedProfile;
};
