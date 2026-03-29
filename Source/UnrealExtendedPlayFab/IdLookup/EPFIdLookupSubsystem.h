// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFIdLookupSubsystem.generated.h"


USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFIdMapping
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|IdLookup")
	FString PlatformId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|IdLookup")
	FString PlayFabId;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFIdLookupComplete, const FEPFResult&, Result, const TArray<FEPFIdMapping>&, Mappings);

/**
 * ID Lookup — cross-reference platform IDs to PlayFab IDs.
 * Essential for resolving friends lists from Steam, Xbox, PSN, etc.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFIdLookupSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Look up PlayFab IDs from Steam IDs */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|IdLookup")
	void GetPlayFabIDsFromSteamIDs(const TArray<FString>& SteamIds);

	/** Look up PlayFab IDs from Custom IDs */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|IdLookup")
	void GetPlayFabIDsFromCustomIDs(const TArray<FString>& CustomIds);

	/** Look up PlayFab IDs from Xbox Live IDs */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|IdLookup")
	void GetPlayFabIDsFromXboxLiveIDs(const TArray<FString>& XboxLiveIds);

	/** Look up PlayFab IDs from PSN Account IDs */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|IdLookup")
	void GetPlayFabIDsFromPSNAccountIDs(const TArray<FString>& PsnAccountIds);

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|IdLookup")
	FOnEPFIdLookupComplete OnIdLookupComplete;

private:

	/** Generic helper — builds request, sends, and parses the platform-specific response */
	void SendIdLookupRequest(const FString& Endpoint, const FString& ArrayFieldName, const TArray<FString>& Ids, const FString& PlatformIdField, const FString& ResponseArrayField);
};
