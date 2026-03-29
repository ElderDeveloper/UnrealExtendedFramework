// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFTradingSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

void UEPFTradingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFTradingSubsystem::Deinitialize()
{
	CachedTrades.Empty();
	Super::Deinitialize();
}


// ── Helpers ──────────────────────────────────────────────────────────────────

EEPFTradeStatus UEPFTradingSubsystem::ParseTradeStatus(const FString& StatusStr)
{
	if (StatusStr == TEXT("Opening")) return EEPFTradeStatus::Opening;
	if (StatusStr == TEXT("Open")) return EEPFTradeStatus::Open;
	if (StatusStr == TEXT("Accepting")) return EEPFTradeStatus::Accepting;
	if (StatusStr == TEXT("Accepted")) return EEPFTradeStatus::Accepted;
	if (StatusStr == TEXT("Filled")) return EEPFTradeStatus::Filled;
	if (StatusStr == TEXT("Cancelled")) return EEPFTradeStatus::Cancelled;
	return EEPFTradeStatus::Invalid;
}

FEPFTradeInfo UEPFTradingSubsystem::ParseTradeJson(const TSharedPtr<FJsonObject>& TradeObj) const
{
	FEPFTradeInfo Trade;
	if (!TradeObj.IsValid()) return Trade;

	TradeObj->TryGetStringField(TEXT("TradeId"), Trade.TradeId);
	TradeObj->TryGetStringField(TEXT("OfferingPlayerId"), Trade.OfferingPlayerId);
	Trade.Status = ParseTradeStatus(TradeObj->GetStringField(TEXT("Status")));
	// FilledByPlayerId and OpenedAt are absent/null for trades that haven't been accepted yet.
	TradeObj->TryGetStringField(TEXT("FilledByPlayerId"), Trade.FilledByPlayerId);
	TradeObj->TryGetStringField(TEXT("OpenedAt"), Trade.OpenedAt);

	const TArray<TSharedPtr<FJsonValue>>* OfferedArr = nullptr;
	if (TradeObj->TryGetArrayField(TEXT("OfferedInventoryInstanceIds"), OfferedArr) && OfferedArr)
	{
		for (const auto& V : *OfferedArr) { FString S; if (V->TryGetString(S)) Trade.OfferedItemInstanceIds.Add(S); }
	}

	const TArray<TSharedPtr<FJsonValue>>* RequestedArr = nullptr;
	if (TradeObj->TryGetArrayField(TEXT("RequestedCatalogItemIds"), RequestedArr) && RequestedArr)
	{
		for (const auto& V : *RequestedArr) { FString S; if (V->TryGetString(S)) Trade.RequestedCatalogItemIds.Add(S); }
	}

	return Trade;
}


// ── Open Trade ───────────────────────────────────────────────────────────────

void UEPFTradingSubsystem::OpenTrade(const TArray<FString>& OfferedItemInstanceIds, const TArray<FString>& RequestedCatalogItemIds, const TArray<FString>& AllowedPlayerIds)
{
	if (OfferedItemInstanceIds.Num() == 0) { OnTradeOpened.Broadcast(FEPFResult::Failure(TEXT("At least one offered item is required")), TEXT("")); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> OfferedArr;
	for (const auto& Id : OfferedItemInstanceIds) OfferedArr.Add(MakeShared<FJsonValueString>(Id));
	Body->SetArrayField(TEXT("OfferedInventoryInstanceIds"), OfferedArr);

	if (RequestedCatalogItemIds.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> RequestedArr;
		for (const auto& Id : RequestedCatalogItemIds) RequestedArr.Add(MakeShared<FJsonValueString>(Id));
		Body->SetArrayField(TEXT("RequestedCatalogItemIds"), RequestedArr);
	}

	if (AllowedPlayerIds.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> AllowedArr;
		for (const auto& Id : AllowedPlayerIds) AllowedArr.Add(MakeShared<FJsonValueString>(Id));
		Body->SetArrayField(TEXT("AllowedPlayerIds"), AllowedArr);
	}

	SendPlayFabRequestDetailed(TEXT("/Client/OpenTrade"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			FString TradeId;
			if (Result.bSuccess && Response.IsValid())
			{
				const TSharedPtr<FJsonObject>* TradeObj = nullptr;
				if (Response->TryGetObjectField(TEXT("Trade"), TradeObj) && TradeObj)
				{
					FEPFTradeInfo Trade = ParseTradeJson(*TradeObj);
					TradeId = Trade.TradeId;
					CachedTrades.Add(Trade);
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFTrading — Trade opened: %s"), *TradeId);
			}
			OnTradeOpened.Broadcast(Result, TradeId);
		}));
}


// ── Accept Trade ─────────────────────────────────────────────────────────────

void UEPFTradingSubsystem::AcceptTrade(const FString& TradeId, const FString& OfferingPlayerId, const TArray<FString>& AcceptedItemInstanceIds)
{
	if (TradeId.IsEmpty() || OfferingPlayerId.IsEmpty()) { OnTradeAccepted.Broadcast(FEPFResult::Failure(TEXT("TradeId and OfferingPlayerId are required")), FEPFTradeInfo()); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("TradeId"), TradeId);
	Body->SetStringField(TEXT("OfferingPlayerId"), OfferingPlayerId);

	if (AcceptedItemInstanceIds.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> AcceptedArr;
		for (const auto& Id : AcceptedItemInstanceIds) AcceptedArr.Add(MakeShared<FJsonValueString>(Id));
		Body->SetArrayField(TEXT("AcceptedInventoryInstanceIds"), AcceptedArr);
	}

	SendPlayFabRequestDetailed(TEXT("/Client/AcceptTrade"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			FEPFTradeInfo Trade;
			if (Result.bSuccess && Response.IsValid())
			{
				const TSharedPtr<FJsonObject>* TradeObj = nullptr;
				if (Response->TryGetObjectField(TEXT("Trade"), TradeObj) && TradeObj)
				{
					Trade = ParseTradeJson(*TradeObj);
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFTrading — Trade accepted: %s"), *Trade.TradeId);
			}
			OnTradeAccepted.Broadcast(Result, Trade);
		}));
}


// ── Cancel Trade ─────────────────────────────────────────────────────────────

void UEPFTradingSubsystem::CancelTrade(const FString& TradeId)
{
	if (TradeId.IsEmpty()) { OnTradeCanceled.Broadcast(FEPFResult::Failure(TEXT("TradeId cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("TradeId"), TradeId);

	SendPlayFabRequestDetailed(TEXT("/Client/CancelTrade"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this, TradeId](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess)
			{
				CachedTrades.RemoveAll([&](const FEPFTradeInfo& T) { return T.TradeId == TradeId; });
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFTrading — Trade canceled: %s"), *TradeId);
			}
			OnTradeCanceled.Broadcast(Result);
		}));
}


// ── Get Player Trades ────────────────────────────────────────────────────────

void UEPFTradingSubsystem::GetPlayerTrades()
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	SendPlayFabRequestDetailed(TEXT("/Client/GetPlayerTrades"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				CachedTrades.Empty();

				// OpenedTrades
				const TArray<TSharedPtr<FJsonValue>>* OpenedArr = nullptr;
				if (Response->TryGetArrayField(TEXT("OpenedTrades"), OpenedArr) && OpenedArr)
				{
					for (const auto& V : *OpenedArr)
					{
						const TSharedPtr<FJsonObject>* Obj = nullptr;
						if (V->TryGetObject(Obj)) CachedTrades.Add(ParseTradeJson(*Obj));
					}
				}

				// AcceptedTrades
				const TArray<TSharedPtr<FJsonValue>>* AcceptedArr = nullptr;
				if (Response->TryGetArrayField(TEXT("AcceptedTrades"), AcceptedArr) && AcceptedArr)
				{
					for (const auto& V : *AcceptedArr)
					{
						const TSharedPtr<FJsonObject>* Obj = nullptr;
						if (V->TryGetObject(Obj)) CachedTrades.Add(ParseTradeJson(*Obj));
					}
				}

				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFTrading — %d active trades"), CachedTrades.Num());
			}
			OnTradesReceived.Broadcast(Result, CachedTrades);
		}));
}


// ── Get Trade Status ─────────────────────────────────────────────────────────

void UEPFTradingSubsystem::GetTradeStatus(const FString& TradeId, const FString& OfferingPlayerId)
{
	if (TradeId.IsEmpty() || OfferingPlayerId.IsEmpty()) { OnTradeInfoReceived.Broadcast(FEPFResult::Failure(TEXT("TradeId and OfferingPlayerId are required")), FEPFTradeInfo()); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("TradeId"), TradeId);
	Body->SetStringField(TEXT("OfferingPlayerId"), OfferingPlayerId);

	SendPlayFabRequestDetailed(TEXT("/Client/GetTradeStatus"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			FEPFTradeInfo Trade;
			if (Result.bSuccess && Response.IsValid())
			{
				const TSharedPtr<FJsonObject>* TradeObj = nullptr;
				if (Response->TryGetObjectField(TEXT("Trade"), TradeObj) && TradeObj)
				{
					Trade = ParseTradeJson(*TradeObj);
				}
			}
			OnTradeInfoReceived.Broadcast(Result, Trade);
		}));
}


// ── Queries ──────────────────────────────────────────────────────────────────

TArray<FEPFTradeInfo> UEPFTradingSubsystem::GetCachedTrades() const { return CachedTrades; }
