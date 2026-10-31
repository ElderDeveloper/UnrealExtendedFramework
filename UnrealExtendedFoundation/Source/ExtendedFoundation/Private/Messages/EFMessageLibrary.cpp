// Copyright Moon Punch Games. All Rights Reserved.

#include "Messages/EFMessageLibrary.h"

#include "ExtendedFoundationModule.h"

void UEFMessageLibrary::BroadcastMessage(const UObject* WorldContextObject, FGameplayTag Channel, const FInstancedStruct& Payload)
{
	if (UEFMessageSubsystem* Messages = UEFMessageSubsystem::Get(WorldContextObject))
	{
		Messages->BroadcastMessage(Channel, Payload);
	}
	else
	{
		UE_LOG(LogExtendedFoundation, Warning, TEXT("BroadcastMessage %s: no game instance for %s."), *Channel.ToString(), *GetNameSafe(WorldContextObject));
	}
}

int32 UEFMessageLibrary::CountMessageListeners(const UObject* WorldContextObject, FGameplayTag Channel)
{
	const UEFMessageSubsystem* Messages = UEFMessageSubsystem::Get(WorldContextObject);
	return Messages ? Messages->CountListeners(Channel) : 0;
}

TArray<FEFMessageRecord> UEFMessageLibrary::GetRecentMessages(const UObject* WorldContextObject)
{
	const UEFMessageSubsystem* Messages = UEFMessageSubsystem::Get(WorldContextObject);
	return Messages ? Messages->GetRecentMessages() : TArray<FEFMessageRecord>();
}

UEFAsyncListenForMessage* UEFAsyncListenForMessage::ListenForMessage(UObject* WorldContextObject, FGameplayTag Channel, EEFMessageMatch Match, UScriptStruct* PayloadType)
{
	UEFAsyncListenForMessage* Action = NewObject<UEFAsyncListenForMessage>();
	Action->ContextObject = WorldContextObject;
	Action->ListenChannel = Channel;
	Action->ListenMatch = Match;
	Action->ListenPayloadType = PayloadType;
	if (WorldContextObject)
	{
		Action->RegisterWithGameInstance(WorldContextObject);
	}
	return Action;
}

void UEFAsyncListenForMessage::Activate()
{
	UEFMessageSubsystem* Messages = UEFMessageSubsystem::Get(ContextObject.Get());
	if (!Messages || !ListenChannel.IsValid())
	{
		UE_LOG(LogExtendedFoundation, Warning, TEXT("Listen For Message: %s; the node stops."),
			Messages ? TEXT("the channel is empty") : TEXT("no game instance for the world context"));
		SetReadyToDestroy();
		return;
	}

	TWeakObjectPtr<UEFAsyncListenForMessage> WeakThis(this);
	Handle = Messages->RegisterListener(ListenChannel, [WeakThis](FGameplayTag ActualChannel, const FInstancedStruct& Payload)
	{
		if (UEFAsyncListenForMessage* This = WeakThis.Get())
		{
			This->HandleMessage(ActualChannel, Payload);
		}
	}, ListenMatch, ListenPayloadType.Get(), this);
}

void UEFAsyncListenForMessage::HandleMessage(FGameplayTag ActualChannel, const FInstancedStruct& Payload)
{
	if (!ContextObject.IsValid())
	{
		// The widget or actor that started listening is gone: stop instead of calling into it.
		Cancel();
		return;
	}
	OnMessageReceived.Broadcast(ActualChannel, Payload);
}

void UEFAsyncListenForMessage::Cancel()
{
	Handle.Unregister();
	Super::Cancel();
}

void UEFAsyncListenForMessage::SetReadyToDestroy()
{
	Handle.Unregister();
	Super::SetReadyToDestroy();
}
