// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncAdvertising.h"


// ── Attribute Install ────────────────────────────────────────────────────────

UEPFAsyncAttributeInstall* UEPFAsyncAttributeInstall::AttributeInstall(UObject* WorldContext, const FString& AdvertisingIdType, const FString& AdvertisingId)
{
	auto* Action = NewObject<UEPFAsyncAttributeInstall>();
	Action->IdType = AdvertisingIdType;
	Action->IdValue = AdvertisingId;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncAttributeInstall::ExecuteAction()
{
	auto* Sub = RequireGameInstanceSubsystem<UEPFAdvertisingSubsystem>();
	if (!Sub) return;
	Sub->OnInstallAttributed.AddDynamic(this, &UEPFAsyncAttributeInstall::HandleComplete);
	CallWithContext([&] { Sub->AttributeInstall(IdType, IdValue); });
}

void UEPFAsyncAttributeInstall::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFAdvertisingSubsystem>())
		Sub->OnInstallAttributed.RemoveDynamic(this, &UEPFAsyncAttributeInstall::HandleComplete);
}

void UEPFAsyncAttributeInstall::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to attribute install")));
}


// ── Get Ad Placements ────────────────────────────────────────────────────────

UEPFAsyncGetAdPlacements* UEPFAsyncGetAdPlacements::GetAdPlacements(UObject* WorldContext, const FString& AppId, const FString& Identifier)
{
	auto* Action = NewObject<UEPFAsyncGetAdPlacements>();
	Action->PlacementAppId = AppId;
	Action->PlacementIdentifier = Identifier;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncGetAdPlacements::ExecuteAction()
{
	auto* Sub = RequireGameInstanceSubsystem<UEPFAdvertisingSubsystem>();
	if (!Sub) return;
	Sub->OnAdPlacementsReceived.AddDynamic(this, &UEPFAsyncGetAdPlacements::HandleComplete);
	CallWithContext([&] { Sub->GetAdPlacements(PlacementAppId, PlacementIdentifier); });
}

void UEPFAsyncGetAdPlacements::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFAdvertisingSubsystem>())
		Sub->OnAdPlacementsReceived.RemoveDynamic(this, &UEPFAsyncGetAdPlacements::HandleComplete);
}

void UEPFAsyncGetAdPlacements::HandleComplete(const FEPFResult& Result, const TArray<FEPFAdPlacement>& Placements)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Placements) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get ad placements")));
}
