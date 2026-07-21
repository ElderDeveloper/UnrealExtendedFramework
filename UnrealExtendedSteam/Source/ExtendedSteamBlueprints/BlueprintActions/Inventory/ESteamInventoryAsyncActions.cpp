// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/Inventory/ESteamInventoryAsyncActions.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace ESteamInventoryAsyncActionHelpers
{
	UESteamInventorySubsystem* GetInventorySubsystem(const UObject* WorldContextObject)
	{
		if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr)
		{
			if (const UGameInstance* GameInstance = World->GetGameInstance())
			{
				return GameInstance->GetSubsystem<UESteamInventorySubsystem>();
			}
		}
		return nullptr;
	}
}

USteamAsyncGetAllInventoryItems* USteamAsyncGetAllInventoryItems::GetAllInventoryItems(UObject* WorldContext, float Timeout)
{
	USteamAsyncGetAllInventoryItems* Action = NewObject<USteamAsyncGetAllInventoryItems>();
	Action->WorldContextObject = WorldContext;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncGetAllInventoryItems::Activate()
{
	UESteamInventorySubsystem* Subsystem = ESteamInventoryAsyncActionHelpers::GetInventorySubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, TArray<FESteamInventoryItem>());
		return;
	}

	InventorySubsystem = Subsystem;
	Subsystem->OnInventoryItemsReceived.AddDynamic(this, &USteamAsyncGetAllInventoryItems::HandleInventoryItemsReceived);

	if (!Subsystem->GetAllItems())
	{
		Complete(false, TArray<FESteamInventoryItem>());
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncGetAllInventoryItems::HandleInventoryItemsReceived(bool bSuccess, const TArray<FESteamInventoryItem>& Items)
{
	// NOTE: shared inventory delegate carries no request id; cannot discriminate. Relies on Timeout + one-in-flight subsystem limit.
	Complete(bSuccess, Items);
}

void USteamAsyncGetAllInventoryItems::OnTimeoutFailure()
{
	Complete(false, TArray<FESteamInventoryItem>());
}

void USteamAsyncGetAllInventoryItems::Complete(bool bSuccess, const TArray<FESteamInventoryItem>& Items)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamInventorySubsystem* Subsystem = InventorySubsystem.Get())
	{
		Subsystem->OnInventoryItemsReceived.RemoveDynamic(this, &USteamAsyncGetAllInventoryItems::HandleInventoryItemsReceived);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Items);
	}
	else
	{
		OnFailure.Broadcast(Items);
	}

	SetReadyToDestroy();
}

// ---- RequestInventoryPrices ----

USteamAsyncRequestInventoryPrices* USteamAsyncRequestInventoryPrices::RequestInventoryPrices(UObject* WorldContext, float Timeout)
{
	USteamAsyncRequestInventoryPrices* Action = NewObject<USteamAsyncRequestInventoryPrices>();
	Action->WorldContextObject = WorldContext;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncRequestInventoryPrices::Activate()
{
	UESteamInventorySubsystem* Subsystem = ESteamInventoryAsyncActionHelpers::GetInventorySubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, FString());
		return;
	}

	InventorySubsystem = Subsystem;
	Subsystem->OnPricesReceived.AddDynamic(this, &USteamAsyncRequestInventoryPrices::HandlePricesReceived);

	if (!Subsystem->RequestPrices())
	{
		Complete(false, FString());
		return;
	}

	ArmTimeout(Timeout);
}

void USteamAsyncRequestInventoryPrices::HandlePricesReceived(bool bSuccess, FString Currency)
{
	Complete(bSuccess, Currency);
}

void USteamAsyncRequestInventoryPrices::OnTimeoutFailure()
{
	Complete(false, FString());
}

void USteamAsyncRequestInventoryPrices::Complete(bool bSuccess, const FString& Currency)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamInventorySubsystem* Subsystem = InventorySubsystem.Get())
	{
		Subsystem->OnPricesReceived.RemoveDynamic(this, &USteamAsyncRequestInventoryPrices::HandlePricesReceived);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Currency);
	}
	else
	{
		OnFailure.Broadcast(Currency);
	}

	SetReadyToDestroy();
}

// ---- StartInventoryPurchase ----

USteamAsyncStartInventoryPurchase* USteamAsyncStartInventoryPurchase::StartInventoryPurchase(UObject* WorldContext, const TArray<int32>& Definitions, const TArray<int32>& Quantities, float Timeout)
{
	USteamAsyncStartInventoryPurchase* Action = NewObject<USteamAsyncStartInventoryPurchase>();
	Action->WorldContextObject = WorldContext;
	Action->Definitions = Definitions;
	Action->Quantities = Quantities;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncStartInventoryPurchase::Activate()
{
	UESteamInventorySubsystem* Subsystem = ESteamInventoryAsyncActionHelpers::GetInventorySubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, 0, 0);
		return;
	}

	InventorySubsystem = Subsystem;
	Subsystem->OnPurchaseStarted.AddDynamic(this, &USteamAsyncStartInventoryPurchase::HandlePurchaseStarted);

	if (!Subsystem->StartPurchase(Definitions, Quantities))
	{
		Complete(false, 0, 0);
		return;
	}

	ArmTimeout(Timeout);
}

void USteamAsyncStartInventoryPurchase::HandlePurchaseStarted(bool bSuccess, int64 OrderId, int64 TransactionId)
{
	Complete(bSuccess, OrderId, TransactionId);
}

void USteamAsyncStartInventoryPurchase::OnTimeoutFailure()
{
	Complete(false, 0, 0);
}

void USteamAsyncStartInventoryPurchase::Complete(bool bSuccess, int64 OrderId, int64 TransactionId)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamInventorySubsystem* Subsystem = InventorySubsystem.Get())
	{
		Subsystem->OnPurchaseStarted.RemoveDynamic(this, &USteamAsyncStartInventoryPurchase::HandlePurchaseStarted);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(OrderId, TransactionId);
	}
	else
	{
		OnFailure.Broadcast(OrderId, TransactionId);
	}

	SetReadyToDestroy();
}

// ---- RequestEligiblePromoItemDefs ----

USteamAsyncRequestEligiblePromoItemDefs* USteamAsyncRequestEligiblePromoItemDefs::RequestEligiblePromoItemDefs(UObject* WorldContext, FESteamId SteamId, float Timeout)
{
	USteamAsyncRequestEligiblePromoItemDefs* Action = NewObject<USteamAsyncRequestEligiblePromoItemDefs>();
	Action->WorldContextObject = WorldContext;
	Action->SteamId = SteamId;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncRequestEligiblePromoItemDefs::Activate()
{
	UESteamInventorySubsystem* Subsystem = ESteamInventoryAsyncActionHelpers::GetInventorySubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, TArray<int32>());
		return;
	}

	InventorySubsystem = Subsystem;
	Subsystem->OnEligiblePromoItemDefsReceived.AddDynamic(this, &USteamAsyncRequestEligiblePromoItemDefs::HandleEligiblePromoReceived);

	if (!Subsystem->RequestEligiblePromoItemDefinitionsIDs(SteamId))
	{
		Complete(false, TArray<int32>());
		return;
	}

	ArmTimeout(Timeout);
}

void USteamAsyncRequestEligiblePromoItemDefs::HandleEligiblePromoReceived(bool bSuccess, const TArray<int32>& DefIds)
{
	Complete(bSuccess, DefIds);
}

void USteamAsyncRequestEligiblePromoItemDefs::OnTimeoutFailure()
{
	Complete(false, TArray<int32>());
}

void USteamAsyncRequestEligiblePromoItemDefs::Complete(bool bSuccess, const TArray<int32>& DefIds)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamInventorySubsystem* Subsystem = InventorySubsystem.Get())
	{
		Subsystem->OnEligiblePromoItemDefsReceived.RemoveDynamic(this, &USteamAsyncRequestEligiblePromoItemDefs::HandleEligiblePromoReceived);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(DefIds);
	}
	else
	{
		OnFailure.Broadcast(DefIds);
	}

	SetReadyToDestroy();
}
