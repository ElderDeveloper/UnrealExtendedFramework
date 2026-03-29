// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFPushSubsystem.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFPushRegistered, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFPushSent, const FEPFResult&, Result);

/**
 * Push Notifications — register device tokens and send push notifications.
 * Supports iOS (APNs) and Android (FCM/GCM).
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFPushSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Register the current device for iOS push notifications */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Push")
	void RegisterForIOSPush(const FString& DeviceToken, bool bSendPushNotificationConfirmation = false);

	/** Register the current device for Android push notifications */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Push")
	void RegisterForAndroidPush(const FString& DeviceToken, bool bSendPushNotificationConfirmation = false);

	/** Send a push notification to another player */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Push")
	void SendPushNotification(const FString& RecipientPlayFabId, const FString& Message, const FString& Subject = TEXT(""));

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Push")
	FOnEPFPushRegistered OnPushRegistered;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Push")
	FOnEPFPushSent OnPushSent;
};
