// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "AccountLinking/EPFAccountLinkingSubsystem.h"
#include "EPFAsyncAccountLinking.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncLinkSuccess, const FString&, Platform);

// ── Link Steam ───────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Link Steam Account"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncLinkSteam : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Link Steam Account"), Category = "PlayFab|Async|AccountLinking")
	static UEPFAsyncLinkSteam* LinkSteamAccount(UObject* WorldContext, const FString& SteamTicket, bool bForceLink = false);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncLinkSuccess OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString Ticket; bool bForce = false;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& Platform);
};

// ── Link Custom ID ───────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Link Custom ID"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncLinkCustomId : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Link Custom ID"), Category = "PlayFab|Async|AccountLinking")
	static UEPFAsyncLinkCustomId* LinkCustomId(UObject* WorldContext, const FString& CustomId, bool bForceLink = false);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncLinkSuccess OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString Id; bool bForce = false;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& Platform);
};

// ── Unlink Steam ─────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Unlink Steam Account"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncUnlinkSteam : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Unlink Steam Account"), Category = "PlayFab|Async|AccountLinking")
	static UEPFAsyncUnlinkSteam* UnlinkSteamAccount(UObject* WorldContext);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncLinkSuccess OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& Platform);
};
