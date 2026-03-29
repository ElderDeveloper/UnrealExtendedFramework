// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFInventorySubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

void UEPFInventorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFInventorySubsystem::Deinitialize()
{
	CachedInventory.Empty();
	CachedCatalog.Empty();
	Super::Deinitialize();
}


// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Get Inventory
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void UEPFInventorySubsystem::GetInventory()
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetUserInventory"),
		Body,
		true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				CachedInventory.Empty();
				const TArray<TSharedPtr<FJsonValue>>* ItemsArr = nullptr;
				if (Response->TryGetArrayField(TEXT("Inventory"), ItemsArr) && ItemsArr)
				{
					for (const auto& ItemVal : *ItemsArr)
					{
						const TSharedPtr<FJsonObject>* ItemObj = nullptr;
						if (ItemVal->TryGetObject(ItemObj) && ItemObj)
						{
							FEPFInventoryItem Item;
							(*ItemObj)->TryGetStringField(TEXT("ItemInstanceId"), Item.ItemInstanceId);
							(*ItemObj)->TryGetStringField(TEXT("ItemId"), Item.ItemId);
							// DisplayName and ItemClass are optional in PlayFab inventory responses.
							(*ItemObj)->TryGetStringField(TEXT("DisplayName"), Item.DisplayName);
							(*ItemObj)->TryGetStringField(TEXT("ItemClass"), Item.ItemClass);

							int32 Uses = -1;
							if ((*ItemObj)->TryGetNumberField(TEXT("RemainingUses"), Uses))
							{
								Item.RemainingUses = Uses;
							}

							const TSharedPtr<FJsonObject>* CustomDataObj = nullptr;
							if ((*ItemObj)->TryGetObjectField(TEXT("CustomData"), CustomDataObj) && CustomDataObj)
							{
								for (const auto& Pair : (*CustomDataObj)->Values)
								{
									FString Value;
									if (Pair.Value->TryGetString(Value))
									{
										Item.CustomData.Add(Pair.Key, Value);
									}
								}
							}

							CachedInventory.Add(Item);
						}
					}
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFInventorySubsystem — Fetched %d inventory items"), CachedInventory.Num());
				OnInventoryReceived.Broadcast(Result, CachedInventory);
			}
			else
			{
				UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFInventorySubsystem — Failed to fetch inventory"));
				OnInventoryReceived.Broadcast(Result, CachedInventory);
			}
		})
	);
}


// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Get Catalog
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void UEPFInventorySubsystem::GetCatalog(const FString& CatalogVersion)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	if (!CatalogVersion.IsEmpty())
	{
		Body->SetStringField(TEXT("CatalogVersion"), CatalogVersion);
	}

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetCatalogItems"),
		Body,
		true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				CachedCatalog.Empty();
				const TArray<TSharedPtr<FJsonValue>>* CatalogArr = nullptr;
				if (Response->TryGetArrayField(TEXT("Catalog"), CatalogArr) && CatalogArr)
				{
					for (const auto& CatVal : *CatalogArr)
					{
						const TSharedPtr<FJsonObject>* CatObj = nullptr;
						if (CatVal->TryGetObject(CatObj) && CatObj)
						{
							FEPFCatalogItem Item;
							(*CatObj)->TryGetStringField(TEXT("ItemId"), Item.ItemId);
							// DisplayName, Description, and ItemClass are optional catalog fields.
							(*CatObj)->TryGetStringField(TEXT("DisplayName"), Item.DisplayName);
							(*CatObj)->TryGetStringField(TEXT("Description"), Item.Description);
							(*CatObj)->TryGetStringField(TEXT("ItemClass"), Item.ItemClass);

							const TSharedPtr<FJsonObject>* PricesObj = nullptr;
							if ((*CatObj)->TryGetObjectField(TEXT("VirtualCurrencyPrices"), PricesObj) && PricesObj)
							{
								for (const auto& Pair : (*PricesObj)->Values)
								{
									int32 Price = 0;
									if (Pair.Value->TryGetNumber(Price))
									{
										Item.VirtualCurrencyPrices.Add(Pair.Key, Price);
									}
								}
							}

							CachedCatalog.Add(Item);
						}
					}
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFInventorySubsystem — Fetched %d catalog items"), CachedCatalog.Num());
				OnCatalogReceived.Broadcast(Result, CachedCatalog);
			}
			else
			{
				UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFInventorySubsystem — Failed to fetch catalog"));
				OnCatalogReceived.Broadcast(Result, CachedCatalog);
			}
		})
	);
}


// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Purchase Item
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void UEPFInventorySubsystem::PurchaseItem(const FString& ItemId, const FString& CurrencyCode, int32 Price, const FString& CatalogVersion)
{
	if (ItemId.IsEmpty())
	{
		UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFInventorySubsystem::PurchaseItem — ItemId cannot be empty"));
		OnItemPurchased.Broadcast(FEPFResult::Failure(TEXT("ItemId cannot be empty")), TEXT(""));
		return;
	}
	if (CurrencyCode.IsEmpty())
	{
		UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFInventorySubsystem::PurchaseItem — CurrencyCode cannot be empty"));
		OnItemPurchased.Broadcast(FEPFResult::Failure(TEXT("CurrencyCode cannot be empty")), TEXT(""));
		return;
	}
	if (Price < 0)
	{
		UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFInventorySubsystem::PurchaseItem — Price cannot be negative"));
		OnItemPurchased.Broadcast(FEPFResult::Failure(TEXT("Price cannot be negative")), TEXT(""));
		return;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("ItemId"), ItemId);
	Body->SetStringField(TEXT("VirtualCurrency"), CurrencyCode);
	Body->SetNumberField(TEXT("Price"), Price);
	if (!CatalogVersion.IsEmpty())
	{
		Body->SetStringField(TEXT("CatalogVersion"), CatalogVersion);
	}

	SendPlayFabRequestDetailed(
		TEXT("/Client/PurchaseItem"),
		Body,
		true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			FString InstanceId;
			if (Result.bSuccess && Response.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* ItemsArr = nullptr;
				if (Response->TryGetArrayField(TEXT("Items"), ItemsArr) && ItemsArr && ItemsArr->Num() > 0)
				{
					const TSharedPtr<FJsonObject>* FirstItem = nullptr;
					if ((*ItemsArr)[0]->TryGetObject(FirstItem) && FirstItem)
					{
						InstanceId = (*FirstItem)->GetStringField(TEXT("ItemInstanceId"));
					}
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFInventorySubsystem — Purchased item, instance: %s"), *InstanceId);
			}
			OnItemPurchased.Broadcast(Result, InstanceId);
		})
	);
}


// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Consume Item
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void UEPFInventorySubsystem::ConsumeItem(const FString& ItemInstanceId, int32 ConsumeCount)
{
	if (ItemInstanceId.IsEmpty())
	{
		UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFInventorySubsystem::ConsumeItem — ItemInstanceId cannot be empty"));
		OnItemConsumed.Broadcast(FEPFResult::Failure(TEXT("ItemInstanceId cannot be empty")), -1);
		return;
	}
	if (ConsumeCount <= 0)
	{
		UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFInventorySubsystem::ConsumeItem — ConsumeCount must be positive"));
		OnItemConsumed.Broadcast(FEPFResult::Failure(TEXT("ConsumeCount must be positive")), -1);
		return;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("ItemInstanceId"), ItemInstanceId);
	Body->SetNumberField(TEXT("ConsumeCount"), ConsumeCount);

	SendPlayFabRequestDetailed(
		TEXT("/Client/ConsumeItem"),
		Body,
		true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			int32 RemainingUses = -1;
			if (Result.bSuccess && Response.IsValid())
			{
				Response->TryGetNumberField(TEXT("RemainingUses"), RemainingUses);
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFInventorySubsystem — Consumed item, remaining: %d"), RemainingUses);
			}
			OnItemConsumed.Broadcast(Result, RemainingUses);
		})
	);
}


// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Queries
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

TArray<FEPFInventoryItem> UEPFInventorySubsystem::GetCachedInventory() const
{
	return CachedInventory;
}

TArray<FEPFCatalogItem> UEPFInventorySubsystem::GetCachedCatalog() const
{
	return CachedCatalog;
}

bool UEPFInventorySubsystem::OwnsItem(const FString& ItemId) const
{
	for (const auto& Item : CachedInventory)
	{
		if (Item.ItemId == ItemId) return true;
	}
	return false;
}

int32 UEPFInventorySubsystem::GetItemCount(const FString& ItemId) const
{
	int32 Count = 0;
	for (const auto& Item : CachedInventory)
	{
		if (Item.ItemId == ItemId) Count++;
	}
	return Count;
}

FEPFInventoryItem UEPFInventorySubsystem::FindItem(const FString& ItemId, bool& bFound) const
{
	for (const auto& Item : CachedInventory)
	{
		if (Item.ItemId == ItemId)
		{
			bFound = true;
			return Item;
		}
	}
	bFound = false;
	return FEPFInventoryItem();
}
