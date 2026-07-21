// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSChatSubsystem.h"
#include "UnrealExtendedEOS.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineChatInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Shared/EEOSBlueprintLibrary.h"

/**
 * DM history/unread keys are "DM_<ProductUserId>": extracting the PUID half folds composite
 * ("<EAS>|<PUID>") and bare-PUID target ids into ONE conversation instead of splitting the
 * history across two fragile keys. Ids without a PUID half fall back to the raw input so the
 * conversation is still recorded under a deterministic key.
 */
static FString MakeDMChannelKey(const FString& TargetUserId)
{
	const FString Puid = UEEOSBlueprintLibrary::ExtractProductUserId(TargetUserId);
	return FString::Printf(TEXT("DM_%s"), Puid.IsEmpty() ? *TargetUserId : *Puid);
}

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

					// Only messages from OTHER users count as unread — the room may echo the
					// local user's own sends back through this delegate (UserId = local recipient)
					if (SenderId != UserId.ToString())
					{
						UnreadCounts.FindOrAdd(ChannelName)++;
					}
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
						// A left channel can no longer be marked read — drop its unread counter
						// so GetUnreadMessageCount doesn't report unreachable messages forever
						UnreadCounts.Remove(ChannelName);
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
	UnreadCounts.Empty();
	Super::Deinitialize();
}

// ── Channel Management ───────────────────────────────────────────────────────

bool UEEOSChatSubsystem::JoinChannel(const FString& ChannelName)
{
	if (JoinedChannels.Contains(ChannelName))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::JoinChannel — Already joined '%s'"), *ChannelName);
		OnChannelJoined.Broadcast(true, ChannelName);
		return true;
	}

	if (bUsingOnlineChat)
	{
		IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
		IOnlineChatPtr ChatInterface = EOSSub->GetChatInterface();
		FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
		if (ChatInterface.IsValid() && UserId.IsValid())
		{
			FString PlayerNickname = EOSSub->GetIdentityInterface()->GetPlayerNickname(0);
			if (!ChatInterface->JoinPublicRoom(*UserId, FChatRoomId(ChannelName), PlayerNickname, FChatRoomConfig()))
			{
				// A false return means NO room delegate will ever fire — fail observably
				// instead of leaving OnChannelJoined waiters hanging
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::JoinChannel — JoinPublicRoom was refused for '%s'"), *ChannelName);
				OnChannelJoined.Broadcast(false, ChannelName);
				return false;
			}

			// The delegate callback registered in Initialize() will handle success/failure
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: Joining channel '%s' via EOS..."), *ChannelName);
			return true;
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
	return true;
}

bool UEEOSChatSubsystem::LeaveChannel(const FString& ChannelName)
{
	if (!JoinedChannels.Contains(ChannelName))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::LeaveChannel — Not in channel '%s'"), *ChannelName);
		return false;
	}

	if (bUsingOnlineChat)
	{
		IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
		IOnlineChatPtr ChatInterface = EOSSub->GetChatInterface();
		FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
		if (ChatInterface.IsValid() && UserId.IsValid())
		{
			if (!ChatInterface->ExitRoom(*UserId, FChatRoomId(ChannelName)))
			{
				// A false return means NO exit delegate will ever fire — drop the channel
				// (and its now-unreachable unread counter) locally and broadcast so
				// OnChannelLeft waiters don't hang
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::LeaveChannel — ExitRoom was refused for '%s', removing channel locally"), *ChannelName);
				JoinedChannels.Remove(ChannelName);
				UnreadCounts.Remove(ChannelName);
				OnChannelLeft.Broadcast(ChannelName);
				return false;
			}

			// Delegate callback handles JoinedChannels/UnreadCounts removal and OnChannelLeft
			return true;
		}
	}

	// Fallback: local-only
	JoinedChannels.Remove(ChannelName);
	UnreadCounts.Remove(ChannelName);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: Left channel '%s'"), *ChannelName);
	OnChannelLeft.Broadcast(ChannelName);
	return true;
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

bool UEEOSChatSubsystem::SendMessage(const FString& ChannelName, const FString& Message)
{
	if (!JoinedChannels.Contains(ChannelName))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::SendMessage — Not in channel '%s'"), *ChannelName);
		OnMessageSent.Broadcast(false, ChannelName);
		return false;
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
			return bSent;
		}
	}

	// No online chat: DO NOT fake success — report failure
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::SendMessage — IOnlineChat not available, message NOT delivered to '%s'"), *ChannelName);
	OnMessageSent.Broadcast(false, ChannelName);
	return false;
}

bool UEEOSChatSubsystem::SendDirectMessage(const FString& TargetUserId, const FString& Message)
{
	// One conversation per target regardless of the id form the caller used — see MakeDMChannelKey
	const FString DMChannelName = MakeDMChannelKey(TargetUserId);

	if (bUsingOnlineChat)
	{
		IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
		IOnlineChatPtr ChatInterface = EOSSub->GetChatInterface();
		FUniqueNetIdPtr LocalUserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
		if (ChatInterface.IsValid() && LocalUserId.IsValid())
		{
			// CreateUniquePlayerId on the EOS OSS returns the non-null registry EmptyId on
			// parse failure — Ptr.IsValid() alone is not a validity check, ask the id itself too
			FUniqueNetIdPtr TargetId = EOSSub->GetIdentityInterface()->CreateUniquePlayerId(TargetUserId);
			if (!TargetId.IsValid() || !TargetId->IsValid())
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::SendDirectMessage — Could not parse target id '%s'"), *TargetUserId);
				OnMessageSent.Broadcast(false, DMChannelName);
				return false;
			}

			const bool bSent = ChatInterface->SendPrivateChat(*LocalUserId, *TargetId, Message);
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

				// Same retention cap as channel history — an unbounded DM thread would
				// grow for the whole session
				if (History.Num() > 200)
				{
					History.RemoveAt(0, History.Num() - 200);
				}

				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: Sent DM to '%s' via EOS"), *TargetUserId);
				OnMessageSent.Broadcast(true, DMChannelName);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem: EOS SendPrivateChat failed for '%s'"), *TargetUserId);
				OnMessageSent.Broadcast(false, DMChannelName);
			}
			return bSent;
		}
	}

	// No online chat: DO NOT fake success
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::SendDirectMessage — IOnlineChat not available, message NOT delivered to '%s'"), *TargetUserId);
	OnMessageSent.Broadcast(false, DMChannelName);
	return false;
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
	int32 Total = 0;
	for (const TPair<FString, int32>& Pair : UnreadCounts)
	{
		Total += Pair.Value;
	}
	return Total;
}

void UEEOSChatSubsystem::MarkChannelRead(const FString& ChannelName)
{
	UnreadCounts.Remove(ChannelName);
}

void UEEOSChatSubsystem::MarkAllRead()
{
	UnreadCounts.Empty();
}
