// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFEntityFilesSubsystem.generated.h"


USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFEntityObject
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|EntityFiles")
	FString ObjectName;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|EntityFiles")
	FString DataAsJson;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|EntityFiles")
	FString EscapedDataValue;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|EntityFiles")
	FString LastUpdated;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFEntityObjectsReceived, const FEPFResult&, Result, const TArray<FEPFEntityObject>&, Objects);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFEntityObjectsUpdated, const FEPFResult&, Result);

/**
 * Entity Files/Objects — structured key-value storage at entity level.
 * More sophisticated than raw PlayerData, supports complex JSON objects.
 * Uses Entity API (requires Entity Token from auth).
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFEntityFilesSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Get entity objects by name */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|EntityFiles")
	void GetObjects(const FString& EntityId, const FString& EntityType = TEXT("title_player_account"));

	/** Set entity objects (key-value with JSON values) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|EntityFiles")
	void SetObjects(const FString& EntityId, const TMap<FString, FString>& Objects, const FString& EntityType = TEXT("title_player_account"));

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get cached objects */
	UFUNCTION(BlueprintPure, Category = "PlayFab|EntityFiles")
	TArray<FEPFEntityObject> GetCachedObjects() const;

	/** Find a specific cached object by name */
	UFUNCTION(BlueprintPure, Category = "PlayFab|EntityFiles")
	bool FindObject(const FString& ObjectName, FEPFEntityObject& OutObject) const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|EntityFiles")
	FOnEPFEntityObjectsReceived OnObjectsReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|EntityFiles")
	FOnEPFEntityObjectsUpdated OnObjectsUpdated;

private:

	TArray<FEPFEntityObject> CachedObjects;
};
