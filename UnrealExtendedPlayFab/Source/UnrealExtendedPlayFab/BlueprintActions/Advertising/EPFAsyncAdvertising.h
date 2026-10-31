// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "Advertising/EPFAdvertisingSubsystem.h"
#include "EPFAsyncAdvertising.generated.h"


// ── Attribute Install ────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Attribute Install"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncAttributeInstall : public UEPFAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Attribute Install"), Category = "PlayFab|Async|Advertising")
	static UEPFAsyncAttributeInstall* AttributeInstall(UObject* WorldContext, const FString& AdvertisingIdType, const FString& AdvertisingId);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncSimpleSuccess OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString IdType, IdValue;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};


// ── Get Ad Placements ────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncAdPlacementsSuccess, const TArray<FEPFAdPlacement>&, Placements);

UCLASS(meta = (DisplayName = "PlayFab: Get Ad Placements"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetAdPlacements : public UEPFAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Ad Placements"), Category = "PlayFab|Async|Advertising")
	static UEPFAsyncGetAdPlacements* GetAdPlacements(UObject* WorldContext, const FString& AppId, const FString& Identifier = TEXT(""));

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncAdPlacementsSuccess OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString PlacementAppId, PlacementIdentifier;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFAdPlacement>& Placements);
};
