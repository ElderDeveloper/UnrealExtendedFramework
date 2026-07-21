// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncPush.h"
#include "PushNotifications/EPFPushSubsystem.h"
#include "Engine/GameInstance.h"


// ── Register Push ────────────────────────────────────────────────────────────

UEPFAsyncRegisterPush* UEPFAsyncRegisterPush::RegisterForIOSPush(UObject* WorldContext, const FString& DeviceToken)
{
	auto* Action = NewObject<UEPFAsyncRegisterPush>();
	Action->Platform = IOS;
	Action->DeviceToken = DeviceToken;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

UEPFAsyncRegisterPush* UEPFAsyncRegisterPush::RegisterForAndroidPush(UObject* WorldContext, const FString& DeviceToken)
{
	auto* Action = NewObject<UEPFAsyncRegisterPush>();
	Action->Platform = Android;
	Action->DeviceToken = DeviceToken;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncRegisterPush::Activate()
{
	auto* Sub = GetEPFSubsystemFromContext<UEPFPushSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Push subsystem not available"))); SetReadyToDestroy(); return; }
	if (DeviceToken.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Device token cannot be empty"))); SetReadyToDestroy(); return; }

	Sub->OnPushRegistered.AddDynamic(this, &UEPFAsyncRegisterPush::HandleComplete);

	switch (Platform)
	{
	case IOS: Sub->RegisterForIOSPush(DeviceToken); break;
	case Android: Sub->RegisterForAndroidPush(DeviceToken); break;
	}
}

void UEPFAsyncRegisterPush::HandleComplete(const FEPFResult& Result)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFPushSubsystem>(WorldContext.Get()))
		Sub->OnPushRegistered.RemoveDynamic(this, &UEPFAsyncRegisterPush::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Push registration failed")));
	SetReadyToDestroy();
}


// ── Send Push ────────────────────────────────────────────────────────────────

UEPFAsyncSendPush* UEPFAsyncSendPush::SendPushNotification(UObject* WorldContext, const FString& RecipientPlayFabId, const FString& Message, const FString& Subject)
{
	auto* Action = NewObject<UEPFAsyncSendPush>();
	Action->RecipientId = RecipientPlayFabId;
	Action->Message = Message;
	Action->Subject = Subject;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncSendPush::Activate()
{
	auto* Sub = GetEPFSubsystemFromContext<UEPFPushSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Push subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnPushSent.AddDynamic(this, &UEPFAsyncSendPush::HandleComplete);
	Sub->SendPushNotification(RecipientId, Message, Subject);
}

void UEPFAsyncSendPush::HandleComplete(const FEPFResult& Result)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFPushSubsystem>(WorldContext.Get()))
		Sub->OnPushSent.RemoveDynamic(this, &UEPFAsyncSendPush::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to send push notification")));
	SetReadyToDestroy();
}
