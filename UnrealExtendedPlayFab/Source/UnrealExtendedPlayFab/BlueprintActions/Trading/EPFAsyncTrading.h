// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "Trading/EPFTradingSubsystem.h"
#include "EPFAsyncTrading.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncTradeOpenedSuccess, const FString&, TradeId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncTradeAcceptedSuccess, const FEPFTradeInfo&, Trade);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncTradeCanceledSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncTradesReceivedSuccess, const TArray<FEPFTradeInfo>&, Trades);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncTradeStatusSuccess, const FEPFTradeInfo&, Trade);

// ── Open Trade ───────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Open Trade"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncOpenTrade : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Open Trade"), Category = "PlayFab|Async|Trading")
	static UEPFAsyncOpenTrade* OpenTrade(UObject* WorldContext, const TArray<FString>& OfferedItemIds, const TArray<FString>& RequestedCatalogItemIds, const TArray<FString>& AllowedPlayerIds);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncTradeOpenedSuccess OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	TArray<FString> Offered; TArray<FString> Requested; TArray<FString> Allowed;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& TradeId);
};

// ── Accept Trade ─────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Accept Trade"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncAcceptTrade : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Accept Trade"), Category = "PlayFab|Async|Trading")
	static UEPFAsyncAcceptTrade* AcceptTrade(UObject* WorldContext, const FString& TradeId, const FString& OfferingPlayerId, const TArray<FString>& AcceptedItemIds);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncTradeAcceptedSuccess OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString TargetTradeId; FString OfferingId; TArray<FString> Accepted;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FEPFTradeInfo& Trade);
};

// ── Cancel Trade ─────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Cancel Trade"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncCancelTrade : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Cancel Trade"), Category = "PlayFab|Async|Trading")
	static UEPFAsyncCancelTrade* CancelTrade(UObject* WorldContext, const FString& TradeId);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncTradeCanceledSuccess OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString TargetTradeId;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};

// ── Get Trades ───────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Get Player Trades"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetTrades : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Player Trades"), Category = "PlayFab|Async|Trading")
	static UEPFAsyncGetTrades* GetPlayerTrades(UObject* WorldContext);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncTradesReceivedSuccess OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFTradeInfo>& Trades);
};

// ── Get Trade Status ─────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Get Trade Status"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetTradeStatus : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Trade Status"), Category = "PlayFab|Async|Trading")
	static UEPFAsyncGetTradeStatus* GetTradeStatus(UObject* WorldContext, const FString& TradeId, const FString& OfferingPlayerId);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncTradeStatusSuccess OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString TargetTradeId; FString OfferingId;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FEPFTradeInfo& Trade);
};
