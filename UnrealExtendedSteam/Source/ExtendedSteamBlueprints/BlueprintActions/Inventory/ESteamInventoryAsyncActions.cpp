// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/Inventory/ESteamInventoryAsyncActions.h"

USteamAsyncGetAllInventoryItems* USteamAsyncGetAllInventoryItems::GetAllInventoryItems(UObject* WorldContext, float Timeout)
{
	USteamAsyncGetAllInventoryItems* Action = NewObject<USteamAsyncGetAllInventoryItems>();
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncGetAllInventoryItems::ExecuteAction()
{
	UESteamInventorySubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamInventorySubsystem>();
	if (!Subsystem)
	{
		return;
	}

	InventorySubsystem = Subsystem;
	Subsystem->OnInventoryItemsReceived.AddDynamic(this, &USteamAsyncGetAllInventoryItems::HandleInventoryItemsReceived);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->GetAllItems(); });
	if (!bStarted)
	{
		Complete(false, TArray<FESteamInventoryItem>());
	}
}

void USteamAsyncGetAllInventoryItems::HandleInventoryItemsReceived(bool bSuccess, const TArray<FESteamInventoryItem>& Items)
{
	// The shared inventory delegate carries no request id. The subsystem tags results of calls made
	// inside another node's call context, so those are skipped; untagged results (direct calls,
	// Steam-initiated updates) are still accepted.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, Items);
}

void USteamAsyncGetAllInventoryItems::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(TArray<FESteamInventoryItem>());
}

void USteamAsyncGetAllInventoryItems::CancelWork()
{
	if (UESteamInventorySubsystem* Subsystem = InventorySubsystem.Get())
	{
		Subsystem->OnInventoryItemsReceived.RemoveDynamic(this, &USteamAsyncGetAllInventoryItems::HandleInventoryItemsReceived);
	}
}

void USteamAsyncGetAllInventoryItems::Complete(bool bSuccess, const TArray<FESteamInventoryItem>& Items)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Items);
	}
	else
	{
		OnFailure.Broadcast(Items);
	}
}

// ---- RequestInventoryPrices ----

USteamAsyncRequestInventoryPrices* USteamAsyncRequestInventoryPrices::RequestInventoryPrices(UObject* WorldContext, float Timeout)
{
	USteamAsyncRequestInventoryPrices* Action = NewObject<USteamAsyncRequestInventoryPrices>();
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncRequestInventoryPrices::ExecuteAction()
{
	UESteamInventorySubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamInventorySubsystem>();
	if (!Subsystem)
	{
		return;
	}

	InventorySubsystem = Subsystem;
	Subsystem->OnPricesReceived.AddDynamic(this, &USteamAsyncRequestInventoryPrices::HandlePricesReceived);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->RequestPrices(); });
	if (!bStarted)
	{
		Complete(false, FString());
	}
}

void USteamAsyncRequestInventoryPrices::HandlePricesReceived(bool bSuccess, FString Currency)
{
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, Currency);
}

void USteamAsyncRequestInventoryPrices::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(FString());
}

void USteamAsyncRequestInventoryPrices::CancelWork()
{
	if (UESteamInventorySubsystem* Subsystem = InventorySubsystem.Get())
	{
		Subsystem->OnPricesReceived.RemoveDynamic(this, &USteamAsyncRequestInventoryPrices::HandlePricesReceived);
	}
}

void USteamAsyncRequestInventoryPrices::Complete(bool bSuccess, const FString& Currency)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Currency);
	}
	else
	{
		OnFailure.Broadcast(Currency);
	}
}

// ---- StartInventoryPurchase ----

USteamAsyncStartInventoryPurchase* USteamAsyncStartInventoryPurchase::StartInventoryPurchase(UObject* WorldContext, const TArray<int32>& Definitions, const TArray<int32>& Quantities, float Timeout)
{
	USteamAsyncStartInventoryPurchase* Action = NewObject<USteamAsyncStartInventoryPurchase>();
	Action->Definitions = Definitions;
	Action->Quantities = Quantities;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncStartInventoryPurchase::ExecuteAction()
{
	UESteamInventorySubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamInventorySubsystem>();
	if (!Subsystem)
	{
		return;
	}

	InventorySubsystem = Subsystem;
	Subsystem->OnPurchaseStarted.AddDynamic(this, &USteamAsyncStartInventoryPurchase::HandlePurchaseStarted);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->StartPurchase(Definitions, Quantities); });
	if (!bStarted)
	{
		Complete(false, 0, 0);
	}
}

void USteamAsyncStartInventoryPurchase::HandlePurchaseStarted(bool bSuccess, int64 OrderId, int64 TransactionId)
{
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, OrderId, TransactionId);
}

void USteamAsyncStartInventoryPurchase::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(0, 0);
}

void USteamAsyncStartInventoryPurchase::CancelWork()
{
	if (UESteamInventorySubsystem* Subsystem = InventorySubsystem.Get())
	{
		Subsystem->OnPurchaseStarted.RemoveDynamic(this, &USteamAsyncStartInventoryPurchase::HandlePurchaseStarted);
	}
}

void USteamAsyncStartInventoryPurchase::Complete(bool bSuccess, int64 OrderId, int64 TransactionId)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(OrderId, TransactionId);
	}
	else
	{
		OnFailure.Broadcast(OrderId, TransactionId);
	}
}

// ---- RequestEligiblePromoItemDefs ----

USteamAsyncRequestEligiblePromoItemDefs* USteamAsyncRequestEligiblePromoItemDefs::RequestEligiblePromoItemDefs(UObject* WorldContext, FESteamId SteamId, float Timeout)
{
	USteamAsyncRequestEligiblePromoItemDefs* Action = NewObject<USteamAsyncRequestEligiblePromoItemDefs>();
	Action->SteamId = SteamId;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncRequestEligiblePromoItemDefs::ExecuteAction()
{
	UESteamInventorySubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamInventorySubsystem>();
	if (!Subsystem)
	{
		return;
	}

	InventorySubsystem = Subsystem;
	Subsystem->OnEligiblePromoItemDefsReceived.AddDynamic(this, &USteamAsyncRequestEligiblePromoItemDefs::HandleEligiblePromoReceived);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->RequestEligiblePromoItemDefinitionsIDs(SteamId); });
	if (!bStarted)
	{
		Complete(false, TArray<int32>());
	}
}

void USteamAsyncRequestEligiblePromoItemDefs::HandleEligiblePromoReceived(bool bSuccess, const TArray<int32>& DefIds)
{
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, DefIds);
}

void USteamAsyncRequestEligiblePromoItemDefs::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(TArray<int32>());
}

void USteamAsyncRequestEligiblePromoItemDefs::CancelWork()
{
	if (UESteamInventorySubsystem* Subsystem = InventorySubsystem.Get())
	{
		Subsystem->OnEligiblePromoItemDefsReceived.RemoveDynamic(this, &USteamAsyncRequestEligiblePromoItemDefs::HandleEligiblePromoReceived);
	}
}

void USteamAsyncRequestEligiblePromoItemDefs::Complete(bool bSuccess, const TArray<int32>& DefIds)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(DefIds);
	}
	else
	{
		OnFailure.Broadcast(DefIds);
	}
}
