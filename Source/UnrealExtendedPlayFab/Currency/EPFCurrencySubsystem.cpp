// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFCurrencySubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

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

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetUserInventory"),
		Body,
		true,
		FOnPlayFabResponseDetailed::CreateUObject(this, &UEPFCurrencySubsystem::ParseCurrencyResponse)
	);
}

void UEPFCurrencySubsystem::AddCurrency(const FString& CurrencyCode, int32 Amount)
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
	Body->SetStringField(TEXT("VirtualCurrency"), CurrencyCode);
	Body->SetNumberField(TEXT("Amount"), Amount);

	SendPlayFabRequestDetailed(
		TEXT("/Client/AddUserVirtualCurrency"),
		Body,
		true,
		FOnPlayFabResponseDetailed::CreateLambda([this, CurrencyCode](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				const int32 NewBalance = static_cast<int32>(Response->GetNumberField(TEXT("Balance")));
				CachedBalances.Add(CurrencyCode, NewBalance);
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFCurrencySubsystem — Added %s, new balance: %d"), *CurrencyCode, NewBalance);
			}
			OnCurrencyModified.Broadcast(Result, CurrencyCode);
		})
	);
}

void UEPFCurrencySubsystem::SubtractCurrency(const FString& CurrencyCode, int32 Amount)
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
	Body->SetStringField(TEXT("VirtualCurrency"), CurrencyCode);
	Body->SetNumberField(TEXT("Amount"), Amount);

	SendPlayFabRequestDetailed(
		TEXT("/Client/SubtractUserVirtualCurrency"),
		Body,
		true,
		FOnPlayFabResponseDetailed::CreateLambda([this, CurrencyCode](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				const int32 NewBalance = static_cast<int32>(Response->GetNumberField(TEXT("Balance")));
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
		const TSharedPtr<FJsonObject>* VCObj = nullptr;
		if (Response->TryGetObjectField(TEXT("VirtualCurrency"), VCObj) && VCObj)
		{
			for (const auto& Pair : (*VCObj)->Values)
			{
				int32 Amount = 0;
				if (Pair.Value->TryGetNumber(Amount))
				{
					CachedBalances.Add(Pair.Key, Amount);
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
