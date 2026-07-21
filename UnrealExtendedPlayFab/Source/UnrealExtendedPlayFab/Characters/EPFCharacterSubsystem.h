// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "Shared/EPFTypes.h"
#include "EPFCharacterSubsystem.generated.h"


USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFCharacter
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Characters")
	FString CharacterId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Characters")
	FString CharacterName;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Characters")
	FString CharacterType;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFCharactersReceived, const FEPFResult&, Result, const TArray<FEPFCharacter>&, Characters);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFCharacterGranted, const FEPFResult&, Result, const FString&, CharacterId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFCharacterDeleted, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFCharacterDataReceived, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFCharacterDataUpdated, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFCharacterStatsReceived, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFCharacterLeaderboardReceived, const FEPFResult&, Result, const TArray<FEPFLeaderboardEntry>&, Entries);

/**
 * Character Management — multiple characters per account (RPG-style).
 * Each character has its own data, stats, and inventory.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFCharacterSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Get all characters for the current player */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Characters")
	void GetAllCharacters();

	/** Grant a new character via catalog item */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Characters")
	void GrantCharacter(const FString& CharacterName, const FString& ItemId);

	/** Delete a character */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Characters")
	void DeleteCharacter(const FString& CharacterId, bool bSaveCharacterInventory = false);

	/** Get character-specific data */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Characters")
	void GetCharacterData(const FString& CharacterId, const TArray<FString>& Keys);

	/** Update character-specific data */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Characters")
	void UpdateCharacterData(const FString& CharacterId, const TMap<FString, FString>& Data);

	/** Get character statistics */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Characters")
	void GetCharacterStatistics(const FString& CharacterId);

	/** Update character statistics */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Characters")
	void UpdateCharacterStatistics(const FString& CharacterId, const TMap<FString, int32>& Stats);

	/** Get character read-only data (server-set) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Characters")
	void GetCharacterReadOnlyData(const FString& CharacterId, const TArray<FString>& Keys);

	/** Get global character leaderboard for a statistic */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Characters")
	void GetCharacterLeaderboard(const FString& StatisticName, int32 StartPosition = 0, int32 MaxResultsCount = 10);

	/** Get leaderboard centered on a specific character */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Characters")
	void GetLeaderboardAroundCharacter(const FString& StatisticName, const FString& CharacterId, int32 MaxResultsCount = 10);

	/** Get leaderboard for all of the current user's characters */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Characters")
	void GetLeaderboardForUserCharacters(const FString& StatisticName);

	// ── Queries ──────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "PlayFab|Characters")
	TArray<FEPFCharacter> GetCachedCharacters() const;

	UFUNCTION(BlueprintPure, Category = "PlayFab|Characters")
	int32 GetCharacterCount() const;

	UFUNCTION(BlueprintPure, Category = "PlayFab|Characters")
	bool FindCharacter(const FString& CharacterId, FEPFCharacter& OutCharacter) const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Characters")
	FOnEPFCharactersReceived OnCharactersReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Characters")
	FOnEPFCharacterGranted OnCharacterGranted;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Characters")
	FOnEPFCharacterDeleted OnCharacterDeleted;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Characters")
	FOnEPFCharacterDataReceived OnCharacterDataReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Characters")
	FOnEPFCharacterDataUpdated OnCharacterDataUpdated;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Characters")
	FOnEPFCharacterStatsReceived OnCharacterStatsReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Characters")
	FOnEPFCharacterLeaderboardReceived OnCharacterLeaderboardReceived;

private:

	TArray<FEPFCharacter> CachedCharacters;

	/** Shared parser for character leaderboard responses */
	void ParseCharacterLeaderboard(const FEPFResult& Result, TSharedPtr<FJsonObject> Response);
};
