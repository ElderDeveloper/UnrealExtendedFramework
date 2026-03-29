// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "EPFAsyncPush.generated.h"


// ── Register Push ────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Register Push Notification"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncRegisterPush : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Register iOS Push"), Category = "PlayFab|Async|Push")
	static UEPFAsyncRegisterPush* RegisterForIOSPush(UObject* WorldContext, const FString& DeviceToken);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Register Android Push"), Category = "PlayFab|Async|Push")
	static UEPFAsyncRegisterPush* RegisterForAndroidPush(UObject* WorldContext, const FString& DeviceToken);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncSimpleSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;

	virtual void Activate() override;

private:
	enum EPlatform { IOS, Android };
	EPlatform Platform;
	FString DeviceToken;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};


// ── Send Push ────────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Send Push Notification"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncSendPush : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Send Push Notification"), Category = "PlayFab|Async|Push")
	static UEPFAsyncSendPush* SendPushNotification(UObject* WorldContext, const FString& RecipientPlayFabId, const FString& Message, const FString& Subject = TEXT(""));

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncSimpleSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;

	virtual void Activate() override;

private:
	FString RecipientId, Message, Subject;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};
