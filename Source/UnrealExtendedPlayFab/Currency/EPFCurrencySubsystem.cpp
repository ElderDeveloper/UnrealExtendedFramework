// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFCurrencySubsystem.h"
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
}

void UEPFCurrencySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFCurrencySubsystem::Deinitialize()
{
	CachedBalances.Empty();
	Super::Deinitialize();
}

void UEPFCurrencySubsystem::GetBalances()
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetObjectField(TEXT("Entity"), MakeEntityKeyObject(GetEntityId(), GetEntityType()));
	Body->SetStringField(TEXT("Filter"), TEXT("type eq 'currency'"));
	Body->SetNumberField(TEXT("Count"), 50);

	SendPlayFabRequestDetailed(
		TEXT("/Inventory/GetInventoryItems"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateUObject(this, &UEPFCurrencySubsystem::ParseCurrencyResponse)
	);
}

void UEPFCurrencySubsystem::AddCurrency(const FString& CurrencyCode, int32 Amount, const FString& IdempotencyId)
{
	if (CurrencyCode.IsEmpty())
	{
		UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFCurrencySubsystem::AddCurrency — CurrencyCode cannot be empty"));
		OnCurrencyModified.Broadcast(FEPFResult::Failure(TEXT("CurrencyCode cannot be empty")), CurrencyCode);
		return;
	}
	if (Amount <= 0)
	{
		UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFCurrencySubsystem::AddCurrency — Amount must be positive, got %d"), Amount);
		OnCurrencyModified.Broadcast(FEPFResult::Failure(TEXT("Amount must be positive")), CurrencyCode);
		return;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetObjectField(TEXT("Entity"), MakeEntityKeyObject(GetEntityId(), GetEntityType()));
	Body->SetNumberField(TEXT("Amount"), Amount);

	TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
	Item->SetStringField(TEXT("Id"), CurrencyCode);
	Item->SetStringField(TEXT("StackId"), TEXT("default"));
	Body->SetObjectField(TEXT("Item"), Item);
	if (!IdempotencyId.IsEmpty())
	{
		Body->SetStringField(TEXT("IdempotencyId"), IdempotencyId);
	}

	SendPlayFabRequestDetailed(
		TEXT("/Inventory/AddInventoryItems"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this, CurrencyCode, Amount](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess)
			{
				const int32 NewBalance = FMath::Max(GetCachedBalance(CurrencyCode), 0) + Amount;
				CachedBalances.Add(CurrencyCode, NewBalance);
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFCurrencySubsystem — Added %s, new balance: %d"), *CurrencyCode, NewBalance);
			}
			OnCurrencyModified.Broadcast(Result, CurrencyCode);
		})
	);
}

void UEPFCurrencySubsystem::SubtractCurrency(const FString& CurrencyCode, int32 Amount, const FString& IdempotencyId)
{
	if (CurrencyCode.IsEmpty())
	{
		UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFCurrencySubsystem::SubtractCurrency — CurrencyCode cannot be empty"));
		OnCurrencyModified.Broadcast(FEPFResult::Failure(TEXT("CurrencyCode cannot be empty")), CurrencyCode);
		return;
	}
	if (Amount <= 0)
	{
		UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFCurrencySubsystem::SubtractCurrency — Amount must be positive, got %d"), Amount);
		OnCurrencyModified.Broadcast(FEPFResult::Failure(TEXT("Amount must be positive")), CurrencyCode);
		return;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetObjectField(TEXT("Entity"), MakeEntityKeyObject(GetEntityId(), GetEntityType()));
	Body->SetNumberField(TEXT("Amount"), Amount);
	Body->SetBoolField(TEXT("DeleteEmptyStacks"), true);

	TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
	Item->SetStringField(TEXT("Id"), CurrencyCode);
	Item->SetStringField(TEXT("StackId"), TEXT("default"));
	Body->SetObjectField(TEXT("Item"), Item);
	if (!IdempotencyId.IsEmpty())
	{
		Body->SetStringField(TEXT("IdempotencyId"), IdempotencyId);
	}

	SendPlayFabRequestDetailed(
		TEXT("/Inventory/SubtractInventoryItems"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this, CurrencyCode, Amount](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess)
			{
				const int32 NewBalance = FMath::Max(FMath::Max(GetCachedBalance(CurrencyCode), 0) - Amount, 0);
				CachedBalances.Add(CurrencyCode, NewBalance);
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFCurrencySubsystem — Subtracted %s, new balance: %d"), *CurrencyCode, NewBalance);
			}
			OnCurrencyModified.Broadcast(Result, CurrencyCode);
		})
	);
}

int32 UEPFCurrencySubsystem::GetCachedBalance(const FString& CurrencyCode) const
{
	const int32* Found = CachedBalances.Find(CurrencyCode);
	return Found ? *Found : -1;
}

TArray<FEPFCurrencyBalance> UEPFCurrencySubsystem::GetAllCachedBalances() const
{
	TArray<FEPFCurrencyBalance> Result;
	for (const auto& Pair : CachedBalances)
	{
		FEPFCurrencyBalance Balance;
		Balance.CurrencyCode = Pair.Key;
		Balance.Amount = Pair.Value;
		Result.Add(Balance);
	}
	return Result;
}

bool UEPFCurrencySubsystem::CanAfford(const FString& CurrencyCode, int32 Amount) const
{
	const int32* Found = CachedBalances.Find(CurrencyCode);
	return Found && *Found >= Amount;
}

void UEPFCurrencySubsystem::ParseCurrencyResponse(const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
{
	if (Result.bSuccess && Response.IsValid())
	{
		CachedBalances.Empty();
		const TArray<TSharedPtr<FJsonValue>>* ItemsArray = nullptr;
		if (Response->TryGetArrayField(TEXT("Items"), ItemsArray) && ItemsArray)
		{
			for (const TSharedPtr<FJsonValue>& ItemValue : *ItemsArray)
			{
				const TSharedPtr<FJsonObject>* ItemObj = nullptr;
				if (!ItemValue.IsValid() || !ItemValue->TryGetObject(ItemObj) || !ItemObj)
				{
					continue;
				}

				FString ItemId;
				(*ItemObj)->TryGetStringField(TEXT("Id"), ItemId);

				double AmountNumber = 0.0;
				if (!ItemId.IsEmpty() && (*ItemObj)->TryGetNumberField(TEXT("Amount"), AmountNumber))
				{
					CachedBalances.Add(ItemId, static_cast<int32>(AmountNumber));
				}
			}
		}
		UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFCurrencySubsystem — Fetched %d currency balances"), CachedBalances.Num());

		TArray<FEPFCurrencyBalance> Balances = GetAllCachedBalances();
		OnBalancesReceived.Broadcast(Result, Balances);
	}
	else
	{
		UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFCurrencySubsystem — Failed to fetch balances"));
		TArray<FEPFCurrencyBalance> Empty;
		OnBalancesReceived.Broadcast(Result, Empty);
	}
}
