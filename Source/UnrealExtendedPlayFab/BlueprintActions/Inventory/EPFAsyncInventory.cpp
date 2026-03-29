// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncInventory.h"
#include "Engine/GameInstance.h"

// ── Get Inventory ────────────────────────────────────────────────────────────

UEPFAsyncGetInventory* UEPFAsyncGetInventory::GetInventory(UObject* WorldContext)
{
	auto* Action = NewObject<UEPFAsyncGetInventory>();
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncGetInventory::Activate()
{
	auto* Sub = GetEPFSubsystemFromContext<UEPFInventorySubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Inventory subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnInventoryReceived.AddDynamic(this, &UEPFAsyncGetInventory::HandleComplete);
	Sub->GetInventory();
}

void UEPFAsyncGetInventory::HandleComplete(const FEPFResult& Result, const TArray<FEPFInventoryItem>& Items)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFInventorySubsystem>(WorldContext.Get()))
		Sub->OnInventoryReceived.RemoveDynamic(this, &UEPFAsyncGetInventory::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Items) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get inventory")));
	SetReadyToDestroy();
}

// ── Get Catalog ──────────────────────────────────────────────────────────────

UEPFAsyncGetCatalog* UEPFAsyncGetCatalog::GetCatalog(UObject* WorldContext, const FString& CatalogVersion)
{
	auto* Action = NewObject<UEPFAsyncGetCatalog>();
	Action->CatalogVersion = CatalogVersion;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncGetCatalog::Activate()
{
	auto* Sub = GetEPFSubsystemFromContext<UEPFInventorySubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Inventory subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnCatalogReceived.AddDynamic(this, &UEPFAsyncGetCatalog::HandleComplete);
	Sub->GetCatalog(CatalogVersion);
}

void UEPFAsyncGetCatalog::HandleComplete(const FEPFResult& Result, const TArray<FEPFCatalogItem>& Items)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFInventorySubsystem>(WorldContext.Get()))
		Sub->OnCatalogReceived.RemoveDynamic(this, &UEPFAsyncGetCatalog::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Items) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get catalog")));
	SetReadyToDestroy();
}

// ── Purchase Item ────────────────────────────────────────────────────────────

UEPFAsyncPurchaseItem* UEPFAsyncPurchaseItem::PurchaseItem(UObject* WorldContext, const FString& ItemId, const FString& CurrencyCode, int32 Price)
{
	auto* Action = NewObject<UEPFAsyncPurchaseItem>();
	Action->ItemId = ItemId;
	Action->CurrencyCode = CurrencyCode;
	Action->Price = Price;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncPurchaseItem::Activate()
{
	if (ItemId.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("ItemId cannot be empty"))); SetReadyToDestroy(); return; }
	if (CurrencyCode.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("CurrencyCode cannot be empty"))); SetReadyToDestroy(); return; }
	auto* Sub = GetEPFSubsystemFromContext<UEPFInventorySubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Inventory subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnItemPurchased.AddDynamic(this, &UEPFAsyncPurchaseItem::HandleComplete);
	Sub->PurchaseItem(ItemId, CurrencyCode, Price);
}

void UEPFAsyncPurchaseItem::HandleComplete(const FEPFResult& Result, const FString& ItemInstanceId)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFInventorySubsystem>(WorldContext.Get()))
		Sub->OnItemPurchased.RemoveDynamic(this, &UEPFAsyncPurchaseItem::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(ItemInstanceId) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to purchase item")));
	SetReadyToDestroy();
}

// ── Consume Item ─────────────────────────────────────────────────────────────

UEPFAsyncConsumeItem* UEPFAsyncConsumeItem::ConsumeItem(UObject* WorldContext, const FString& ItemInstanceId, int32 ConsumeCount)
{
	auto* Action = NewObject<UEPFAsyncConsumeItem>();
	Action->ItemInstanceId = ItemInstanceId;
	Action->ConsumeCount = FMath::Max(1, ConsumeCount);
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncConsumeItem::Activate()
{
	if (ItemInstanceId.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("ItemInstanceId cannot be empty"))); SetReadyToDestroy(); return; }
	auto* Sub = GetEPFSubsystemFromContext<UEPFInventorySubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Inventory subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnItemConsumed.AddDynamic(this, &UEPFAsyncConsumeItem::HandleComplete);
	Sub->ConsumeItem(ItemInstanceId, ConsumeCount);
}

void UEPFAsyncConsumeItem::HandleComplete(const FEPFResult& Result, int32 RemainingUses)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFInventorySubsystem>(WorldContext.Get()))
		Sub->OnItemConsumed.RemoveDynamic(this, &UEPFAsyncConsumeItem::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(RemainingUses) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to consume item")));
	SetReadyToDestroy();
}
