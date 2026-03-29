// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFPushSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

void UEPFPushSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UEPFPushSubsystem::Deinitialize() { Super::Deinitialize(); }


// ── iOS Push Registration ────────────────────────────────────────────────────

void UEPFPushSubsystem::RegisterForIOSPush(const FString& DeviceToken, bool bSendPushNotificationConfirmation)
{
	if (DeviceToken.IsEmpty()) { OnPushRegistered.Broadcast(FEPFResult::Failure(TEXT("DeviceToken cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("DeviceToken"), DeviceToken);
	Body->SetBoolField(TEXT("SendPushNotificationConfirmation"), bSendPushNotificationConfirmation);

	SendPlayFabRequestDetailed(TEXT("/Client/RegisterForIOSPushNotification"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFPush — iOS push registered"));
			OnPushRegistered.Broadcast(Result);
		}));
}


// ── Android Push Registration ────────────────────────────────────────────────

void UEPFPushSubsystem::RegisterForAndroidPush(const FString& DeviceToken, bool bSendPushNotificationConfirmation)
{
	if (DeviceToken.IsEmpty()) { OnPushRegistered.Broadcast(FEPFResult::Failure(TEXT("DeviceToken cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("DeviceToken"), DeviceToken);
	Body->SetBoolField(TEXT("SendPushNotificationConfirmation"), bSendPushNotificationConfirmation);

	SendPlayFabRequestDetailed(TEXT("/Client/AndroidDevicePushNotificationRegistration"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFPush — Android push registered"));
			OnPushRegistered.Broadcast(Result);
		}));
}


// ── Send Push Notification ───────────────────────────────────────────────────

void UEPFPushSubsystem::SendPushNotification(const FString& RecipientPlayFabId, const FString& Message, const FString& Subject)
{
	if (RecipientPlayFabId.IsEmpty() || Message.IsEmpty()) { OnPushSent.Broadcast(FEPFResult::Failure(TEXT("RecipientPlayFabId and Message are required"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("Recipient"), RecipientPlayFabId);
	Body->SetStringField(TEXT("Message"), Message);
	if (!Subject.IsEmpty()) Body->SetStringField(TEXT("Subject"), Subject);

	SendPlayFabRequestDetailed(TEXT("/Client/SendPushNotification"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFPush — Push notification sent"));
			OnPushSent.Broadcast(Result);
		}));
}
