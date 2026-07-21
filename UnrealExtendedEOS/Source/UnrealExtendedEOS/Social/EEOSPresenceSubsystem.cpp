// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSPresenceSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "UnrealExtendedEOS.h"

void UEEOSPresenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSPresenceSubsystem::Deinitialize()
{
	CachedRichText.Empty();
	CachedStatusText.Empty();
	CachedPresenceProperties.Empty();
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

UEEOSPresenceSubsystem::FEEOSPendingPresence UEEOSPresenceSubsystem::StagePendingFromCache() const
{
	FEEOSPendingPresence Pending;
	Pending.State = CachedPresenceState;
	Pending.StatusText = CachedStatusText;
	Pending.RichText = CachedRichText;
	Pending.Properties = CachedPresenceProperties;
	return Pending;
}

bool UEEOSPresenceSubsystem::SetPresence(const FString& StatusString, const FString& RichText)
{
	FEEOSPendingPresence Pending = StagePendingFromCache();
	Pending.StatusText = StatusString;
	Pending.RichText = RichText;
	Pending.Properties.Add(TEXT("RichPresence"), RichText);
	return SubmitPresence(MoveTemp(Pending), TEXT("SetPresence"));
}

bool UEEOSPresenceSubsystem::SetPresenceWithStatus(EEOSOnlineStatus OnlineStatus, const FString& RichText)
{
	FEEOSPendingPresence Pending = StagePendingFromCache();
	Pending.State = OnlineStatus;
	Pending.StatusText = RichText;
	Pending.RichText = RichText;
	Pending.Properties.Add(TEXT("RichPresence"), RichText);
	return SubmitPresence(MoveTemp(Pending), TEXT("SetPresenceWithStatus"));
}

bool UEEOSPresenceSubsystem::SetPresenceKey(const FString& Key, const FString& Value)
{
	FEEOSPendingPresence Pending = StagePendingFromCache();
	Pending.Properties.Add(Key, Value);
	return SubmitPresence(MoveTemp(Pending), TEXT("SetPresenceKey"));
}

bool UEEOSPresenceSubsystem::SetJoinInfo(const FString& JoinInfoString)
{
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPresenceSubsystem: Setting JoinInfo to '%s'"), *JoinInfoString);
	return SetPresenceKey(TEXT("JoinInfo"), JoinInfoString);
}

bool UEEOSPresenceSubsystem::ClearPresence()
{
	// Submit Offline with everything emptied. The staged state — not the caches — carries the
	// Offline; on success the remembered state resets to Online so later setters bring the
	// user back instead of faithfully re-submitting Offline forever.
	FEEOSPendingPresence Pending;
	Pending.State = EEOSOnlineStatus::Offline;
	Pending.bResetStateToOnlineOnSuccess = true;
	return SubmitPresence(MoveTemp(Pending), TEXT("ClearPresence"));
}

bool UEEOSPresenceSubsystem::SubmitPresence(FEEOSPendingPresence&& Pending, const FString& CallerName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(CallerName);
		OnPresenceSet.Broadcast(false);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlinePresencePtr PresenceInterface = EOSSub->GetPresenceInterface();
	if (!PresenceInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPresenceSubsystem::%s — Presence interface not available"), *CallerName);
		OnPresenceSet.Broadcast(false);
		return false;
	}

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSPresenceSubsystem::%s — User not logged in"), *CallerName);
		OnPresenceSet.Broadcast(false);
		return false;
	}

	// EOS presence requires an Epic account: the OSS returns WITHOUT executing the completion
	// delegate when the net id has no valid Epic Account handle (UserManagerEOS.cpp SetPresence),
	// which would silently drop our exactly-once broadcast. Detect Connect-only logins up front.
	if (UEEOSBlueprintLibrary::ExtractEpicAccountId(UserId->ToString()).IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSPresenceSubsystem::%s — Local user has no Epic account (Connect-only login); EOS presence is unavailable"), *CallerName);
		OnPresenceSet.Broadcast(false);
		return false;
	}

	// KNOWN RESIDUAL: the engine's SetPresence also returns without executing the delegate
	// when EOS_Presence_CreatePresenceModification hands back no modification handle
	// (UserManagerEOS.cpp:3373-3377). That path is undetectable from outside the OSS (the
	// engine call is void) and only occurs when the presence handle itself is broken; the
	// interface/login/Epic-account pre-checks above cover every reproducible cause, and the
	// residual is accepted rather than papered over with a timeout.

	// Submit the FULL staged status: the engine builds every SetPresence modification from
	// scratch (state + raw rich text from StatusStr + data records from Properties), so any
	// field left out of one call would clobber what a previous call set.
	FOnlineUserPresenceStatus Status;
	Status.StatusStr = Pending.StatusText;
	Status.State = ConvertStatus(Pending.State);
	for (const TPair<FString, FString>& Property : Pending.Properties)
	{
		Status.Properties.Add(Property.Key, Property.Value);
	}

	PresenceInterface->SetPresence(*UserId, Status,
		IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateWeakLambda(this,
			[this, CallerName, Pending = MoveTemp(Pending)](const FUniqueNetId& InUserId, const bool bWasSuccessful)
			{
				if (bWasSuccessful)
				{
					// Commit the staged values ONLY now that the backend confirmed them — a
					// failed submit leaves the caches (and the getters) on the last real state
					CachedStatusText = Pending.StatusText;
					CachedRichText = Pending.RichText;
					CachedPresenceProperties = Pending.Properties;
					CachedPresenceState = Pending.bResetStateToOnlineOnSuccess ? EEOSOnlineStatus::Online : Pending.State;

					CachedLocalPresence.Status = Pending.StatusText;
					CachedLocalPresence.RichText = Pending.RichText;
					CachedLocalPresence.bIsOnline = (Pending.State != EEOSOnlineStatus::Offline);

					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPresenceSubsystem: %s — presence set (state=%d, status='%s', %d keys)"),
						*CallerName, static_cast<int32>(Pending.State), *Pending.StatusText, Pending.Properties.Num());
				}
				else
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSPresenceSubsystem: %s — failed to set presence (local caches left unchanged)"), *CallerName);
				}
				OnPresenceSet.Broadcast(bWasSuccessful);
			}));
	return true;
}

bool UEEOSPresenceSubsystem::QueryPresence(const FString& UserId)
{
	// Every failure path broadcasts OnPresenceQueryFailed — flows waiting on this query can
	// terminate instead of hanging on a log-only dead end
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryPresence"));
		OnPresenceQueryFailed.Broadcast(UserId);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlinePresencePtr PresenceInterface = EOSSub->GetPresenceInterface();
	if (!PresenceInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPresenceSubsystem::QueryPresence — Presence interface not available"));
		OnPresenceQueryFailed.Broadcast(UserId);
		return false;
	}

	// Must be constructed by the identity interface: the EOS OSS downcasts incoming ids to
	// FUniqueNetIdEOS, so a generic FUniqueNetIdString here is undefined behavior.
	// CreateUniquePlayerId on the EOS OSS returns the non-null registry EmptyId on parse
	// failure — Ptr.IsValid() alone is not a validity check, ask the id itself too.
	const FUniqueNetIdPtr NetId = EOSSub->GetIdentityInterface()->CreateUniquePlayerId(UserId);
	if (!NetId.IsValid() || !NetId->IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSPresenceSubsystem::QueryPresence — Could not parse user id '%s'"), *UserId);
		OnPresenceQueryFailed.Broadcast(UserId);
		return false;
	}

	PresenceInterface->QueryPresence(*NetId,
		IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateWeakLambda(this,
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
					OnPresenceQueryFailed.Broadcast(InUserId.ToString());
				}
			}));
	return true;
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
