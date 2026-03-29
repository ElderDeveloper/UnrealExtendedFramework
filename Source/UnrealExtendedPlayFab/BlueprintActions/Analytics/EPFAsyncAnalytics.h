// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "Shared/EPFTypes.h"
#include "EPFAsyncAnalytics.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncEventLogged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncBatchEventsLogged);

// ── Log Single Event ─────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Log Event"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncLogEvent : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Log Event"), Category = "PlayFab|Async|Analytics")
	static UEPFAsyncLogEvent* LogEvent(UObject* WorldContext, const FString& EventName, const TMap<FString, FString>& Body);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncEventLogged OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;

private:
	FString EventName;
	TMap<FString, FString> Body;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};

// ── Log Batch Events ─────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Log Events (Batch)"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncLogEvents : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Log Events (Batch)"), Category = "PlayFab|Async|Analytics")
	static UEPFAsyncLogEvents* LogEvents(UObject* WorldContext, const TArray<FEPFAnalyticsEvent>& Events);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncBatchEventsLogged OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;

private:
	TArray<FEPFAnalyticsEvent> Events;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};
