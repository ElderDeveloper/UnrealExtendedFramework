// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncInventory.h"

// ── Get Inventory ────────────────────────────────────────────────────────────

UEPFAsyncGetInventory* UEPFAsyncGetInventory::GetInventory(UObject* WorldContext)
{
	auto* Action = NewObject<UEPFAsyncGetInventory>();
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncGetInventory::ExecuteAction()
{
	auto* Sub = RequireGameInstanceSubsystem<UEPFInventorySubsystem>();
	if (!Sub) return;
	Sub->OnInventoryReceived.AddDynamic(this, &UEPFAsyncGetInventory::HandleComplete);
	CallWithContext([&] { Sub->GetInventory(); });
}

void UEPFAsyncGetInventory::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFInventorySubsystem>())
		Sub->OnInventoryReceived.RemoveDynamic(this, &UEPFAsyncGetInventory::HandleComplete);
}

void UEPFAsyncGetInventory::HandleComplete(const FEPFResult& Result, const TArray<FEPFInventoryItem>& Items)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Items) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get inventory")));
}

// ── Get Catalog ──────────────────────────────────────────────────────────────

UEPFAsyncGetCatalog* UEPFAsyncGetCatalog::GetCatalog(UObject* WorldContext, const FString& CatalogVersion)
{
	auto* Action = NewObject<UEPFAsyncGetCatalog>();
	Action->RequestedCatalogVersion = CatalogVersion;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncGetCatalog::ExecuteAction()
{
	auto* Sub = RequireGameInstanceSubsystem<UEPFInventorySubsystem>();
	if (!Sub) return;
	Sub->OnCatalogReceived.AddDynamic(this, &UEPFAsyncGetCatalog::HandleComplete);
	CallWithContext([&] { Sub->GetCatalog(RequestedCatalogVersion); });
}

void UEPFAsyncGetCatalog::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFInventorySubsystem>())
		Sub->OnCatalogReceived.RemoveDynamic(this, &UEPFAsyncGetCatalog::HandleComplete);
}

void UEPFAsyncGetCatalog::HandleComplete(const FEPFResult& Result, const TArray<FEPFCatalogItem>& Items)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Items) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get catalog")));
}

// ── Purchase Item ────────────────────────────────────────────────────────────

UEPFAsyncPurchaseItem* UEPFAsyncPurchaseItem::PurchaseItem(UObject* WorldContext, const FString& ItemId, const FString& CurrencyCode, int32 Price)
{
	auto* Action = NewObject<UEPFAsyncPurchaseItem>();
	Action->PurchaseItemId = ItemId;
	Action->PurchaseCurrencyCode = CurrencyCode;
	Action->PurchasePrice = Price;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncPurchaseItem::ExecuteAction()
{
	if (PurchaseItemId.IsEmpty()) { FailInvalidArgument(TEXT("ItemId cannot be empty")); return; }
	if (PurchaseCurrencyCode.IsEmpty()) { FailInvalidArgument(TEXT("CurrencyCode cannot be empty")); return; }
	auto* Sub = RequireGameInstanceSubsystem<UEPFInventorySubsystem>();
	if (!Sub) return;
	Sub->OnItemPurchased.AddDynamic(this, &UEPFAsyncPurchaseItem::HandleComplete);
	CallWithContext([&] { Sub->PurchaseItem(PurchaseItemId, PurchaseCurrencyCode, PurchasePrice); });
}

void UEPFAsyncPurchaseItem::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFInventorySubsystem>())
		Sub->OnItemPurchased.RemoveDynamic(this, &UEPFAsyncPurchaseItem::HandleComplete);
}

void UEPFAsyncPurchaseItem::HandleComplete(const FEPFResult& Result, const FString& ItemInstanceId)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(ItemInstanceId) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to purchase item")));
}

// ── Consume Item ─────────────────────────────────────────────────────────────

UEPFAsyncConsumeItem* UEPFAsyncConsumeItem::ConsumeItem(UObject* WorldContext, const FString& ItemInstanceId, int32 ConsumeCount)
{
	auto* Action = NewObject<UEPFAsyncConsumeItem>();
	Action->ConsumedInstanceId = ItemInstanceId;
	Action->ConsumeAmount = FMath::Max(1, ConsumeCount);
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncConsumeItem::ExecuteAction()
{
	if (ConsumedInstanceId.IsEmpty()) { FailInvalidArgument(TEXT("ItemInstanceId cannot be empty")); return; }
	auto* Sub = RequireGameInstanceSubsystem<UEPFInventorySubsystem>();
	if (!Sub) return;
	Sub->OnItemConsumed.AddDynamic(this, &UEPFAsyncConsumeItem::HandleComplete);
	CallWithContext([&] { Sub->ConsumeItem(ConsumedInstanceId, ConsumeAmount); });
}

void UEPFAsyncConsumeItem::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFInventorySubsystem>())
		Sub->OnItemConsumed.RemoveDynamic(this, &UEPFAsyncConsumeItem::HandleComplete);
}

void UEPFAsyncConsumeItem::HandleComplete(const FEPFResult& Result, int32 RemainingUses)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(RemainingUses) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to consume item")));
}
