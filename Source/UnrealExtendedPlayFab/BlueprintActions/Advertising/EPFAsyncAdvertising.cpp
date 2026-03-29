// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncAdvertising.h"
#include "Engine/GameInstance.h"


// ── Attribute Install ────────────────────────────────────────────────────────

UEPFAsyncAttributeInstall* UEPFAsyncAttributeInstall::AttributeInstall(UObject* WorldContext, const FString& AdvertisingIdType, const FString& AdvertisingId)
{
	auto* Action = NewObject<UEPFAsyncAttributeInstall>();
	Action->IdType = AdvertisingIdType;
	Action->IdValue = AdvertisingId;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncAttributeInstall::Activate()
{
	auto* Sub = GetEPFSubsystemFromContext<UEPFAdvertisingSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Advertising subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnInstallAttributed.AddDynamic(this, &UEPFAsyncAttributeInstall::HandleComplete);
	Sub->AttributeInstall(IdType, IdValue);
}

void UEPFAsyncAttributeInstall::HandleComplete(const FEPFResult& Result)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFAdvertisingSubsystem>(WorldContext.Get()))
		Sub->OnInstallAttributed.RemoveDynamic(this, &UEPFAsyncAttributeInstall::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to attribute install")));
	SetReadyToDestroy();
}


// ── Get Ad Placements ────────────────────────────────────────────────────────

UEPFAsyncGetAdPlacements* UEPFAsyncGetAdPlacements::GetAdPlacements(UObject* WorldContext, const FString& AppId, const FString& Identifier)
{
	auto* Action = NewObject<UEPFAsyncGetAdPlacements>();
	Action->AppId = AppId;
	Action->Identifier = Identifier;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncGetAdPlacements::Activate()
{
	auto* Sub = GetEPFSubsystemFromContext<UEPFAdvertisingSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Advertising subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnAdPlacementsReceived.AddDynamic(this, &UEPFAsyncGetAdPlacements::HandleComplete);
	Sub->GetAdPlacements(AppId, Identifier);
}

void UEPFAsyncGetAdPlacements::HandleComplete(const FEPFResult& Result, const TArray<FEPFAdPlacement>& Placements)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFAdvertisingSubsystem>(WorldContext.Get()))
		Sub->OnAdPlacementsReceived.RemoveDynamic(this, &UEPFAsyncGetAdPlacements::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Placements) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get ad placements")));
	SetReadyToDestroy();
}
