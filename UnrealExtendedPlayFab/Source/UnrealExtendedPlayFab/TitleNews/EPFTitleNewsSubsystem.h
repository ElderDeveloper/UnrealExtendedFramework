// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFTitleNewsSubsystem.generated.h"


USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFTitleNewsItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|TitleNews")
	FString NewsId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|TitleNews")
	FString Title;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|TitleNews")
	FString Body;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|TitleNews")
	FDateTime Timestamp;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFTitleNewsReceived, const FEPFResult&, Result, const TArray<FEPFTitleNewsItem>&, NewsItems);

/**
 * Fetches in-game announcements from PlayFab Title News.
 * Manage news items from the PlayFab Game Manager dashboard — no game update needed.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFTitleNewsSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Fetch the latest title news (set count to limit results) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|TitleNews")
	void GetTitleNews(int32 Count = 10);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get cached news items from the last query */
	UFUNCTION(BlueprintPure, Category = "PlayFab|TitleNews")
	TArray<FEPFTitleNewsItem> GetCachedNews() const;

	/** Get the number of cached news items */
	UFUNCTION(BlueprintPure, Category = "PlayFab|TitleNews")
	int32 GetNewsCount() const;

	/** Check if there are any news items */
	UFUNCTION(BlueprintPure, Category = "PlayFab|TitleNews")
	bool HasNews() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|TitleNews")
	FOnEPFTitleNewsReceived OnTitleNewsReceived;

private:

	TArray<FEPFTitleNewsItem> CachedNews;
};
