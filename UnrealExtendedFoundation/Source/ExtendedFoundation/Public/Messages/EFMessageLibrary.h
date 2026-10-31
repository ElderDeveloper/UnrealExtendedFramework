// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/CancellableAsyncAction.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Messages/EFMessageSubsystem.h"
#include "EFMessageLibrary.generated.h"

UCLASS(meta = (ScriptName = "EFMessageLibrary"))
class EXTENDEDFOUNDATION_API UEFMessageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Sends a message to everything listening on Channel in this game instance. Build the payload
	 * with Make Instanced Struct, or leave it empty for a message that carries no data.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Messages", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Payload"))
	static void BroadcastMessage(const UObject* WorldContextObject, FGameplayTag Channel, const FInstancedStruct& Payload);

	/** Listeners that would receive a message sent on Channel now. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Messages", meta = (WorldContext = "WorldContextObject"))
	static int32 CountMessageListeners(const UObject* WorldContextObject, FGameplayTag Channel);

	/** The most recent messages, oldest first, for debug UI. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Messages", meta = (WorldContext = "WorldContextObject"))
	static TArray<FEFMessageRecord> GetRecentMessages(const UObject* WorldContextObject);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEFOnMessageReceivedPin, FGameplayTag, Channel, const FInstancedStruct&, Payload);

/**
 * Listen For Message: fires On Message Received for every message on the channel until it is
 * cancelled through its Async Task pin or its world context object is destroyed.
 */
UCLASS()
class EXTENDEDFOUNDATION_API UEFAsyncListenForMessage : public UCancellableAsyncAction
{
	GENERATED_BODY()

public:
	/** Channel is the channel the message was sent on, which differs from the one listened to with Partial Match. */
	UPROPERTY(BlueprintAssignable)
	FEFOnMessageReceivedPin OnMessageReceived;

	/**
	 * Listens on Channel. With Payload Type set, only messages carrying that struct are delivered;
	 * read the payload with Get Instanced Struct Value.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Messages", meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true", DisplayName = "Listen For Message"))
	static UEFAsyncListenForMessage* ListenForMessage(UObject* WorldContextObject, FGameplayTag Channel,
		EEFMessageMatch Match = EEFMessageMatch::ExactMatch, UScriptStruct* PayloadType = nullptr);

	virtual void Activate() override;
	virtual void Cancel() override;
	virtual void SetReadyToDestroy() override;

private:
	void HandleMessage(FGameplayTag ActualChannel, const FInstancedStruct& Payload);

	TWeakObjectPtr<UObject> ContextObject;
	FGameplayTag ListenChannel;
	EEFMessageMatch ListenMatch = EEFMessageMatch::ExactMatch;
	TWeakObjectPtr<UScriptStruct> ListenPayloadType;
	FEFMessageListenerHandle Handle;
};
