// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "Matchmaking/EPFMatchmakingSubsystem.h"
#include "EPFAsyncMatchmaking.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncTicketCreatedSuccess, const FString&, TicketId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncTicketStatusSuccess, const FEPFMatchmakingResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncMatchCanceled);

// ── Create Ticket ────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Create Matchmaking Ticket"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncCreateTicket : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Create Matchmaking Ticket"), Category = "PlayFab|Async|Matchmaking")
	static UEPFAsyncCreateTicket* CreateTicket(UObject* WorldContext, const FString& QueueName, const TMap<FString, FString>& Attributes, int32 GiveUpAfterSeconds = 120);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncTicketCreatedSuccess OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	// GiveUpAfterSeconds is PlayFab's ticket lifetime, not a node timeout: the node finishes as soon as the ticket exists.
	FString TicketQueueName; TMap<FString, FString> TicketAttributes; int32 Timeout = 120;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& TicketId);
};

// ── Get Ticket Status ────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Get Matchmaking Status"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetTicketStatus : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Matchmaking Status"), Category = "PlayFab|Async|Matchmaking")
	static UEPFAsyncGetTicketStatus* GetTicketStatus(UObject* WorldContext, const FString& QueueName, const FString& TicketId);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncTicketStatusSuccess OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString TicketQueueName; FString TargetTicketId;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FEPFMatchmakingResult& MatchmakingResult);
};

// ── Cancel Matchmaking ───────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Cancel Matchmaking"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncCancelMatchmaking : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Cancel Matchmaking"), Category = "PlayFab|Async|Matchmaking")
	static UEPFAsyncCancelMatchmaking* CancelMatchmaking(UObject* WorldContext, const FString& QueueName, const FString& TicketId);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncMatchCanceled OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString TicketQueueName; FString TargetTicketId;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};
