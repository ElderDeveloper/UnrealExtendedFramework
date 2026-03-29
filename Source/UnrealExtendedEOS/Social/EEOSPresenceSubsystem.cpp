// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSPresenceSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "UnrealExtendedEOS.h"

void UEEOSPresenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSPresenceSubsystem::Deinitialize()
{
	CachedRichText.Empty();
	Super::Deinitialize();
}

static EOnlinePresenceState::Type ConvertStatus(EEOSOnlineStatus Status)
{
	switch (Status)
	{
	case EEOSOnlineStatus::Online:			return EOnlinePresenceState::Online;
	case EEOSOnlineStatus::Away:			return EOnlinePresenceState::Away;
	case EEOSOnlineStatus::DoNotDisturb:	return EOnlinePresenceState::DoNotDisturb;
	case EEOSOnlineStatus::ExtendedAway:	return EOnlinePresenceState::ExtendedAway;
	case EEOSOnlineStatus::Offline:			return EOnlinePresenceState::Offline;
	default:								return EOnlinePresenceState::Online;
	}
}

void UEEOSPresenceSubsystem::SetPresence(const FString& StatusString, const FString& RichText)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("SetPresence"));
		OnPresenceSet.Broadcast(false);
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlinePresencePtr PresenceInterface = EOSSub->GetPresenceInterface();
	if (!PresenceInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPresenceSubsystem::SetPresence — Presence interface not available"));
		OnPresenceSet.Broadcast(false);
		return;
	}

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSPresenceSubsystem::SetPresence — User not logged in"));
		OnPresenceSet.Broadcast(false);
		return;
	}

	CachedRichText = RichText;

	FOnlineUserPresenceStatus Status;
	Status.StatusStr = StatusString;
	Status.Properties.Add(TEXT("RichPresence"), RichText);
	Status.State = EOnlinePresenceState::Online;

	PresenceInterface->SetPresence(*UserId, Status,
		IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateLambda(
			[this, StatusString, RichText](const FUniqueNetId& InUserId, const bool bWasSuccessful)
			{
				if (bWasSuccessful)
				{
					CachedLocalPresence.Status = StatusString;
					CachedLocalPresence.bIsOnline = true;
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPresenceSubsystem: Presence set — '%s'"), *RichText);
				}
				else
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSPresenceSubsystem: Failed to set presence"));
				}
				OnPresenceSet.Broadcast(bWasSuccessful);
			}));
}

void UEEOSPresenceSubsystem::SetPresenceWithStatus(EEOSOnlineStatus OnlineStatus, const FString& RichText)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("SetPresenceWithStatus"));
		OnPresenceSet.Broadcast(false);
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlinePresencePtr PresenceInterface = EOSSub->GetPresenceInterface();
	if (!PresenceInterface.IsValid())
	{
		OnPresenceSet.Broadcast(false);
		return;
	}

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		OnPresenceSet.Broadcast(false);
		return;
	}

	CachedRichText = RichText;

	FOnlineUserPresenceStatus Status;
	Status.StatusStr = RichText;
	Status.State = ConvertStatus(OnlineStatus);
	Status.Properties.Add(TEXT("RichPresence"), RichText);

	PresenceInterface->SetPresence(*UserId, Status,
		IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateLambda(
			[this, OnlineStatus, RichText](const FUniqueNetId& InUserId, const bool bWasSuccessful)
			{
				if (bWasSuccessful)
				{
					CachedLocalPresence.bIsOnline = (OnlineStatus != EEOSOnlineStatus::Offline);
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPresenceSubsystem: Presence set with status %d — '%s'"), static_cast<int32>(OnlineStatus), *RichText);
				}
				OnPresenceSet.Broadcast(bWasSuccessful);
			}));
}

void UEEOSPresenceSubsystem::SetPresenceKey(const FString& Key, const FString& Value)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("SetPresenceKey"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlinePresencePtr PresenceInterface = EOSSub->GetPresenceInterface();
	if (!PresenceInterface.IsValid()) return;

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid()) return;

	FOnlineUserPresenceStatus Status;
	Status.State = EOnlinePresenceState::Online;
	Status.Properties.Add(Key, Value);

	PresenceInterface->SetPresence(*UserId, Status,
		IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateLambda(
			[Key, Value](const FUniqueNetId& InUserId, const bool bWasSuccessful)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPresenceSubsystem: SetPresenceKey '%s'='%s' — %s"), *Key, *Value, bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
			}));
}

void UEEOSPresenceSubsystem::SetJoinInfo(const FString& JoinInfoString)
{
	SetPresenceKey(TEXT("JoinInfo"), JoinInfoString);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPresenceSubsystem: JoinInfo set to '%s'"), *JoinInfoString);
}

void UEEOSPresenceSubsystem::ClearPresence()
{
	SetPresenceWithStatus(EEOSOnlineStatus::Offline, TEXT(""));
	CachedLocalPresence = FEEOSPresenceInfo();
	CachedRichText.Empty();
}

void UEEOSPresenceSubsystem::QueryPresence(const FString& UserId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryPresence"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlinePresencePtr PresenceInterface = EOSSub->GetPresenceInterface();
	if (!PresenceInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPresenceSubsystem::QueryPresence — Presence interface not available"));
		return;
	}

	const FUniqueNetIdRef NetId = FUniqueNetIdString::Create(UserId, EOS_SUBSYSTEM);

	PresenceInterface->QueryPresence(*NetId,
		IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateLambda(
			[this, PresenceInterface](const FUniqueNetId& InUserId, const bool bWasSuccessful)
			{
				if (bWasSuccessful)
				{
					FEEOSPresenceInfo Info;
					Info.UserId = InUserId.ToString();

					TSharedPtr<FOnlineUserPresence> PresencePtr;
					if (PresenceInterface->GetCachedPresence(InUserId, PresencePtr) == EOnlineCachedResult::Success && PresencePtr.IsValid())
					{
						Info.bIsOnline = PresencePtr->bIsOnline;
						Info.bIsPlaying = PresencePtr->bIsPlayingThisGame;
						Info.Status = PresencePtr->Status.StatusStr;
					}
					else
					{
						Info.bIsOnline = true;
					}

					OnPresenceUpdated.Broadcast(Info);
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPresenceSubsystem: Presence queried for %s — Online=%d"), *InUserId.ToString(), Info.bIsOnline);
				}
				else
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSPresenceSubsystem: Failed to query presence for %s"), *InUserId.ToString());
				}
			}));
}

FEEOSPresenceInfo UEEOSPresenceSubsystem::GetLocalPresence() const
{
	return CachedLocalPresence;
}

bool UEEOSPresenceSubsystem::IsOnline() const
{
	return CachedLocalPresence.bIsOnline;
}

FString UEEOSPresenceSubsystem::GetRichPresenceText() const
{
	return CachedRichText;
}
