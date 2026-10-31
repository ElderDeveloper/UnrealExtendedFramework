// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "ContentDelivery/EPFContentDeliverySubsystem.h"
#include "EPFAsyncContentDelivery.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncContentUrlSuccess, const FString&, Url);

// ── Get Content URL ──────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Get Content Download URL"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetContentUrl : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Content Download URL"), Category = "PlayFab|Async|Content")
	static UEPFAsyncGetContentUrl* GetContentDownloadUrl(UObject* WorldContext, const FString& Key, bool bThruCDN = true);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncContentUrlSuccess OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString ContentKey; bool bCDN = true;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& Url);
};

// ── Download Content ─────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Download Content"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncDownloadContent : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Download Content As String"), Category = "PlayFab|Async|Content")
	static UEPFAsyncDownloadContent* DownloadContentAsString(UObject* WorldContext, const FString& Key);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncContentUrlSuccess OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString ContentKey;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& Content);
};
