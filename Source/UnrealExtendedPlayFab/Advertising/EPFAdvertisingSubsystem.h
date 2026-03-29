// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFAdvertisingSubsystem.generated.h"


USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFAdPlacement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Advertising")
	FString PlacementId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Advertising")
	FString PlacementName;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Advertising")
	FString RewardAssetUrl;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Advertising")
	FString RewardDescription;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Advertising")
	FString RewardId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Advertising")
	FString RewardName;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFInstallAttributed, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFAdPlacementsReceived, const FEPFResult&, Result, const TArray<FEPFAdPlacement>&, Placements);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAdActivityReported, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAdActivityRewarded, const FEPFResult&, Result);

/**
 * Advertising — install attribution, ad placements, and rewarded ads.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFAdvertisingSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Attribute an install for advertisement tracking */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Advertising")
	void AttributeInstall(const FString& AdvertisingIdType, const FString& AdvertisingId);

	/** Get available ad placements */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Advertising")
	void GetAdPlacements(const FString& AppId, const FString& Identifier = TEXT(""));

	/** Report player's ad activity */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Advertising")
	void ReportAdActivity(const FString& PlacementId, const FString& RewardId, const FString& Activity);

	/** Reward player for ad activity */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Advertising")
	void RewardAdActivity(const FString& PlacementId, const FString& RewardId);

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Advertising")
	FOnEPFInstallAttributed OnInstallAttributed;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Advertising")
	FOnEPFAdPlacementsReceived OnAdPlacementsReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Advertising")
	FOnEPFAdActivityReported OnAdActivityReported;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Advertising")
	FOnEPFAdActivityRewarded OnAdActivityRewarded;
};
