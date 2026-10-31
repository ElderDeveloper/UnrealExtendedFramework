// Copyright Moon Punch Games. All Rights Reserved.

#include "Messages/EFMessageSubsystem.h"

#include "ExtendedFoundationModule.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"
#include "Subsystems/EFSubsystems.h"

namespace EFMessagesPrivate
{
	/** A listener that sends a message that reaches itself again would otherwise never stop. */
	constexpr int32 MaxBroadcastDepth = 16;
	constexpr int32 MaxRecentMessages = 64;

	int32 LogMessages = 0;
	FAutoConsoleVariableRef CVarLogMessages(
		TEXT("Extended.Messages.Log"),
		LogMessages,
		TEXT("1 logs every message sent through the Extended message bus (channel, payload type, listener count)."));
}

void FEFMessageListenerHandle::Unregister()
{
	if (UEFMessageSubsystem* Owner = Subsystem.Get())
	{
		Owner->UnregisterListener(*this);
	}
	Subsystem.Reset();
	Channel = FGameplayTag();
	Id = 0;
}

UEFMessageSubsystem* UEFMessageSubsystem::Get(const UObject* WorldContextObject)
{
	return EF::GetGameInstanceSubsystem<UEFMessageSubsystem>(WorldContextObject);
}

void UEFMessageSubsystem::Deinitialize()
{
	ListenersByChannel.Reset();
	RecentMessages.Reset();
	OnAnyMessage.Clear();
	Super::Deinitialize();
}

bool UEFMessageSubsystem::Hears(const FListenerEntry& Entry, bool bExactLevel, const UScriptStruct* PayloadStruct)
{
	if (!bExactLevel && Entry.Match == EEFMessageMatch::ExactMatch)
	{
		return false;
	}
	if (Entry.bHasOwner && !Entry.Owner.IsValid())
	{
		return false;
	}
	if (Entry.bHasPayloadType)
	{
		const UScriptStruct* Wanted = Entry.PayloadType.Get();
		if (!Wanted || !PayloadStruct || !PayloadStruct->IsChildOf(Wanted))
		{
			return false;
		}
	}
	return true;
}

void UEFMessageSubsystem::BroadcastMessage(FGameplayTag Channel, const FInstancedStruct& Payload)
{
	if (!Channel.IsValid())
	{
		UE_LOG(LogExtendedFoundation, Warning, TEXT("BroadcastMessage: the channel is empty; the message was not sent."));
		return;
	}
	if (BroadcastDepth >= EFMessagesPrivate::MaxBroadcastDepth)
	{
		UE_LOG(LogExtendedFoundation, Error, TEXT("BroadcastMessage: %s was sent from listeners nested %d deep; it was dropped to stop a loop."),
			*Channel.ToString(), BroadcastDepth);
		return;
	}

	++BroadcastDepth;
	ON_SCOPE_EXIT { --BroadcastDepth; };

	const UScriptStruct* PayloadStruct = Payload.GetScriptStruct();

	// Collect first, call after: listeners may add or remove listeners while they run.
	struct FMatch
	{
		FGameplayTag ListenerChannel;
		int64 Id;
	};
	TArray<FMatch, TInlineAllocator<8>> Matches;
	bool bFoundDeadOwner = false;

	FGameplayTag Level = Channel;
	bool bExactLevel = true;
	while (Level.IsValid())
	{
		if (const TArray<FListenerEntry>* Entries = ListenersByChannel.Find(Level))
		{
			for (const FListenerEntry& Entry : *Entries)
			{
				if (Entry.bHasOwner && !Entry.Owner.IsValid())
				{
					bFoundDeadOwner = true;
					continue;
				}
				if (Hears(Entry, bExactLevel, PayloadStruct))
				{
					Matches.Add({ Level, Entry.Id });
				}
			}
		}
		Level = Level.RequestDirectParent();
		bExactLevel = false;
	}

	int32 Delivered = 0;
	for (const FMatch& Match : Matches)
	{
		TArray<FListenerEntry>* Entries = ListenersByChannel.Find(Match.ListenerChannel);
		FListenerEntry* Entry = Entries ? Entries->FindByPredicate([&Match](const FListenerEntry& Candidate) { return Candidate.Id == Match.Id; }) : nullptr;
		if (!Entry || !Entry->Callback)
		{
			continue;  // removed by an earlier listener of this message
		}
		if (Entry->bHasOwner && !Entry->Owner.IsValid())
		{
			bFoundDeadOwner = true;
			continue;  // its owner was destroyed by an earlier listener of this message
		}

		// Copy: the callback may remove its own listener, which would free the entry it lives in.
		const FListener Callback = Entry->Callback;
		Callback(Channel, Payload);
		++Delivered;
	}

	RecordMessage(Channel, PayloadStruct, Delivered);
	OnAnyMessage.Broadcast(Channel, Payload);

	if (bFoundDeadOwner)
	{
		PruneDeadListeners();
	}
}

FEFMessageListenerHandle UEFMessageSubsystem::RegisterListener(FGameplayTag Channel, FListener Callback, EEFMessageMatch Match,
	const UScriptStruct* PayloadType, const UObject* Owner)
{
	FEFMessageListenerHandle Handle;
	if (!Channel.IsValid() || !Callback)
	{
		UE_LOG(LogExtendedFoundation, Warning, TEXT("RegisterListener: %s; no listener was added."),
			Channel.IsValid() ? TEXT("the callback is empty") : TEXT("the channel is empty"));
		return Handle;
	}

	FListenerEntry& Entry = ListenersByChannel.FindOrAdd(Channel).AddDefaulted_GetRef();
	Entry.Id = ++LastListenerId;
	Entry.Match = Match;
	Entry.PayloadType = PayloadType;
	Entry.bHasPayloadType = PayloadType != nullptr;
	Entry.Owner = Owner;
	Entry.bHasOwner = Owner != nullptr;
	Entry.Callback = MoveTemp(Callback);

	Handle.Subsystem = this;
	Handle.Channel = Channel;
	Handle.Id = Entry.Id;
	return Handle;
}

void UEFMessageSubsystem::UnregisterListener(FEFMessageListenerHandle& Handle)
{
	if (Handle.IsValid() && Handle.Subsystem.Get() == this)
	{
		RemoveListener(Handle.Channel, Handle.Id);
	}
	Handle.Subsystem.Reset();
	Handle.Channel = FGameplayTag();
	Handle.Id = 0;
}

void UEFMessageSubsystem::RemoveListener(FGameplayTag Channel, int64 Id)
{
	if (TArray<FListenerEntry>* Entries = ListenersByChannel.Find(Channel))
	{
		Entries->RemoveAll([Id](const FListenerEntry& Entry) { return Entry.Id == Id; });
		if (Entries->Num() == 0)
		{
			ListenersByChannel.Remove(Channel);
		}
	}
}

void UEFMessageSubsystem::PruneDeadListeners()
{
	for (auto It = ListenersByChannel.CreateIterator(); It; ++It)
	{
		It.Value().RemoveAll([](const FListenerEntry& Entry) { return Entry.bHasOwner && !Entry.Owner.IsValid(); });
		if (It.Value().Num() == 0)
		{
			It.RemoveCurrent();
		}
	}
}

int32 UEFMessageSubsystem::CountListeners(FGameplayTag Channel) const
{
	int32 Count = 0;
	FGameplayTag Level = Channel;
	bool bExactLevel = true;
	while (Level.IsValid())
	{
		if (const TArray<FListenerEntry>* Entries = ListenersByChannel.Find(Level))
		{
			for (const FListenerEntry& Entry : *Entries)
			{
				// Payload filters are not known without a payload, so count every listener on the channel.
				if ((bExactLevel || Entry.Match == EEFMessageMatch::PartialMatch) && (!Entry.bHasOwner || Entry.Owner.IsValid()))
				{
					++Count;
				}
			}
		}
		Level = Level.RequestDirectParent();
		bExactLevel = false;
	}
	return Count;
}

void UEFMessageSubsystem::RecordMessage(FGameplayTag Channel, const UScriptStruct* PayloadStruct, int32 ListenerCount)
{
	if (RecentMessages.Num() >= EFMessagesPrivate::MaxRecentMessages)
	{
		RecentMessages.RemoveAt(0, RecentMessages.Num() - EFMessagesPrivate::MaxRecentMessages + 1);
	}

	FEFMessageRecord& Record = RecentMessages.AddDefaulted_GetRef();
	Record.Channel = Channel;
	Record.PayloadType = PayloadStruct ? PayloadStruct->GetFName() : NAME_None;
	Record.ListenerCount = ListenerCount;
	Record.TimeSeconds = FPlatformTime::Seconds();

	if (EFMessagesPrivate::LogMessages != 0)
	{
		UE_LOG(LogExtendedFoundation, Display, TEXT("Message %s (%s) -> %d listener(s)"),
			*Channel.ToString(), PayloadStruct ? *PayloadStruct->GetName() : TEXT("no payload"), ListenerCount);
	}
}
