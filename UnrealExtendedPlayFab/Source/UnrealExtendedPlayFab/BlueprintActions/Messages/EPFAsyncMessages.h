// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "Messages/EPFMessagesSubsystem.h"
#include "EPFAsyncMessages.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncMessagesSuccess, const TArray<FEPFPlayerMessage>&, Messages);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncMessageAction);

// ── Fetch Messages ───────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Fetch Messages"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncFetchMessages : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Fetch Messages"), Category = "PlayFab|Async|Messages")
	static UEPFAsyncFetchMessages* FetchMessages(UObject* WorldContext);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncMessagesSuccess OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFPlayerMessage>& Messages);
};

// ── Mark As Read ─────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Mark Message Read"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncMarkMessageRead : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Mark Message Read"), Category = "PlayFab|Async|Messages")
	static UEPFAsyncMarkMessageRead* MarkMessageRead(UObject* WorldContext, const FString& MessageId);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncMessageAction OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString TargetMessageId;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};

// ── Delete Message ───────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Delete Message"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncDeleteMessage : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Delete Message"), Category = "PlayFab|Async|Messages")
	static UEPFAsyncDeleteMessage* DeleteMessage(UObject* WorldContext, const FString& MessageId);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncMessageAction OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString TargetMessageId;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};
