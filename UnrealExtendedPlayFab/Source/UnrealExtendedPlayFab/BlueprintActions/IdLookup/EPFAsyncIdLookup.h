// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "IdLookup/EPFIdLookupSubsystem.h"
#include "EPFAsyncIdLookup.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncIdLookupSuccess, const TArray<FEPFIdMapping>&, Mappings);

UCLASS(meta = (DisplayName = "PlayFab: ID Lookup"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncIdLookup : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get PlayFab IDs from Steam IDs"), Category = "PlayFab|Async|IdLookup")
	static UEPFAsyncIdLookup* FromSteamIDs(UObject* WorldContext, const TArray<FString>& SteamIds);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get PlayFab IDs from Custom IDs"), Category = "PlayFab|Async|IdLookup")
	static UEPFAsyncIdLookup* FromCustomIDs(UObject* WorldContext, const TArray<FString>& CustomIds);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get PlayFab IDs from Xbox Live IDs"), Category = "PlayFab|Async|IdLookup")
	static UEPFAsyncIdLookup* FromXboxLiveIDs(UObject* WorldContext, const TArray<FString>& XboxLiveIds);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get PlayFab IDs from PSN Account IDs"), Category = "PlayFab|Async|IdLookup")
	static UEPFAsyncIdLookup* FromPSNAccountIDs(UObject* WorldContext, const TArray<FString>& PsnAccountIds);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncIdLookupSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;

	virtual void Activate() override;

private:
	enum ELookupMethod { Steam, Custom, Xbox, PSN };
	ELookupMethod Method;
	TArray<FString> Ids;
	TWeakObjectPtr<UObject> WorldContext;

	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFIdMapping>& Mappings);
};
