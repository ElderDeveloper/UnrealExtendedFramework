// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "ContentDelivery/EPFContentDeliverySubsystem.h"
#include "EPFAsyncContentDelivery.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncContentUrlSuccess, const FString&, Url);

// ── Get Content URL ──────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Get Content Download URL"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetContentUrl : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Content Download URL"), Category = "PlayFab|Async|Content")
	static UEPFAsyncGetContentUrl* GetContentDownloadUrl(UObject* WorldContext, const FString& Key, bool bThruCDN = true);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncContentUrlSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString Key; bool bCDN; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& Url);
};

// ── Download Content ─────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Download Content"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncDownloadContent : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Download Content As String"), Category = "PlayFab|Async|Content")
	static UEPFAsyncDownloadContent* DownloadContentAsString(UObject* WorldContext, const FString& Key);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncContentUrlSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString Key; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& Content);
};
