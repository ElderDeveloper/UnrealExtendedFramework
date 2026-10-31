// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncPush.h"
#include "PushNotifications/EPFPushSubsystem.h"


// ── Register Push ────────────────────────────────────────────────────────────

UEPFAsyncRegisterPush* UEPFAsyncRegisterPush::RegisterForIOSPush(UObject* WorldContext, const FString& DeviceToken)
{
	auto* Action = NewObject<UEPFAsyncRegisterPush>();
	Action->Platform = IOS;
	Action->PushDeviceToken = DeviceToken;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

UEPFAsyncRegisterPush* UEPFAsyncRegisterPush::RegisterForAndroidPush(UObject* WorldContext, const FString& DeviceToken)
{
	auto* Action = NewObject<UEPFAsyncRegisterPush>();
	Action->Platform = Android;
	Action->PushDeviceToken = DeviceToken;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncRegisterPush::ExecuteAction()
{
	auto* Sub = RequireGameInstanceSubsystem<UEPFPushSubsystem>();
	if (!Sub) return;
	if (PushDeviceToken.IsEmpty()) { FailInvalidArgument(TEXT("Device token cannot be empty")); return; }

	Sub->OnPushRegistered.AddDynamic(this, &UEPFAsyncRegisterPush::HandleComplete);

	CallWithContext([&]
	{
		switch (Platform)
		{
		case IOS: Sub->RegisterForIOSPush(PushDeviceToken); break;
		case Android: Sub->RegisterForAndroidPush(PushDeviceToken); break;
		}
	});
}

void UEPFAsyncRegisterPush::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFPushSubsystem>())
		Sub->OnPushRegistered.RemoveDynamic(this, &UEPFAsyncRegisterPush::HandleComplete);
}

void UEPFAsyncRegisterPush::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Push registration failed")));
}


// ── Send Push ────────────────────────────────────────────────────────────────

UEPFAsyncSendPush* UEPFAsyncSendPush::SendPushNotification(UObject* WorldContext, const FString& RecipientPlayFabId, const FString& Message, const FString& Subject)
{
	auto* Action = NewObject<UEPFAsyncSendPush>();
	Action->RecipientId = RecipientPlayFabId;
	Action->PushMessage = Message;
	Action->PushSubject = Subject;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncSendPush::ExecuteAction()
{
	auto* Sub = RequireGameInstanceSubsystem<UEPFPushSubsystem>();
	if (!Sub) return;
	Sub->OnPushSent.AddDynamic(this, &UEPFAsyncSendPush::HandleComplete);
	CallWithContext([&] { Sub->SendPushNotification(RecipientId, PushMessage, PushSubject); });
}

void UEPFAsyncSendPush::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFPushSubsystem>())
		Sub->OnPushSent.RemoveDynamic(this, &UEPFAsyncSendPush::HandleComplete);
}

void UEPFAsyncSendPush::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to send push notification")));
}
