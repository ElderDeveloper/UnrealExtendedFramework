// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSPresenceSubsystem.h"
#include "Shared/EEOSNativeOperation.h"
#include "Shared/EEOSSettings.h"
#include "Shared/EEOSIdentityUtils.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "UnrealExtendedEOS.h"

void UEEOSPresenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PresenceTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UEEOSPresenceSubsystem::TickPresenceIdentity), 1.0f);
}

void UEEOSPresenceSubsystem::Deinitialize()
{
	BeginEOSShutdown(); bShuttingDown = true; ++PresenceGeneration;
	if (PresenceTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(PresenceTicker);
	PresenceTicker.Reset();
	if (bPresenceInFlight) CompleteOperation(TEXT("SetPresence"), false, EEOSOperationCode::Canceled,
		TEXT("Shutdown retired the presence recipient; native cleanup retains its lease."));
	const auto Retired = MoveTemp(PendingWrites); PendingWrites.Empty();
	for (const auto& Request : Retired) RetireQueuedPresence(Request, TEXT("Shutdown retired queued presence intent."));
	bPresenceInFlight = false; ActivePresenceRequest = 0; ActivePresenceInterface = nullptr; PresenceLease.Reset();
	CachedLocalPresence = FEEOSPresenceInfo(); CachedStatusText.Empty(); CachedRichText.Empty(); CachedPresenceProperties.Empty();
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
	return FEEOSPendingPresence(); // Setters carry intent; merge against confirmed state at dispatch.
}

bool UEEOSPresenceSubsystem::SetPresence(const FString& StatusString, const FString& RichText)
{
	FEEOSPendingPresence Pending = StagePendingFromCache();
	Pending.bSetText = Pending.bSetRichText = true;
	Pending.StatusText = StatusString;
	Pending.RichText = RichText;
	Pending.Properties.Add(TEXT("RichPresence"), RichText);
	return SubmitPresence(MoveTemp(Pending), TEXT("SetPresence"));
}

bool UEEOSPresenceSubsystem::SetPresenceWithStatus(EEOSOnlineStatus OnlineStatus, const FString& RichText)
{
	FEEOSPendingPresence Pending = StagePendingFromCache();
	Pending.bSetState = Pending.bSetText = Pending.bSetRichText = true;
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
	UE_LOG(LogExtendedEOS, Verbose, TEXT("EEOSPresenceSubsystem: Setting JoinInfo (value omitted)"));
	return SetPresenceKey(TEXT("JoinInfo"), JoinInfoString);
}

bool UEEOSPresenceSubsystem::ClearPresence()
{
	// Submit Offline with everything emptied. The staged state — not the caches — carries the
	// Offline; on success the remembered state resets to Online so later setters bring the
	// user back instead of faithfully re-submitting Offline forever.
	FEEOSPendingPresence Pending;
	Pending.bClear = Pending.bSetState = true;
	Pending.State = EEOSOnlineStatus::Offline;
	Pending.bResetStateToOnlineOnSuccess = true;
	return SubmitPresence(MoveTemp(Pending), TEXT("ClearPresence"));
}

bool UEEOSPresenceSubsystem::SubmitPresence(FEEOSPendingPresence&& Pending, const FString& CallerName)
{
	if (bShuttingDown || PendingWrites.Num() >= 32)
	{
		RejectOperation(TEXT("SetPresence"), EEOSOperationCode::Busy, TEXT("Presence queue is full or shutting down.")); return false;
	}
	IOnlineSubsystem* OSS = GetEOSOnlineSubsystem();
	const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
	const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	if (!OSS || !OSS->GetPresenceInterface().IsValid() || !Local.IsValid() || !Local->IsValid()
		|| Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn || UEEOSBlueprintLibrary::ExtractEpicAccountId(Local->ToString()).IsEmpty())
	{
		RejectOperation(TEXT("SetPresence"), EEOSOperationCode::IdentityUnavailable, TEXT("Presence requires a logged-in Epic account and presence interface."));
		// A rejected setter must not complete a different pending write's legacy waiters.
		if (!bPresenceInFlight && PendingWrites.IsEmpty()) OnPresenceSet.Broadcast(false);
		return false;
	}
	Pending.RequestId = FEEOSNativeOperationLease::NextRequestId();
	Pending.IdentityId = Local->ToString(); Pending.Context = CaptureEOSContext();
	Pending.StartedUtc = FDateTime::UtcNow(); Pending.StartedSeconds = FPlatformTime::Seconds();
	if (GetEOSSettings()->bEnableOperationLogging) UE_LOG(LogExtendedEOS, Log, TEXT("EOSPresence Request=%lld Phase=Accepted Context=%lld QueueDepth=%d"),
		Pending.RequestId, Pending.Context.Generation, PendingWrites.Num() + 1);
	PendingWrites.Add(MoveTemp(Pending));
	TickPresenceIdentity(0); PumpPresenceWrites(); return true;
}

void UEEOSPresenceSubsystem::PumpPresenceWrites()
{
	if (bShuttingDown || bPresenceInFlight || PendingWrites.IsEmpty()) return;
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
	const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	const auto Presence = OSS ? OSS->GetPresenceInterface() : IOnlinePresencePtr();
	if (!Presence.IsValid() || !Local.IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn) return;
	if (!PresenceLease.TryAcquire(Presence.Get(), TEXT("LocalPresence"), this, TEXT("SetPresence"), PendingWrites[0].RequestId)) return;
	const FEEOSPendingPresence Change = PendingWrites[0]; PendingWrites.RemoveAt(0);
	if (!IsEOSContextCurrent(Change.Context))
	{
		PresenceLease.Reset(); RetireQueuedPresence(Change, TEXT("Presence context changed before dispatch."));
		PumpPresenceWrites(); return;
	}
	FEEOSPendingPresence Full;
	if (!Change.bClear)
	{
		Full.State = CachedPresenceState; Full.StatusText = CachedStatusText; Full.RichText = CachedRichText;
		Full.Properties = CachedPresenceProperties;
	}
	if (Change.bSetState) Full.State = Change.State;
	if (Change.bSetText) Full.StatusText = Change.StatusText;
	if (Change.bSetRichText) Full.RichText = Change.RichText;
	for (const auto& Pair : Change.Properties) Full.Properties.Add(Pair.Key, Pair.Value);
	Full.bResetStateToOnlineOnSuccess = Change.bResetStateToOnlineOnSuccess;
	FOnlineUserPresenceStatus Status;
	Status.State = ConvertStatus(Full.State); Status.StatusStr = Full.StatusText;
	for (const auto& Pair : Full.Properties) Status.Properties.Add(Pair.Key, Pair.Value);
	bPresenceInFlight = true; ActivePresenceInterface = Presence.Get(); ActivePresenceRequest = Change.RequestId; ActivePresenceStart = FPlatformTime::Seconds(); bStallReported = false;
	ActivePresenceContext = Change.Context;
	BeginOperation(TEXT("SetPresence"), FString(), Change.RequestId);
	const uint64 Generation = PresenceGeneration;
	const auto Weak = TWeakObjectPtr<UEEOSPresenceSubsystem>(this);
	// Retain the interface and lease independently of this subsystem until the native callback.
	Presence->SetPresence(*Local, Status, IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateLambda(
		[Weak, Presence, Lease = PresenceLease, Full, Generation, Ownership = Change.Context, Request = Change.RequestId](const FUniqueNetId& InUserId, bool bSuccess) mutable
		{
			Lease.Reset();
			auto* Self = Weak.Get();
			if (!Self || Self->bShuttingDown || Self->ActivePresenceRequest != Request) return;
			const bool bCurrent = Self->PresenceGeneration == Generation && Self->IsEOSContextCurrent(Ownership)
				&& Ownership.LocalId == InUserId.ToString();
			if (bSuccess && bCurrent)
			{
				Self->CachedPresenceState = Full.bResetStateToOnlineOnSuccess ? EEOSOnlineStatus::Online : Full.State;
				Self->CachedStatusText = Full.StatusText; Self->CachedRichText = Full.RichText; Self->CachedPresenceProperties = Full.Properties;
				Self->CachedLocalPresence.Status = Full.StatusText; Self->CachedLocalPresence.RichText = Full.RichText;
				Self->CachedLocalPresence.bIsOnline = Full.State != EEOSOnlineStatus::Offline;
			}
			Self->bPresenceInFlight = false; Self->ActivePresenceInterface = nullptr; Self->ActivePresenceRequest = 0; Self->PresenceLease.Reset();
			const auto Outcome = Self->CompleteOperation(TEXT("SetPresence"), bSuccess && bCurrent,
				!bCurrent ? EEOSOperationCode::Canceled : bSuccess ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure,
				!bCurrent ? TEXT("Presence identity changed before completion.") : bSuccess ? TEXT("Presence applied.") : TEXT("Native presence write failed."),
				FString(), bSuccess ? TEXT("Success") : TEXT("Failure"), EEOSResultSource::NativeCallback);
			FEEOSOutcomeDispatchScope Dispatch(Self, Outcome);
			Self->OnPresenceSet.Broadcast(bSuccess && bCurrent); Self->OnOperationCompleted.Broadcast(Outcome);
			Self->PumpPresenceWrites();
		}));
}

void UEEOSPresenceSubsystem::RetireQueuedPresence(const FEEOSPendingPresence& Request, const FString& Reason)
{
	FEEOSOperationOutcome Outcome; Outcome.RequestId = Request.RequestId; Outcome.Operation = TEXT("SetPresence");
	Outcome.Code = EEOSOperationCode::Canceled; Outcome.Phase = TEXT("Complete"); Outcome.Message = Reason;
	Outcome.ContextGeneration = Request.Context.Generation; Outcome.StartedUtc = Request.StartedUtc;
	Outcome.CompletedUtc = Outcome.LastTransitionUtc = FDateTime::UtcNow();
	Outcome.ElapsedMilliseconds = (FPlatformTime::Seconds() - Request.StartedSeconds) * 1000.0;
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSPresence Request=%lld Phase=QueuedRecipientRetired Context=%lld ElapsedMs=%.0f Shutdown=%d"),
		Request.RequestId, Outcome.ContextGeneration, Outcome.ElapsedMilliseconds, bShuttingDown);
	if (!bShuttingDown) { FEEOSOutcomeDispatchScope Dispatch(this, Outcome); OnOperationCompleted.Broadcast(Outcome); }
}

bool UEEOSPresenceSubsystem::TickPresenceIdentity(float)
{
	if (bShuttingDown) return false;
	const auto CurrentContext = CaptureEOSContext();
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
	const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	const FString Current = Local.IsValid() && Local->IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn ? Local->ToString() : FString();
	TOptional<FEEOSOperationOutcome> Retired;
	if (bPresenceInFlight && (!OSS || OSS->GetPresenceInterface().Get() != ActivePresenceInterface || !IsEOSContextCurrent(ActivePresenceContext)))
	{
		Retired = CompleteOperation(TEXT("SetPresence"), false, EEOSOperationCode::Canceled, TEXT("Presence context retired before completion."));
		bPresenceInFlight = false; ActivePresenceRequest = 0; ActivePresenceInterface = nullptr; PresenceLease.Reset();
	}
	if (PresenceIdentity != Current || ObservedPresenceContext.Generation != CurrentContext.Generation)
	{
		PresenceIdentity = Current; ObservedPresenceContext = CurrentContext; ++PresenceGeneration;
		CachedLocalPresence = FEEOSPresenceInfo(); CachedStatusText.Empty(); CachedRichText.Empty(); CachedPresenceProperties.Empty();
		CachedPresenceState = EEOSOnlineStatus::Online;
		TArray<FEEOSPendingPresence> Old = MoveTemp(PendingWrites); PendingWrites.Empty();
		for (auto& Request : Old)
		{
			if (IsEOSContextCurrent(Request.Context)) { PendingWrites.Add(MoveTemp(Request)); continue; }
			RetireQueuedPresence(Request, TEXT("Queued presence context retired."));
		}
	}
	if (Retired.IsSet())
	{
		FEEOSOutcomeDispatchScope Dispatch(this, Retired.GetValue());
		OnPresenceSet.Broadcast(false); OnOperationCompleted.Broadcast(Retired.GetValue());
	}
	if (bPresenceInFlight && !bStallReported && FPlatformTime::Seconds() - ActivePresenceStart > 15)
	{
		bStallReported = true;
		UE_LOG(LogExtendedEOS, Warning, TEXT("EOSPresence Request=%lld Phase=WaitingForNativeCallback ElapsedSeconds=%.0f"), ActivePresenceRequest, FPlatformTime::Seconds() - ActivePresenceStart);
	}
	PumpPresenceWrites(); return true;
}

bool UEEOSPresenceSubsystem::QueryPresence(const FString& UserId)
{
	// Every failure path broadcasts OnPresenceQueryFailed — flows waiting on this query can
	// terminate instead of hanging on a log-only dead end
	if (bShuttingDown) return false;
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
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("QueryPresence"), TEXT("CapabilityUnavailable"));
		OnPresenceQueryFailed.Broadcast(UserId);
		return false;
	}

	const auto Identity = EOSSub->GetIdentityInterface();
	if (!EEOSIdentity::HasLocalEpicAccount(Identity))
	{
		OnPresenceQueryFailed.Broadcast(UserId); return false;
	}
	// Must be constructed by the identity interface: the EOS OSS downcasts incoming ids to
	// FUniqueNetIdEOS, so a generic FUniqueNetIdString here is undefined behavior.
	// CreateUniquePlayerId on the EOS OSS returns the non-null registry EmptyId on parse
	// failure — Ptr.IsValid() alone is not a validity check, ask the id itself too.
	const FUniqueNetIdPtr NetId = Identity->CreateUniquePlayerId(UserId);
	if (!NetId.IsValid() || !NetId->IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSPresenceSubsystem::QueryPresence — Could not parse user id '%s'"), *FEEOSNativeOperationLease::SafeField(UserId));
		OnPresenceQueryFailed.Broadcast(UserId);
		return false;
	}

	PresenceInterface->QueryPresence(*NetId,
		IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateWeakLambda(this,
			[this, PresenceInterface, Generation = PresenceGeneration, Ownership = CaptureEOSContext()](const FUniqueNetId& InUserId, const bool bWasSuccessful)
			{
				if (bShuttingDown) return;
				if (Generation != PresenceGeneration || !IsEOSContextCurrent(Ownership))
				{ OnPresenceQueryFailed.Broadcast(InUserId.ToString()); return; }
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
						OnPresenceQueryFailed.Broadcast(InUserId.ToString());
						return;
					}

					OnPresenceUpdated.Broadcast(Info);
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPresenceSubsystem: Presence queried for %s — Online=%d"), *FEEOSNativeOperationLease::SafeField(InUserId.ToString()), Info.bIsOnline);
				}
				else
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSPresenceSubsystem: Failed to query presence for %s"), *FEEOSNativeOperationLease::SafeField(InUserId.ToString()));
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
