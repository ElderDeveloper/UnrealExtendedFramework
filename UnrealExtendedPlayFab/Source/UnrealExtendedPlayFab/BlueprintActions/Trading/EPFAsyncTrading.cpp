// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncTrading.h"

// ── Open Trade ───────────────────────────────────────────────────────────────
UEPFAsyncOpenTrade* UEPFAsyncOpenTrade::OpenTrade(UObject* WorldContext, const TArray<FString>& OfferedItemIds, const TArray<FString>& RequestedCatalogItemIds, const TArray<FString>& AllowedPlayerIds)
{
	auto* A = NewObject<UEPFAsyncOpenTrade>(); A->Offered = OfferedItemIds; A->Requested = RequestedCatalogItemIds; A->Allowed = AllowedPlayerIds; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncOpenTrade::ExecuteAction()
{
	if (Offered.Num() == 0) { FailInvalidArgument(TEXT("Must offer at least one item")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFTradingSubsystem>();
	if (!S) return;
	S->OnTradeOpened.AddDynamic(this, &UEPFAsyncOpenTrade::HandleComplete);
	CallWithContext([&] { S->OpenTrade(Offered, Requested, Allowed); });
}
void UEPFAsyncOpenTrade::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFTradingSubsystem>()) S->OnTradeOpened.RemoveDynamic(this, &UEPFAsyncOpenTrade::HandleComplete);
}
void UEPFAsyncOpenTrade::HandleComplete(const FEPFResult& Result, const FString& TradeId)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(TradeId) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to open trade")));
}

// ── Accept Trade ─────────────────────────────────────────────────────────────
UEPFAsyncAcceptTrade* UEPFAsyncAcceptTrade::AcceptTrade(UObject* WorldContext, const FString& TradeId, const FString& OfferingPlayerId, const TArray<FString>& AcceptedItemIds)
{
	auto* A = NewObject<UEPFAsyncAcceptTrade>(); A->TargetTradeId = TradeId; A->OfferingId = OfferingPlayerId; A->Accepted = AcceptedItemIds; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncAcceptTrade::ExecuteAction()
{
	if (TargetTradeId.IsEmpty() || OfferingId.IsEmpty()) { FailInvalidArgument(TEXT("TradeId and OfferingPlayerId required")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFTradingSubsystem>();
	if (!S) return;
	S->OnTradeAccepted.AddDynamic(this, &UEPFAsyncAcceptTrade::HandleComplete);
	CallWithContext([&] { S->AcceptTrade(TargetTradeId, OfferingId, Accepted); });
}
void UEPFAsyncAcceptTrade::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFTradingSubsystem>()) S->OnTradeAccepted.RemoveDynamic(this, &UEPFAsyncAcceptTrade::HandleComplete);
}
void UEPFAsyncAcceptTrade::HandleComplete(const FEPFResult& Result, const FEPFTradeInfo& Trade)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Trade) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to accept trade")));
}

// ── Cancel Trade ─────────────────────────────────────────────────────────────
UEPFAsyncCancelTrade* UEPFAsyncCancelTrade::CancelTrade(UObject* WorldContext, const FString& TradeId)
{
	auto* A = NewObject<UEPFAsyncCancelTrade>(); A->TargetTradeId = TradeId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncCancelTrade::ExecuteAction()
{
	if (TargetTradeId.IsEmpty()) { FailInvalidArgument(TEXT("TradeId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFTradingSubsystem>();
	if (!S) return;
	S->OnTradeCanceled.AddDynamic(this, &UEPFAsyncCancelTrade::HandleComplete);
	CallWithContext([&] { S->CancelTrade(TargetTradeId); });
}
void UEPFAsyncCancelTrade::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFTradingSubsystem>()) S->OnTradeCanceled.RemoveDynamic(this, &UEPFAsyncCancelTrade::HandleComplete);
}
void UEPFAsyncCancelTrade::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to cancel trade")));
}

// ── Get Trades ───────────────────────────────────────────────────────────────
UEPFAsyncGetTrades* UEPFAsyncGetTrades::GetPlayerTrades(UObject* WorldContext)
{
	auto* A = NewObject<UEPFAsyncGetTrades>(); A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGetTrades::ExecuteAction()
{
	auto* S = RequireGameInstanceSubsystem<UEPFTradingSubsystem>();
	if (!S) return;
	S->OnTradesReceived.AddDynamic(this, &UEPFAsyncGetTrades::HandleComplete);
	CallWithContext([&] { S->GetPlayerTrades(); });
}
void UEPFAsyncGetTrades::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFTradingSubsystem>()) S->OnTradesReceived.RemoveDynamic(this, &UEPFAsyncGetTrades::HandleComplete);
}
void UEPFAsyncGetTrades::HandleComplete(const FEPFResult& Result, const TArray<FEPFTradeInfo>& Trades)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Trades) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get trades")));
}

// ── Get Trade Status ─────────────────────────────────────────────────────────
UEPFAsyncGetTradeStatus* UEPFAsyncGetTradeStatus::GetTradeStatus(UObject* WorldContext, const FString& TradeId, const FString& OfferingPlayerId)
{
	auto* A = NewObject<UEPFAsyncGetTradeStatus>(); A->TargetTradeId = TradeId; A->OfferingId = OfferingPlayerId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGetTradeStatus::ExecuteAction()
{
	if (TargetTradeId.IsEmpty() || OfferingId.IsEmpty()) { FailInvalidArgument(TEXT("TradeId and OfferingPlayerId required")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFTradingSubsystem>();
	if (!S) return;
	S->OnTradeInfoReceived.AddDynamic(this, &UEPFAsyncGetTradeStatus::HandleComplete);
	CallWithContext([&] { S->GetTradeStatus(TargetTradeId, OfferingId); });
}
void UEPFAsyncGetTradeStatus::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFTradingSubsystem>()) S->OnTradeInfoReceived.RemoveDynamic(this, &UEPFAsyncGetTradeStatus::HandleComplete);
}
void UEPFAsyncGetTradeStatus::HandleComplete(const FEPFResult& Result, const FEPFTradeInfo& Trade)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Trade) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get trade status")));
}
