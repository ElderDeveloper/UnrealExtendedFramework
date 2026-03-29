// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSChatSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FEEOSChatMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "EOS|Chat")
	FString SenderId;

	UPROPERTY(BlueprintReadWrite, Category = "EOS|Chat")
	FString SenderDisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "EOS|Chat")
	FString Message;

	UPROPERTY(BlueprintReadWrite, Category = "EOS|Chat")
	FString ChannelName;

	UPROPERTY(BlueprintReadWrite, Category = "EOS|Chat")
	FDateTime Timestamp;
};

USTRUCT(BlueprintType)
struct FEEOSChatChannel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "EOS|Chat")
	FString ChannelName;

	UPROPERTY(BlueprintReadWrite, Category = "EOS|Chat")
	int32 MemberCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "EOS|Chat")
	bool bIsJoined = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSChatMessageReceived, const FEEOSChatMessage&, Message, const FString&, ChannelName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSChatChannelJoined, bool, bSuccess, const FString&, ChannelName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSChatChannelLeft, const FString&, ChannelName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSChatMessageSent, bool, bSuccess, const FString&, ChannelName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSChatUserJoined, const FString&, UserId, const FString&, ChannelName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSChatUserLeft, const FString&, UserId, const FString&, ChannelName);

/**
 * Text chat system with channel management.
 * Supports channel join/leave, message send/receive, and chat history.
 * 
 * Note: Full implementation requires direct EOS SDK chat calls or 
 * integration with the IOnlineChat interface (if available).
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSChatSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Channel Management ───────────────────────────────────────────────────

	/** Join a chat channel */
	UFUNCTION(BlueprintCallable, Category = "EOS|Chat")
	void JoinChannel(const FString& ChannelName);

	/** Leave a chat channel */
	UFUNCTION(BlueprintCallable, Category = "EOS|Chat")
	void LeaveChannel(const FString& ChannelName);

	/** Leave all joined channels */
	UFUNCTION(BlueprintCallable, Category = "EOS|Chat")
	void LeaveAllChannels();

	// ── Messaging ────────────────────────────────────────────────────────────

	/** Send a text message to a channel */
	UFUNCTION(BlueprintCallable, Category = "EOS|Chat")
	void SendMessage(const FString& ChannelName, const FString& Message);

	/** Send a direct message to a specific user */
	UFUNCTION(BlueprintCallable, Category = "EOS|Chat")
	void SendDirectMessage(const FString& TargetUserId, const FString& Message);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get all joined channels */
	UFUNCTION(BlueprintPure, Category = "EOS|Chat")
	TArray<FEEOSChatChannel> GetJoinedChannels() const;

	/** Check if currently joined to a channel */
	UFUNCTION(BlueprintPure, Category = "EOS|Chat")
	bool IsInChannel(const FString& ChannelName) const;

	/** Get the chat history for a channel */
	UFUNCTION(BlueprintPure, Category = "EOS|Chat")
	TArray<FEEOSChatMessage> GetChannelHistory(const FString& ChannelName, int32 MaxMessages = 50) const;

	/** Get the total number of unread messages across all channels */
	UFUNCTION(BlueprintPure, Category = "EOS|Chat")
	int32 GetUnreadMessageCount() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|Chat")
	FOnEOSChatMessageReceived OnChatMessageReceived;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Chat")
	FOnEOSChatChannelJoined OnChannelJoined;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Chat")
	FOnEOSChatChannelLeft OnChannelLeft;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Chat")
	FOnEOSChatMessageSent OnMessageSent;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Chat")
	FOnEOSChatUserJoined OnUserJoined;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Chat")
	FOnEOSChatUserLeft OnUserLeft;

private:

	/** Joined channels */
	TMap<FString, FEEOSChatChannel> JoinedChannels;

	/** Chat history per channel */
	TMap<FString, TArray<FEEOSChatMessage>> ChannelHistory;

	int32 UnreadCount = 0;

	/** Whether IOnlineChat is available for real networked chat */
	bool bUsingOnlineChat = false;

	/** Delegate handles for proper cleanup */
	FDelegateHandle ChatMessageReceivedHandle;
	FDelegateHandle ChatRoomJoinHandle;
	FDelegateHandle ChatRoomExitHandle;
	FDelegateHandle ChatMemberJoinHandle;
	FDelegateHandle ChatMemberExitHandle;
};

