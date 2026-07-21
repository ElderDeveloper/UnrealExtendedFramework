// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFMessagesSubsystem.generated.h"


USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFPlayerMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Messages")
	FString MessageId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Messages")
	FString Subject;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Messages")
	FString Body;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Messages")
	FString Sender;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Messages")
	FString Timestamp;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Messages")
	bool bRead = false;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFMessagesReceived, const FEPFResult&, Result, const TArray<FEPFPlayerMessage>&, Messages);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFMessageMarkedRead, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFNewMessagesAvailable, int32, UnreadCount);

/**
 * Player Messages / Inbox — poll-based notification system for PC games.
 * Messages are stored as internal player data (prefixed "msg_") and managed
 * via CloudScript for sending. This subsystem handles the client-side of
 * fetching, reading, and managing the inbox.
 *
 * To send messages, configure a CloudScript function like "SendMessage" that
 * writes to the recipient's internal data.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFMessagesSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Fetch all messages from the player's inbox */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Messages")
	void FetchMessages();

	/** Mark a message as read */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Messages")
	void MarkAsRead(const FString& MessageId);

	/** Delete a message from the inbox */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Messages")
	void DeleteMessage(const FString& MessageId);

	/**
	 * Start automatic inbox polling.
	 * @param IntervalSeconds  Polling interval (minimum 30s to avoid API throttling).
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Messages")
	void StartPolling(float IntervalSeconds = 60.0f);

	/** Stop inbox polling */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Messages")
	void StopPolling();

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get cached messages */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Messages")
	TArray<FEPFPlayerMessage> GetCachedMessages() const;

	/** Get number of unread messages */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Messages")
	int32 GetUnreadCount() const;

	/** Check if currently polling */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Messages")
	bool IsPolling() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Messages")
	FOnEPFMessagesReceived OnMessagesReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Messages")
	FOnEPFMessageMarkedRead OnMessageMarkedRead;

	/** Fired when polling detects new unread messages */
	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Messages")
	FOnEPFNewMessagesAvailable OnNewMessagesAvailable;

private:

	TArray<FEPFPlayerMessage> CachedMessages;
	int32 PreviousUnreadCount = 0;
	FTimerHandle PollTimerHandle;

	static const FString MessageKeyPrefix;

	void PollTick();
	void ParseMessages(const TMap<FString, FString>& RawData);
};
