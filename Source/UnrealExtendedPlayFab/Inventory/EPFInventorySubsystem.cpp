// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFInventorySubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
	TSharedRef<FJsonObject> MakeEntityKeyObject(const FString& EntityId, const FString& EntityType)
	{
		TSharedRef<FJsonObject> Entity = MakeShared<FJsonObject>();
		Entity->SetStringField(TEXT("Id"), EntityId);
		Entity->SetStringField(TEXT("Type"), EntityType);
		return Entity;
	}

	FString GetLocalizedField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName)
	{
		const TSharedPtr<FJsonObject>* LocalizedObject = nullptr;
		if (!Object.IsValid() || !Object->TryGetObjectField(FieldName, LocalizedObject) || !LocalizedObject)
		{
			return FString();
		}

		FString NeutralValue;
		if ((*LocalizedObject)->TryGetStringField(TEXT("NEUTRAL"), NeutralValue))
		{
			return NeutralValue;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*LocalizedObject)->Values)
		{
			FString Value;
			if (Pair.Value.IsValid() && Pair.Value->TryGetString(Value))
			{
				return Value;
			}
		}

		return FString();
	}

	void ExtractPriceAmounts(const TSharedPtr<FJsonObject>& CatalogObject, TMap<FString, int32>& OutPrices)
	{
		const TSharedPtr<FJsonObject>* PriceOptions = nullptr;
		if (!CatalogObject.IsValid() || !CatalogObject->TryGetObjectField(TEXT("PriceOptions"), PriceOptions) || !PriceOptions)
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* PricesArray = nullptr;
		if (!(*PriceOptions)->TryGetArrayField(TEXT("Prices"), PricesArray) || !PricesArray)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& PriceValue : *PricesArray)
		{
			const TSharedPtr<FJsonObject>* PriceObject = nullptr;
			if (!PriceValue.IsValid() || !PriceValue->TryGetObject(PriceObject) || !PriceObject)
			{
				continue;
			}

			const TArray<TSharedPtr<FJsonValue>>* AmountsArray = nullptr;
			if (!(*PriceObject)->TryGetArrayField(TEXT("Amounts"), AmountsArray) || !AmountsArray)
			{
				continue;
			}

			for (const TSharedPtr<FJsonValue>& AmountValue : *AmountsArray)
			{
				const TSharedPtr<FJsonObject>* AmountObject = nullptr;
				if (!AmountValue.IsValid() || !AmountValue->TryGetObject(AmountObject) || !AmountObject)
				{
					continue;
				}

				FString PriceItemId;
				double PriceAmount = 0.0;
				(*AmountObject)->TryGetStringField(TEXT("ItemId"), PriceItemId);
				if (!PriceItemId.IsEmpty() && (*AmountObject)->TryGetNumberField(TEXT("Amount"), PriceAmount))
				{
					OutPrices.Add(PriceItemId, static_cast<int32>(PriceAmount));
				}
			}
		}
	}

	FString GetJsonScalarString(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return FString();
		}

		FString StringValue;
		if (Value->TryGetString(StringValue))
		{
			return StringValue;
		}

		double NumberValue = 0.0;
		if (Value->TryGetNumber(NumberValue))
		{
			return FString::Printf(TEXT("%.0f"), NumberValue);
		}

		bool BoolValue = false;
		if (Value->TryGetBool(BoolValue))
		{
			return BoolValue ? TEXT("true") : TEXT("false");
		}

		return FString();
	}

}

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
	Body->SetObjectField(TEXT("Entity"), MakeEntityKeyObject(GetEntityId(), GetEntityType()));
	Body->SetNumberField(TEXT("Count"), 50);

	SendPlayFabRequestDetailed(
		TEXT("/Inventory/GetInventoryItems"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				CachedInventory.Empty();
				const TArray<TSharedPtr<FJsonValue>>* ItemsArr = nullptr;
				if (Response->TryGetArrayField(TEXT("Items"), ItemsArr) && ItemsArr)
				{
					for (const TSharedPtr<FJsonValue>& ItemVal : *ItemsArr)
					{
						const TSharedPtr<FJsonObject>* ItemObj = nullptr;
						if (ItemVal.IsValid() && ItemVal->TryGetObject(ItemObj) && ItemObj)
						{
							FEPFInventoryItem Item;
							(*ItemObj)->TryGetStringField(TEXT("StackId"), Item.StackId);
							Item.ItemInstanceId = Item.StackId;
							(*ItemObj)->TryGetStringField(TEXT("Id"), Item.ItemId);
							(*ItemObj)->TryGetStringField(TEXT("Type"), Item.ItemClass);
							Item.DisplayName = Item.ItemId;

							for (const FEPFCatalogItem& CatalogItem : CachedCatalog)
							{
								if (CatalogItem.ItemId == Item.ItemId)
								{
									if (!CatalogItem.DisplayName.IsEmpty())
									{
										Item.DisplayName = CatalogItem.DisplayName;
									}
									break;
								}
							}

							double AmountValue = 0.0;
							if ((*ItemObj)->TryGetNumberField(TEXT("Amount"), AmountValue))
							{
								Item.Amount = static_cast<int32>(AmountValue);
								Item.RemainingUses = Item.Amount;
							}

							const TSharedPtr<FJsonObject>* CustomDataObj = nullptr;
							if ((*ItemObj)->TryGetObjectField(TEXT("DisplayProperties"), CustomDataObj) && CustomDataObj)
							{
								for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*CustomDataObj)->Values)
								{
									const FString Value = GetJsonScalarString(Pair.Value);
									if (!Value.IsEmpty())
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

void UEPFInventorySubsystem::GetCatalog(const FString& StoreId)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetObjectField(TEXT("Entity"), MakeEntityKeyObject(GetEntityId(), GetEntityType()));
	Body->SetNumberField(TEXT("Count"), 50);
	if (!StoreId.IsEmpty())
	{
		TSharedRef<FJsonObject> StoreObject = MakeShared<FJsonObject>();
		StoreObject->SetStringField(TEXT("Id"), StoreId);
		Body->SetObjectField(TEXT("Store"), StoreObject);
	}

	SendPlayFabRequestDetailed(
		TEXT("/Catalog/SearchItems"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				CachedCatalog.Empty();
				const TArray<TSharedPtr<FJsonValue>>* CatalogArr = nullptr;
				if (Response->TryGetArrayField(TEXT("Items"), CatalogArr) && CatalogArr)
				{
					for (const TSharedPtr<FJsonValue>& CatVal : *CatalogArr)
					{
						const TSharedPtr<FJsonObject>* CatObj = nullptr;
						if (CatVal.IsValid() && CatVal->TryGetObject(CatObj) && CatObj)
						{
							FEPFCatalogItem Item;
							(*CatObj)->TryGetStringField(TEXT("Id"), Item.ItemId);
							Item.DisplayName = GetLocalizedField(*CatObj, TEXT("Title"));
							Item.Description = GetLocalizedField(*CatObj, TEXT("Description"));
							(*CatObj)->TryGetStringField(TEXT("Type"), Item.ItemType);
							Item.ItemClass = Item.ItemType;
							ExtractPriceAmounts(*CatObj, Item.VirtualCurrencyPrices);

							CachedCatalog.Add(Item);
						}
					}
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFInventorySubsystem — Fetched %d catalog definitions"), CachedCatalog.Num());
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

void UEPFInventorySubsystem::PurchaseItem(const FString& ItemId, const FString& CurrencyCode, int32 Price, const FString& StoreId)
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
	Body->SetObjectField(TEXT("Entity"), MakeEntityKeyObject(GetEntityId(), GetEntityType()));
	Body->SetNumberField(TEXT("Amount"), 1);

	TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
	Item->SetStringField(TEXT("Id"), ItemId);
	Body->SetObjectField(TEXT("Item"), Item);

	TArray<TSharedPtr<FJsonValue>> PriceAmounts;
	TSharedRef<FJsonObject> PriceAmount = MakeShared<FJsonObject>();
	PriceAmount->SetStringField(TEXT("ItemId"), CurrencyCode);
	PriceAmount->SetNumberField(TEXT("Amount"), Price);
	PriceAmount->SetStringField(TEXT("StackId"), TEXT("default"));
	PriceAmounts.Add(MakeShared<FJsonValueObject>(PriceAmount));
	Body->SetArrayField(TEXT("PriceAmounts"), PriceAmounts);

	if (!StoreId.IsEmpty())
	{
		Body->SetStringField(TEXT("StoreId"), StoreId);
	}

	SendPlayFabRequestDetailed(
		TEXT("/Inventory/PurchaseInventoryItems"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			FString InstanceId;
			if (Result.bSuccess)
			{
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
	Body->SetObjectField(TEXT("Entity"), MakeEntityKeyObject(GetEntityId(), GetEntityType()));
	Body->SetNumberField(TEXT("Amount"), ConsumeCount);
	Body->SetBoolField(TEXT("DeleteEmptyStacks"), true);

	TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
	Item->SetStringField(TEXT("StackId"), ItemInstanceId);
	for (const FEPFInventoryItem& CachedItem : CachedInventory)
	{
		if (CachedItem.StackId == ItemInstanceId)
		{
			Item->SetStringField(TEXT("Id"), CachedItem.ItemId);
			break;
		}
	}
	Body->SetObjectField(TEXT("Item"), Item);

	SendPlayFabRequestDetailed(
		TEXT("/Inventory/SubtractInventoryItems"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this, ItemInstanceId, ConsumeCount](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			int32 RemainingUses = -1;
			if (Result.bSuccess)
			{
				for (int32 Index = 0; Index < CachedInventory.Num(); ++Index)
				{
					if (CachedInventory[Index].StackId == ItemInstanceId)
					{
						CachedInventory[Index].Amount = FMath::Max(CachedInventory[Index].Amount - ConsumeCount, 0);
						CachedInventory[Index].RemainingUses = CachedInventory[Index].Amount;
						RemainingUses = CachedInventory[Index].Amount;
						if (CachedInventory[Index].Amount == 0)
						{
							CachedInventory.RemoveAt(Index);
						}
						break;
					}
				}
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
		if (Item.ItemId == ItemId && Item.Amount > 0) return true;
	}
	return false;
}

int32 UEPFInventorySubsystem::GetItemCount(const FString& ItemId) const
{
	int32 Count = 0;
	for (const auto& Item : CachedInventory)
	{
		if (Item.ItemId == ItemId) Count += FMath::Max(Item.Amount, 1);
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
