// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "EPFAsyncPush.generated.h"


// ── Register Push ────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Register Push Notification"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncRegisterPush : public UEPFAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Register iOS Push"), Category = "PlayFab|Async|Push")
	static UEPFAsyncRegisterPush* RegisterForIOSPush(UObject* WorldContext, const FString& DeviceToken);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Register Android Push"), Category = "PlayFab|Async|Push")
	static UEPFAsyncRegisterPush* RegisterForAndroidPush(UObject* WorldContext, const FString& DeviceToken);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncSimpleSuccess OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	enum EPlatform { IOS, Android };
	EPlatform Platform = IOS;
	FString PushDeviceToken;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};


// ── Send Push ────────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Send Push Notification"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncSendPush : public UEPFAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Send Push Notification"), Category = "PlayFab|Async|Push")
	static UEPFAsyncSendPush* SendPushNotification(UObject* WorldContext, const FString& RecipientPlayFabId, const FString& Message, const FString& Subject = TEXT(""));

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncSimpleSuccess OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString RecipientId, PushMessage, PushSubject;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};
