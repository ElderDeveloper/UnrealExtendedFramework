// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSChatSubsystem.h"
#include "UnrealExtendedEOS.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineChatInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"

void UEEOSChatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Register for incoming chat messages if the interface is available
	if (IsEOSAvailable())
	{
		IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
		IOnlineChatPtr ChatInterface = EOSSub->GetChatInterface();
		if (ChatInterface.IsValid())
		{
			ChatMessageReceivedHandle = ChatInterface->AddOnChatRoomMessageReceivedDelegate_Handle(
				FOnChatRoomMessageReceivedDelegate::CreateLambda(
				[this](const FUniqueNetId& UserId, const FChatRoomId& RoomId, const TSharedRef<FChatMessage>& ChatMessage)
				{
					FString ChannelName = RoomId;
					FString SenderId = ChatMessage->GetUserId()->ToString();
					FString SenderName = ChatMessage->GetNickname();
					FString MessageBody = ChatMessage->GetBody();

					FEEOSChatMessage Msg;
					Msg.SenderId = SenderId;
					Msg.SenderDisplayName = SenderName;
					Msg.Message = MessageBody;
					Msg.ChannelName = ChannelName;
					Msg.Timestamp = ChatMessage->GetTimestamp();

					// Store in history
					TArray<FEEOSChatMessage>& History = ChannelHistory.FindOrAdd(ChannelName);
					History.Add(Msg);

					if (History.Num() > 200)
					{
						History.RemoveAt(0, History.Num() - 200);
					}

					UnreadCount++;
					OnChatMessageReceived.Broadcast(Msg, ChannelName);
				}));

			ChatRoomJoinHandle = ChatInterface->AddOnChatRoomJoinPublicDelegate_Handle(
				FOnChatRoomJoinPublicDelegate::CreateLambda(
				[this](const FUniqueNetId& UserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error)
				{
					FString ChannelName = RoomId;
					if (bWasSuccessful)
					{
						FEEOSChatChannel Channel;
						Channel.ChannelName = ChannelName;
						Channel.bIsJoined = true;
						Channel.MemberCount = 1;
						JoinedChannels.Add(ChannelName, Channel);

						if (!ChannelHistory.Contains(ChannelName))
						{
							ChannelHistory.Add(ChannelName, TArray<FEEOSChatMessage>());
						}

						UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: Joined channel '%s' via EOS"), *ChannelName);
					}
					else
					{
						UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem: Failed to join channel '%s' — %s"), *ChannelName, *Error);
					}

					OnChannelJoined.Broadcast(bWasSuccessful, ChannelName);
				}));

			ChatRoomExitHandle = ChatInterface->AddOnChatRoomExitDelegate_Handle(
				FOnChatRoomExitDelegate::CreateLambda(
				[this](const FUniqueNetId& UserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error)
				{
					if (bWasSuccessful)
					{
						FString ChannelName = RoomId;
						JoinedChannels.Remove(ChannelName);
						UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: Left channel '%s' via EOS"), *ChannelName);
						OnChannelLeft.Broadcast(ChannelName);
					}
				}));

			ChatMemberJoinHandle = ChatInterface->AddOnChatRoomMemberJoinDelegate_Handle(
				FOnChatRoomMemberJoinDelegate::CreateLambda(
				[this](const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FUniqueNetId& MemberId)
				{
					FString ChannelName = RoomId;
					FString UserIdStr = MemberId.ToString();
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: User '%s' joined channel '%s'"), *UserIdStr, *ChannelName);
					OnUserJoined.Broadcast(UserIdStr, ChannelName);
				}));

			ChatMemberExitHandle = ChatInterface->AddOnChatRoomMemberExitDelegate_Handle(
				FOnChatRoomMemberExitDelegate::CreateLambda(
				[this](const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FUniqueNetId& MemberId)
				{
					FString ChannelName = RoomId;
					FString UserIdStr = MemberId.ToString();
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: User '%s' left channel '%s'"), *UserIdStr, *ChannelName);
					OnUserLeft.Broadcast(UserIdStr, ChannelName);
				}));

			bUsingOnlineChat = true;
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem initialized with IOnlineChat interface"));
		}
		else
		{
			bUsingOnlineChat = false;
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem: IOnlineChat not available — send/receive will not transmit over network. "
				"Consider using P2P subsystem for text messaging if the chat interface is not supported."));
		}
	}
}

void UEEOSChatSubsystem::Deinitialize()
{
	// Clean up delegate handles
	if (IsEOSAvailable())
	{
		IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
		if (EOSSub)
		{
			IOnlineChatPtr ChatInterface = EOSSub->GetChatInterface();
			if (ChatInterface.IsValid())
			{
				ChatInterface->ClearOnChatRoomMessageReceivedDelegate_Handle(ChatMessageReceivedHandle);
				ChatInterface->ClearOnChatRoomJoinPublicDelegate_Handle(ChatRoomJoinHandle);
				ChatInterface->ClearOnChatRoomExitDelegate_Handle(ChatRoomExitHandle);
				ChatInterface->ClearOnChatRoomMemberJoinDelegate_Handle(ChatMemberJoinHandle);
				ChatInterface->ClearOnChatRoomMemberExitDelegate_Handle(ChatMemberExitHandle);
			}
		}
	}

	LeaveAllChannels();
	ChannelHistory.Empty();
	Super::Deinitialize();
}

// ── Channel Management ───────────────────────────────────────────────────────

void UEEOSChatSubsystem::JoinChannel(const FString& ChannelName)
{
	if (JoinedChannels.Contains(ChannelName))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::JoinChannel — Already joined '%s'"), *ChannelName);
		OnChannelJoined.Broadcast(true, ChannelName);
		return;
	}

	if (bUsingOnlineChat)
	{
		IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
		IOnlineChatPtr ChatInterface = EOSSub->GetChatInterface();
		FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
		if (ChatInterface.IsValid() && UserId.IsValid())
		{
			// The delegate callback registered in Initialize() will handle success/failure
			FString PlayerNickname = EOSSub->GetIdentityInterface()->GetPlayerNickname(0);
			ChatInterface->JoinPublicRoom(*UserId, FChatRoomId(ChannelName), PlayerNickname, FChatRoomConfig());
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: Joining channel '%s' via EOS..."), *ChannelName);
			return;
		}
	}

	// Fallback: no online chat interface
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::JoinChannel — IOnlineChat not available, channel '%s' is local-only"), *ChannelName);
	FEEOSChatChannel Channel;
	Channel.ChannelName = ChannelName;
	Channel.bIsJoined = true;
	Channel.MemberCount = 1;
	JoinedChannels.Add(ChannelName, Channel);

	if (!ChannelHistory.Contains(ChannelName))
	{
		ChannelHistory.Add(ChannelName, TArray<FEEOSChatMessage>());
	}

	OnChannelJoined.Broadcast(true, ChannelName);
}

void UEEOSChatSubsystem::LeaveChannel(const FString& ChannelName)
{
	if (!JoinedChannels.Contains(ChannelName))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::LeaveChannel — Not in channel '%s'"), *ChannelName);
		return;
	}

	if (bUsingOnlineChat)
	{
		IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
		IOnlineChatPtr ChatInterface = EOSSub->GetChatInterface();
		FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
		if (ChatInterface.IsValid() && UserId.IsValid())
		{
			ChatInterface->ExitRoom(*UserId, FChatRoomId(ChannelName));
			// Delegate callback handles JoinedChannels removal and OnChannelLeft broadcast
			return;
		}
	}

	// Fallback: local-only
	JoinedChannels.Remove(ChannelName);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: Left channel '%s'"), *ChannelName);
	OnChannelLeft.Broadcast(ChannelName);
}

void UEEOSChatSubsystem::LeaveAllChannels()
{
	TArray<FString> ChannelNames;
	JoinedChannels.GetKeys(ChannelNames);

	for (const FString& Name : ChannelNames)
	{
		LeaveChannel(Name);
	}
}

// ── Messaging ────────────────────────────────────────────────────────────────

void UEEOSChatSubsystem::SendMessage(const FString& ChannelName, const FString& Message)
{
	if (!JoinedChannels.Contains(ChannelName))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::SendMessage — Not in channel '%s'"), *ChannelName);
		OnMessageSent.Broadcast(false, ChannelName);
		return;
	}

	if (bUsingOnlineChat)
	{
		IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
		IOnlineChatPtr ChatInterface = EOSSub->GetChatInterface();
		FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
		if (ChatInterface.IsValid() && UserId.IsValid())
		{
			bool bSent = ChatInterface->SendRoomChat(*UserId, FChatRoomId(ChannelName), Message);
			if (bSent)
			{
				// Add to local history
				FEEOSChatMessage ChatMsg;
				ChatMsg.SenderId = UserId->ToString();
				ChatMsg.SenderDisplayName = EOSSub->GetIdentityInterface()->GetPlayerNickname(0);
				ChatMsg.Message = Message;
				ChatMsg.ChannelName = ChannelName;
				ChatMsg.Timestamp = FDateTime::UtcNow();

				TArray<FEEOSChatMessage>& History = ChannelHistory.FindOrAdd(ChannelName);
				History.Add(ChatMsg);

				if (History.Num() > 200)
				{
					History.RemoveAt(0, History.Num() - 200);
				}

				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: Sent message in '%s' via EOS"), *ChannelName);
				OnMessageSent.Broadcast(true, ChannelName);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem: EOS SendRoomChat failed for channel '%s'"), *ChannelName);
				OnMessageSent.Broadcast(false, ChannelName);
			}
			return;
		}
	}

	// No online chat: DO NOT fake success — report failure
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::SendMessage — IOnlineChat not available, message NOT delivered to '%s'"), *ChannelName);
	OnMessageSent.Broadcast(false, ChannelName);
}

void UEEOSChatSubsystem::SendDirectMessage(const FString& TargetUserId, const FString& Message)
{
	if (bUsingOnlineChat)
	{
		IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
		IOnlineChatPtr ChatInterface = EOSSub->GetChatInterface();
		FUniqueNetIdPtr LocalUserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
		if (ChatInterface.IsValid() && LocalUserId.IsValid())
		{
			FUniqueNetIdPtr TargetId = EOSSub->GetIdentityInterface()->CreateUniquePlayerId(TargetUserId);
			if (TargetId.IsValid())
			{
				bool bSent = ChatInterface->SendPrivateChat(*LocalUserId, *TargetId, Message);
				FString DMChannelName = FString::Printf(TEXT("DM_%s"), *TargetUserId);

				if (bSent)
				{
					FEEOSChatMessage ChatMsg;
					ChatMsg.SenderId = LocalUserId->ToString();
					ChatMsg.SenderDisplayName = EOSSub->GetIdentityInterface()->GetPlayerNickname(0);
					ChatMsg.Message = Message;
					ChatMsg.ChannelName = DMChannelName;
					ChatMsg.Timestamp = FDateTime::UtcNow();

					TArray<FEEOSChatMessage>& History = ChannelHistory.FindOrAdd(DMChannelName);
					History.Add(ChatMsg);

					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: Sent DM to '%s' via EOS"), *TargetUserId);
					OnMessageSent.Broadcast(true, DMChannelName);
				}
				else
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem: EOS SendPrivateChat failed for '%s'"), *TargetUserId);
					OnMessageSent.Broadcast(false, DMChannelName);
				}
				return;
			}
		}
	}

	// No online chat: DO NOT fake success
	FString DMChannelName = FString::Printf(TEXT("DM_%s"), *TargetUserId);
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::SendDirectMessage — IOnlineChat not available, message NOT delivered to '%s'"), *TargetUserId);
	OnMessageSent.Broadcast(false, DMChannelName);
}

// ── Queries ──────────────────────────────────────────────────────────────────

TArray<FEEOSChatChannel> UEEOSChatSubsystem::GetJoinedChannels() const
{
	TArray<FEEOSChatChannel> Result;
	JoinedChannels.GenerateValueArray(Result);
	return Result;
}

bool UEEOSChatSubsystem::IsInChannel(const FString& ChannelName) const
{
	return JoinedChannels.Contains(ChannelName);
}

TArray<FEEOSChatMessage> UEEOSChatSubsystem::GetChannelHistory(const FString& ChannelName, int32 MaxMessages) const
{
	const TArray<FEEOSChatMessage>* History = ChannelHistory.Find(ChannelName);
	if (!History) return TArray<FEEOSChatMessage>();

	if (History->Num() <= MaxMessages) return *History;

	TArray<FEEOSChatMessage> Result;
	int32 Start = FMath::Max(0, History->Num() - MaxMessages);
	for (int32 i = Start; i < History->Num(); i++)
	{
		Result.Add((*History)[i]);
	}
	return Result;
}

int32 UEEOSChatSubsystem::GetUnreadMessageCount() const
{
	return UnreadCount;
}
