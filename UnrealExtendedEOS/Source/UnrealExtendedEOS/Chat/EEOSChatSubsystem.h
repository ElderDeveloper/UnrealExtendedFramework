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
 * LOCAL-ONLY chat room/history model.
 *
 * The engine's OnlineSubsystemEOS does NOT provide an IOnlineChat backend (EOS itself has
 * no text-chat service), so messages sent through this subsystem never leave the local
 * machine. What this subsystem actually provides is a local channel/history/unread model
 * suitable for prototyping chat UI: join/leave bookkeeping, per-channel message history,
 * and per-channel unread counters.
 *
 * For real networked chat use the game's own replicated chat system. The IOnlineChat
 * wiring below only activates if a future online subsystem supplies that interface;
 * on EOS every send completes with OnMessageSent(false, ...) — delivery is never faked.
 *
 * Entry points return bool: true → the operation started (async) or completed successfully
 * (its delegate has fired / will fire exactly once); false → the operation failed, with the
 * failure already signaled on the matching delegate where one exists.
 *
 * Direct-message history and unread counters are keyed "DM_<ProductUserId>" — composite
 * ("<EAS>|<PUID>") and bare-PUID target ids land in the same conversation.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSChatSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Channel Management ───────────────────────────────────────────────────

	/** Join a chat channel. Completion arrives on OnChannelJoined (already-joined channels
	 *  re-broadcast success immediately). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Chat")
	bool JoinChannel(const FString& ChannelName);

	/** Leave a chat channel. Clears the channel's unread counter. Returns false when not in
	 *  the channel (no broadcast) or when the engine refused the exit (the channel is then
	 *  dropped locally and OnChannelLeft still fires). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Chat")
	bool LeaveChannel(const FString& ChannelName);

	/** Leave all joined channels */
	UFUNCTION(BlueprintCallable, Category = "EOS|Chat")
	void LeaveAllChannels();

	// ── Messaging ────────────────────────────────────────────────────────────

	/** Send a text message to a channel. The result (real delivery, never faked) is also
	 *  broadcast on OnMessageSent. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Chat")
	bool SendMessage(const FString& ChannelName, const FString& Message);

	/** Send a direct message to a specific user (composite or bare-PUID id — both map to the
	 *  same DM conversation). The result is also broadcast on OnMessageSent. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Chat")
	bool SendDirectMessage(const FString& TargetUserId, const FString& Message);

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

	/** Get the total number of unread messages across all channels. Messages sent by the
	 *  local user are never counted. Reset per channel with MarkChannelRead / MarkAllRead. */
	UFUNCTION(BlueprintPure, Category = "EOS|Chat")
	int32 GetUnreadMessageCount() const;

	/** Reset the unread counter for one channel (call when the channel's UI is viewed) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Chat")
	void MarkChannelRead(const FString& ChannelName);

	/** Reset the unread counters of all channels */
	UFUNCTION(BlueprintCallable, Category = "EOS|Chat")
	void MarkAllRead();

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

	/** Unread message count per channel (only messages from OTHER users are counted) */
	TMap<FString, int32> UnreadCounts;

	/** Whether IOnlineChat is available for real networked chat */
	bool bUsingOnlineChat = false;

	/** Delegate handles for proper cleanup */
	FDelegateHandle ChatMessageReceivedHandle;
	FDelegateHandle ChatRoomJoinHandle;
	FDelegateHandle ChatRoomExitHandle;
	FDelegateHandle ChatMemberJoinHandle;
	FDelegateHandle ChatMemberExitHandle;
};

