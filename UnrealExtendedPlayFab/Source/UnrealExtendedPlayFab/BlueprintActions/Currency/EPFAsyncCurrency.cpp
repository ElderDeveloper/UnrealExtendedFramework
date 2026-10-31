// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncCurrency.h"

// ── Get Balances ─────────────────────────────────────────────────────────────

UEPFAsyncGetBalances* UEPFAsyncGetBalances::GetBalances(UObject* WorldContext)
{
	auto* Action = NewObject<UEPFAsyncGetBalances>();
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncGetBalances::ExecuteAction()
{
	auto* Sub = RequireGameInstanceSubsystem<UEPFCurrencySubsystem>();
	if (!Sub) return;
	Sub->OnBalancesReceived.AddDynamic(this, &UEPFAsyncGetBalances::HandleComplete);
	CallWithContext([&] { Sub->GetBalances(); });
}

void UEPFAsyncGetBalances::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFCurrencySubsystem>())
		Sub->OnBalancesReceived.RemoveDynamic(this, &UEPFAsyncGetBalances::HandleComplete);
}

void UEPFAsyncGetBalances::HandleComplete(const FEPFResult& Result, const TArray<FEPFCurrencyBalance>& Balances)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Balances) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get balances")));
}

// ── Add Currency ─────────────────────────────────────────────────────────────

UEPFAsyncAddCurrency* UEPFAsyncAddCurrency::AddCurrency(UObject* WorldContext, const FString& CurrencyCode, int32 Amount)
{
	auto* Action = NewObject<UEPFAsyncAddCurrency>();
	Action->RequestedCurrencyCode = CurrencyCode;
	Action->RequestedAmount = Amount;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncAddCurrency::ExecuteAction()
{
	if (RequestedCurrencyCode.IsEmpty()) { FailInvalidArgument(TEXT("Currency code cannot be empty")); return; }
	if (RequestedAmount <= 0) { FailInvalidArgument(TEXT("Amount must be positive")); return; }
	auto* Sub = RequireGameInstanceSubsystem<UEPFCurrencySubsystem>();
	if (!Sub) return;
	Sub->OnCurrencyModified.AddDynamic(this, &UEPFAsyncAddCurrency::HandleComplete);
	CallWithContext([&] { Sub->AddCurrency(RequestedCurrencyCode, RequestedAmount); });
}

void UEPFAsyncAddCurrency::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFCurrencySubsystem>())
		Sub->OnCurrencyModified.RemoveDynamic(this, &UEPFAsyncAddCurrency::HandleComplete);
}

void UEPFAsyncAddCurrency::HandleComplete(const FEPFResult& Result, const FString& Code)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Code) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to add currency")));
}

// ── Subtract Currency ────────────────────────────────────────────────────────

UEPFAsyncSubtractCurrency* UEPFAsyncSubtractCurrency::SubtractCurrency(UObject* WorldContext, const FString& CurrencyCode, int32 Amount)
{
	auto* Action = NewObject<UEPFAsyncSubtractCurrency>();
	Action->RequestedCurrencyCode = CurrencyCode;
	Action->RequestedAmount = Amount;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncSubtractCurrency::ExecuteAction()
{
	if (RequestedCurrencyCode.IsEmpty()) { FailInvalidArgument(TEXT("Currency code cannot be empty")); return; }
	if (RequestedAmount <= 0) { FailInvalidArgument(TEXT("Amount must be positive")); return; }
	auto* Sub = RequireGameInstanceSubsystem<UEPFCurrencySubsystem>();
	if (!Sub) return;
	Sub->OnCurrencyModified.AddDynamic(this, &UEPFAsyncSubtractCurrency::HandleComplete);
	CallWithContext([&] { Sub->SubtractCurrency(RequestedCurrencyCode, RequestedAmount); });
}

void UEPFAsyncSubtractCurrency::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFCurrencySubsystem>())
		Sub->OnCurrencyModified.RemoveDynamic(this, &UEPFAsyncSubtractCurrency::HandleComplete);
}

void UEPFAsyncSubtractCurrency::HandleComplete(const FEPFResult& Result, const FString& Code)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Code) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to subtract currency")));
}
