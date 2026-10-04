// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSChatSubsystem.h"
#include "Shared/EEOSNativeOperation.h"
#include "Shared/EEOSIdentityUtils.h"
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
	Super::Initialize(Collection); bShuttingDown = false;
	BindNativeChat();
	ChatBindingTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UEEOSChatSubsystem::TickNativeChat), 0.5f);
}
void UEEOSChatSubsystem::BindNativeChat()
{
	ChatBindingContext = CaptureEOSContext();
	// Register for incoming chat messages if the interface is available
	if (IOnlineSubsystem* EOSSub = GetExistingEOSOnlineSubsystem())
	{
		IOnlineChatPtr ChatInterface = EOSSub ? EOSSub->GetChatInterface() : IOnlineChatPtr();
		if (ChatInterface.IsValid())
		{
			BoundChat = ChatInterface;
			ChatMessageReceivedHandle = ChatInterface->AddOnChatRoomMessageReceivedDelegate_Handle(
				FOnChatRoomMessageReceivedDelegate::CreateWeakLambda(this,
				[this, Ownership = CaptureEOSContext()](const FUniqueNetId& UserId, const FChatRoomId& RoomId, const TSharedRef<FChatMessage>& ChatMessage)
				{
					if (bShuttingDown || !IsEOSContextCurrent(Ownership) || Ownership.LocalId != UserId.ToString()) return;
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
				FOnChatRoomJoinPublicDelegate::CreateWeakLambda(this,
				[this, Ownership = CaptureEOSContext()](const FUniqueNetId& UserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error)
				{
					if (bShuttingDown || !IsEOSContextCurrent(Ownership) || Ownership.LocalId != UserId.ToString()) return;
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

						UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: Joined channel '%s' via EOS"), *FEEOSNativeOperationLease::SafeField(ChannelName));
					}
					else
					{
						UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem: Failed to join channel '%s' — %s"), *FEEOSNativeOperationLease::SafeField(ChannelName), *FEEOSNativeOperationLease::SafeField(Error));
					}

					OnChannelJoined.Broadcast(bWasSuccessful, ChannelName);
				}));

			ChatRoomExitHandle = ChatInterface->AddOnChatRoomExitDelegate_Handle(
				FOnChatRoomExitDelegate::CreateWeakLambda(this,
				[this, Ownership = CaptureEOSContext()](const FUniqueNetId& UserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error)
				{
					if (bShuttingDown || !IsEOSContextCurrent(Ownership) || Ownership.LocalId != UserId.ToString()) return;
					if (bWasSuccessful)
					{
						FString ChannelName = RoomId;
						JoinedChannels.Remove(ChannelName);
						// A left channel can no longer be marked read — drop its unread counter
						// so GetUnreadMessageCount doesn't report unreachable messages forever
						UnreadCounts.Remove(ChannelName);
						UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: Left channel '%s' via EOS"), *FEEOSNativeOperationLease::SafeField(ChannelName));
						OnChannelLeft.Broadcast(ChannelName);
					}
				}));

			ChatMemberJoinHandle = ChatInterface->AddOnChatRoomMemberJoinDelegate_Handle(
				FOnChatRoomMemberJoinDelegate::CreateWeakLambda(this,
				[this, Ownership = CaptureEOSContext()](const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FUniqueNetId& MemberId)
				{
					if (bShuttingDown || !IsEOSContextCurrent(Ownership) || Ownership.LocalId != UserId.ToString()) return;
					FString ChannelName = RoomId;
					FString UserIdStr = MemberId.ToString();
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: User '%s' joined channel '%s'"), *FEEOSNativeOperationLease::SafeField(UserIdStr), *FEEOSNativeOperationLease::SafeField(ChannelName));
					OnUserJoined.Broadcast(UserIdStr, ChannelName);
				}));

			ChatMemberExitHandle = ChatInterface->AddOnChatRoomMemberExitDelegate_Handle(
				FOnChatRoomMemberExitDelegate::CreateWeakLambda(this,
				[this, Ownership = CaptureEOSContext()](const FUniqueNetId& UserId, const FChatRoomId& RoomId, const FUniqueNetId& MemberId)
				{
					if (bShuttingDown || !IsEOSContextCurrent(Ownership) || Ownership.LocalId != UserId.ToString()) return;
					FString ChannelName = RoomId;
					FString UserIdStr = MemberId.ToString();
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: User '%s' left channel '%s'"), *FEEOSNativeOperationLease::SafeField(UserIdStr), *FEEOSNativeOperationLease::SafeField(ChannelName));
					OnUserLeft.Broadcast(UserIdStr, ChannelName);
				}));

			bUsingOnlineChat = true;
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem initialized with IOnlineChat interface"));
		}
		else
		{
			bUsingOnlineChat = false;
			FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("Chat"), TEXT("UnsupportedChatCapability"));
		}
	}
}
void UEEOSChatSubsystem::UnbindNativeChat()
{
	if (BoundChat.IsValid())
	{
		BoundChat->ClearOnChatRoomMessageReceivedDelegate_Handle(ChatMessageReceivedHandle);
		BoundChat->ClearOnChatRoomJoinPublicDelegate_Handle(ChatRoomJoinHandle);
		BoundChat->ClearOnChatRoomExitDelegate_Handle(ChatRoomExitHandle);
		BoundChat->ClearOnChatRoomMemberJoinDelegate_Handle(ChatMemberJoinHandle);
		BoundChat->ClearOnChatRoomMemberExitDelegate_Handle(ChatMemberExitHandle);
	}
	ChatMessageReceivedHandle.Reset(); ChatRoomJoinHandle.Reset(); ChatRoomExitHandle.Reset(); ChatMemberJoinHandle.Reset(); ChatMemberExitHandle.Reset();
	BoundChat.Reset(); bUsingOnlineChat = false;
}
bool UEEOSChatSubsystem::TickNativeChat(float)
{
	if (bShuttingDown) return false;
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	const auto Current = OSS ? OSS->GetChatInterface() : IOnlineChatPtr();
	if (Current != BoundChat || !IsEOSContextCurrent(ChatBindingContext))
	{
		UnbindNativeChat(); JoinedChannels.Empty(); ChannelHistory.Empty(); UnreadCounts.Empty();
		BindNativeChat();
	}
	return true;
}


void UEEOSChatSubsystem::Deinitialize()
{
	// Only leave channels with the exact local identity that joined them.
	const bool bOriginalMember = IsEOSContextCurrent(ChatBindingContext);
	const auto OriginalChat = BoundChat;
	const auto Identity = ChatBindingContext.Identity;
	const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	BeginEOSShutdown(); bShuttingDown = true;
	if (ChatBindingTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(ChatBindingTicker);
	ChatBindingTicker.Reset(); UnbindNativeChat();
	if (bOriginalMember && OriginalChat.IsValid() && Local.IsValid() && Local->IsValid())
		for (const auto& Channel : JoinedChannels) OriginalChat->ExitRoom(*Local, FChatRoomId(Channel.Key));
	JoinedChannels.Empty(); ChannelHistory.Empty(); UnreadCounts.Empty();
	Super::Deinitialize();
}

// ── Channel Management ───────────────────────────────────────────────────────

bool UEEOSChatSubsystem::JoinChannel(const FString& ChannelName)
{
	if (bShuttingDown) return false;
	TickNativeChat(0);
	if (JoinedChannels.Contains(ChannelName))
	{
		UE_LOG(LogExtendedEOS, Verbose, TEXT("EEOSChatSubsystem::JoinChannel — Already joined '%s'"), *FEEOSNativeOperationLease::SafeField(ChannelName));
		OnChannelJoined.Broadcast(true, ChannelName);
		return true;
	}

	if (bUsingOnlineChat)
	{
		IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
		IOnlineChatPtr ChatInterface = EOSSub ? EOSSub->GetChatInterface() : IOnlineChatPtr();
		const auto Identity = EOSSub ? EOSSub->GetIdentityInterface() : IOnlineIdentityPtr();
		FUniqueNetIdPtr UserId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
		if (ChatInterface.IsValid() && UserId.IsValid() && UserId->IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
		{
			FString PlayerNickname = EEOSIdentity::SafeLocalNickname(EOSSub->GetIdentityInterface());
			if (!ChatInterface->JoinPublicRoom(*UserId, FChatRoomId(ChannelName), PlayerNickname, FChatRoomConfig()))
			{
				// A false return means NO room delegate will ever fire — fail observably
				// instead of leaving OnChannelJoined waiters hanging
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::JoinChannel — JoinPublicRoom was refused for '%s'"), *FEEOSNativeOperationLease::SafeField(ChannelName));
				OnChannelJoined.Broadcast(false, ChannelName);
				return false;
			}

			// The delegate callback registered in Initialize() will handle success/failure
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: Joining channel '%s' via EOS..."), *FEEOSNativeOperationLease::SafeField(ChannelName));
			return true;
		}
	}

	// Fallback: no online chat interface
	FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("JoinChannel"), TEXT("UnsupportedChatCapability"));
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
	if (bShuttingDown) return false;
	TickNativeChat(0);
	if (!JoinedChannels.Contains(ChannelName))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::LeaveChannel — Not in channel '%s'"), *FEEOSNativeOperationLease::SafeField(ChannelName));
		return false;
	}

	if (bUsingOnlineChat)
	{
		IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
		IOnlineChatPtr ChatInterface = EOSSub ? EOSSub->GetChatInterface() : IOnlineChatPtr();
		const auto Identity = EOSSub ? EOSSub->GetIdentityInterface() : IOnlineIdentityPtr();
		FUniqueNetIdPtr UserId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
		if (ChatInterface.IsValid() && UserId.IsValid() && UserId->IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
		{
			if (!ChatInterface->ExitRoom(*UserId, FChatRoomId(ChannelName)))
			{
				// A false return means NO exit delegate will ever fire — drop the channel
				// (and its now-unreachable unread counter) locally and broadcast so
				// OnChannelLeft waiters don't hang
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::LeaveChannel — ExitRoom was refused for '%s', removing channel locally"), *FEEOSNativeOperationLease::SafeField(ChannelName));
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
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: Left channel '%s'"), *FEEOSNativeOperationLease::SafeField(ChannelName));
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
	if (bShuttingDown) return false;
	TickNativeChat(0);
	if (!JoinedChannels.Contains(ChannelName))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::SendMessage — Not in channel '%s'"), *FEEOSNativeOperationLease::SafeField(ChannelName));
		OnMessageSent.Broadcast(false, ChannelName);
		return false;
	}

	if (bUsingOnlineChat)
	{
		IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
		IOnlineChatPtr ChatInterface = EOSSub ? EOSSub->GetChatInterface() : IOnlineChatPtr();
		const auto Identity = EOSSub ? EOSSub->GetIdentityInterface() : IOnlineIdentityPtr();
		FUniqueNetIdPtr UserId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
		if (ChatInterface.IsValid() && UserId.IsValid() && UserId->IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
		{
			bool bSent = ChatInterface->SendRoomChat(*UserId, FChatRoomId(ChannelName), Message);
			if (bSent)
			{
				// Add to local history
				FEEOSChatMessage ChatMsg;
				ChatMsg.SenderId = UserId->ToString();
				ChatMsg.SenderDisplayName = EEOSIdentity::SafeLocalNickname(EOSSub->GetIdentityInterface());
				ChatMsg.Message = Message;
				ChatMsg.ChannelName = ChannelName;
				ChatMsg.Timestamp = FDateTime::UtcNow();

				TArray<FEEOSChatMessage>& History = ChannelHistory.FindOrAdd(ChannelName);
				History.Add(ChatMsg);

				if (History.Num() > 200)
				{
					History.RemoveAt(0, History.Num() - 200);
				}

				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: Sent message in '%s' via EOS"), *FEEOSNativeOperationLease::SafeField(ChannelName));
				OnMessageSent.Broadcast(true, ChannelName);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem: EOS SendRoomChat failed for channel '%s'"), *FEEOSNativeOperationLease::SafeField(ChannelName));
				OnMessageSent.Broadcast(false, ChannelName);
			}
			return bSent;
		}
	}

	// No online chat: DO NOT fake success — report failure
	FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("SendMessage"), TEXT("UnsupportedChatCapability"));
	OnMessageSent.Broadcast(false, ChannelName);
	return false;
}

bool UEEOSChatSubsystem::SendDirectMessage(const FString& TargetUserId, const FString& Message)
{
	if (bShuttingDown) return false;
	TickNativeChat(0);
	// One conversation per target regardless of the id form the caller used — see MakeDMChannelKey
	const FString DMChannelName = MakeDMChannelKey(TargetUserId);

	if (bUsingOnlineChat)
	{
		IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
		IOnlineChatPtr ChatInterface = EOSSub ? EOSSub->GetChatInterface() : IOnlineChatPtr();
		const auto Identity = EOSSub ? EOSSub->GetIdentityInterface() : IOnlineIdentityPtr();
		FUniqueNetIdPtr LocalUserId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
		if (ChatInterface.IsValid() && LocalUserId.IsValid() && LocalUserId->IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
		{
			// CreateUniquePlayerId on the EOS OSS returns the non-null registry EmptyId on
			// parse failure — Ptr.IsValid() alone is not a validity check, ask the id itself too
			FUniqueNetIdPtr TargetId = EOSSub->GetIdentityInterface()->CreateUniquePlayerId(TargetUserId);
			if (!TargetId.IsValid() || !TargetId->IsValid())
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem::SendDirectMessage — Could not parse target id '%s'"), *FEEOSNativeOperationLease::SafeField(TargetUserId));
				OnMessageSent.Broadcast(false, DMChannelName);
				return false;
			}

			const bool bSent = ChatInterface->SendPrivateChat(*LocalUserId, *TargetId, Message);
			if (bSent)
			{
				FEEOSChatMessage ChatMsg;
				ChatMsg.SenderId = LocalUserId->ToString();
				ChatMsg.SenderDisplayName = EEOSIdentity::SafeLocalNickname(EOSSub->GetIdentityInterface());
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

				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSChatSubsystem: Sent DM to '%s' via EOS"), *FEEOSNativeOperationLease::SafeField(TargetUserId));
				OnMessageSent.Broadcast(true, DMChannelName);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSChatSubsystem: EOS SendPrivateChat failed for '%s'"), *FEEOSNativeOperationLease::SafeField(TargetUserId));
				OnMessageSent.Broadcast(false, DMChannelName);
			}
			return bSent;
		}
	}

	// No online chat: DO NOT fake success
	FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("SendDirectMessage"), TEXT("UnsupportedChatCapability"));
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

FEEOSChatCapabilitySnapshot UEEOSChatSubsystem::GetChatCapability() const
{
	FEEOSChatCapabilitySnapshot Result;
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	const auto Chat = OSS ? OSS->GetChatInterface() : IOnlineChatPtr();
	Result.bNativeInterfaceAvailable = Chat.IsValid(); Result.bLocalIdentityReady = GetEOSReadiness().bNativeLoggedIn;
	Result.bCanSubmitNativeMessage = !bShuttingDown && Result.bNativeInterfaceAvailable && Result.bLocalIdentityReady;
	Result.Code = bShuttingDown ? EEOSOperationCode::Canceled : !Result.bNativeInterfaceAvailable ? EEOSOperationCode::UnsupportedCapability : !Result.bLocalIdentityReady ? EEOSOperationCode::IdentityUnavailable : EEOSOperationCode::None;
	Result.Route = Result.bNativeInterfaceAvailable ? FName(TEXT("IOnlineChat")) : FName(TEXT("LocalHistoryOnly"));
	return Result;
}
