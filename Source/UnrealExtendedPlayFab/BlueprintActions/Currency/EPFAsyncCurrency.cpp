// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncCurrency.h"
#include "Engine/GameInstance.h"

// ── Get Balances ─────────────────────────────────────────────────────────────

UEPFAsyncGetBalances* UEPFAsyncGetBalances::GetBalances(UObject* WorldContext)
{
	auto* Action = NewObject<UEPFAsyncGetBalances>();
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncGetBalances::Activate()
{
	auto* Sub = GetEPFSubsystemFromContext<UEPFCurrencySubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Currency subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnBalancesReceived.AddDynamic(this, &UEPFAsyncGetBalances::HandleComplete);
	Sub->GetBalances();
}

void UEPFAsyncGetBalances::HandleComplete(const FEPFResult& Result, const TArray<FEPFCurrencyBalance>& Balances)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFCurrencySubsystem>(WorldContext.Get()))
		Sub->OnBalancesReceived.RemoveDynamic(this, &UEPFAsyncGetBalances::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Balances) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get balances")));
	SetReadyToDestroy();
}

// ── Add Currency ─────────────────────────────────────────────────────────────

UEPFAsyncAddCurrency* UEPFAsyncAddCurrency::AddCurrency(UObject* WorldContext, const FString& CurrencyCode, int32 Amount)
{
	auto* Action = NewObject<UEPFAsyncAddCurrency>();
	Action->CurrencyCode = CurrencyCode;
	Action->Amount = Amount;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncAddCurrency::Activate()
{
	if (CurrencyCode.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Currency code cannot be empty"))); SetReadyToDestroy(); return; }
	if (Amount <= 0) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Amount must be positive"))); SetReadyToDestroy(); return; }
	auto* Sub = GetEPFSubsystemFromContext<UEPFCurrencySubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Currency subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnCurrencyModified.AddDynamic(this, &UEPFAsyncAddCurrency::HandleComplete);
	Sub->AddCurrency(CurrencyCode, Amount);
}

void UEPFAsyncAddCurrency::HandleComplete(const FEPFResult& Result, const FString& Code)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFCurrencySubsystem>(WorldContext.Get()))
		Sub->OnCurrencyModified.RemoveDynamic(this, &UEPFAsyncAddCurrency::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Code) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to add currency")));
	SetReadyToDestroy();
}

// ── Subtract Currency ────────────────────────────────────────────────────────

UEPFAsyncSubtractCurrency* UEPFAsyncSubtractCurrency::SubtractCurrency(UObject* WorldContext, const FString& CurrencyCode, int32 Amount)
{
	auto* Action = NewObject<UEPFAsyncSubtractCurrency>();
	Action->CurrencyCode = CurrencyCode;
	Action->Amount = Amount;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncSubtractCurrency::Activate()
{
	if (CurrencyCode.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Currency code cannot be empty"))); SetReadyToDestroy(); return; }
	if (Amount <= 0) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Amount must be positive"))); SetReadyToDestroy(); return; }
	auto* Sub = GetEPFSubsystemFromContext<UEPFCurrencySubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Currency subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnCurrencyModified.AddDynamic(this, &UEPFAsyncSubtractCurrency::HandleComplete);
	Sub->SubtractCurrency(CurrencyCode, Amount);
}

void UEPFAsyncSubtractCurrency::HandleComplete(const FEPFResult& Result, const FString& Code)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFCurrencySubsystem>(WorldContext.Get()))
		Sub->OnCurrencyModified.RemoveDynamic(this, &UEPFAsyncSubtractCurrency::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Code) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to subtract currency")));
	SetReadyToDestroy();
}
