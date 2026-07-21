// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSCustomInvitesSubsystem.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "UnrealExtendedEOS.h"

#if WITH_EOS_SDK
#include "IEOSSDKManager.h"
#include "eos_custominvites.h"
#include "eos_sdk.h"

// ── Static helper to get Custom Invites handle ──────────────────────────────

static EOS_HCustomInvites GetCustomInvitesHandle()
{
	IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
	if (!SDKManager) return nullptr;

	TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
	if (Platforms.Num() == 0) return nullptr;

	EOS_HPlatform PlatformHandle = *Platforms[0];
	return EOS_Platform_GetCustomInvitesInterface(PlatformHandle);
}

static EOS_ProductUserId GetLocalProductUserId()
{
	IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
	if (!SDKManager) return nullptr;

	TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
	if (Platforms.Num() == 0) return nullptr;

	EOS_HPlatform PlatformHandle = *Platforms[0];
	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle) return nullptr;

	return EOS_Connect_GetLoggedInUserByIndex(ConnectHandle, 0);
}

/**
 * Build an EOS_ProductUserId from a user id string that may be either a bare PUID or the
 * composite "<EpicAccountId>|<ProductUserId>" every other subsystem hands out.
 * EOS_ProductUserId_FromString performs NO validation — feeding it a composite silently
 * produces a garbage PUID — so the PUID half is extracted first; an empty extraction is the
 * only reliable failure signal and yields nullptr.
 */
static EOS_ProductUserId ResolveProductUserId(const FString& CompositeOrBareId)
{
	const FString PuidStr = UEEOSBlueprintLibrary::ExtractProductUserId(CompositeOrBareId);
	if (PuidStr.IsEmpty())
	{
		return nullptr;
	}
	return EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PuidStr));
}

/**
 * Run EOS_CustomInvites_FinalizeInvite for a recorded invite. Shared by local accepts,
 * overlay accepts, and superseded-invite cleanup. Returns whether the SDK call succeeded.
 */
static bool FinalizeInviteWithSdk(const FString& SenderId, const FString& CustomInviteId, EOS_EResult ProcessingResult)
{
	if (CustomInviteId.IsEmpty())
	{
		return false;
	}

	EOS_HCustomInvites Handle = GetCustomInvitesHandle();
	if (!Handle)
	{
		return false;
	}

	EOS_ProductUserId LocalUserId = GetLocalProductUserId();
	EOS_ProductUserId SenderPUID = ResolveProductUserId(SenderId);
	if (!LocalUserId || !SenderPUID)
	{
		return false;
	}

	// The UTF-8 conversion outlives the synchronous SDK call
	FTCHARToUTF8 InviteIdUtf8(*CustomInviteId);

	EOS_CustomInvites_FinalizeInviteOptions Options = {};
	Options.ApiVersion = EOS_CUSTOMINVITES_FINALIZEINVITE_API_LATEST;
	Options.LocalUserId = LocalUserId;
	Options.TargetUserId = SenderPUID;
	Options.CustomInviteId = InviteIdUtf8.Get();
	Options.ProcessingResult = ProcessingResult;

	const EOS_EResult Result = EOS_CustomInvites_FinalizeInvite(Handle, &Options);
	if (Result != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: FinalizeInvite (%s) for %s — %s"),
			ProcessingResult == EOS_EResult::EOS_Success ? TEXT("accept") : TEXT("cancel/reject"),
			*SenderId, ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		return false;
	}
	return true;
}

// ── Static EOS_CALL callbacks ───────────────────────────────────────────────

static void EOS_CALL OnInviteReceivedStatic(const EOS_CustomInvites_OnCustomInviteReceivedCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	UEEOSCustomInvitesSubsystem* Self = static_cast<UEEOSCustomInvitesSubsystem*>(Data->ClientData);

	char SenderBuf[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
	int32_t SenderLen = sizeof(SenderBuf);
	EOS_ProductUserId_ToString(Data->TargetUserId, SenderBuf, &SenderLen);
	FString SenderId(UTF8_TO_TCHAR(SenderBuf));
	FString Payload(UTF8_TO_TCHAR(Data->Payload ? Data->Payload : ""));
	// The SDK-issued invite id is required later by EOS_CustomInvites_FinalizeInvite — keep it
	FString CustomInviteId(UTF8_TO_TCHAR(Data->CustomInviteId ? Data->CustomInviteId : ""));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: Invite received from %s (id: %s)"), *SenderId, *CustomInviteId);

	AsyncTask(ENamedThreads::GameThread, [WeakSelf = TWeakObjectPtr<UEEOSCustomInvitesSubsystem>(Self), SenderId, CustomInviteId, Payload]()
	{
		if (UEEOSCustomInvitesSubsystem* S = WeakSelf.Get())
		{
			S->HandleCustomInviteReceived(SenderId, CustomInviteId, Payload);
		}
	});
}

static void EOS_CALL OnInviteAcceptedStatic(const EOS_CustomInvites_OnCustomInviteAcceptedCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	UEEOSCustomInvitesSubsystem* Self = static_cast<UEEOSCustomInvitesSubsystem*>(Data->ClientData);

	char SenderBuf[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
	int32_t SenderLen = sizeof(SenderBuf);
	EOS_ProductUserId_ToString(Data->TargetUserId, SenderBuf, &SenderLen);
	FString SenderId(UTF8_TO_TCHAR(SenderBuf));
	FString Payload(UTF8_TO_TCHAR(Data->Payload ? Data->Payload : ""));
	// Overlay accepts must still be finalized (eos_custominvites.h:68-69) — carry the id through
	FString CustomInviteId(UTF8_TO_TCHAR(Data->CustomInviteId ? Data->CustomInviteId : ""));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: Invite accepted (overlay) from %s (id: %s)"), *SenderId, *CustomInviteId);

	AsyncTask(ENamedThreads::GameThread, [WeakSelf = TWeakObjectPtr<UEEOSCustomInvitesSubsystem>(Self), SenderId, CustomInviteId, Payload]()
	{
		if (UEEOSCustomInvitesSubsystem* S = WeakSelf.Get())
		{
			S->HandleCustomInviteAcceptedOverlay(SenderId, CustomInviteId, Payload);
		}
	});
}

static void EOS_CALL OnInviteRejectedStatic(const EOS_CustomInvites_CustomInviteRejectedCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	UEEOSCustomInvitesSubsystem* Self = static_cast<UEEOSCustomInvitesSubsystem*>(Data->ClientData);

	char SenderBuf[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
	int32_t SenderLen = sizeof(SenderBuf);
	EOS_ProductUserId_ToString(Data->TargetUserId, SenderBuf, &SenderLen);
	FString SenderId(UTF8_TO_TCHAR(SenderBuf));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: Invite rejected (overlay) from %s"), *SenderId);

	AsyncTask(ENamedThreads::GameThread, [WeakSelf = TWeakObjectPtr<UEEOSCustomInvitesSubsystem>(Self), SenderId]()
	{
		if (UEEOSCustomInvitesSubsystem* S = WeakSelf.Get())
		{
			S->HandleCustomInviteRejectedOverlay(SenderId);
		}
	});
}

static void EOS_CALL OnRTJReceivedStatic(const EOS_CustomInvites_RequestToJoinReceivedCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	UEEOSCustomInvitesSubsystem* Self = static_cast<UEEOSCustomInvitesSubsystem*>(Data->ClientData);

	char FromBuf[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
	int32_t FromLen = sizeof(FromBuf);
	EOS_ProductUserId_ToString(Data->FromUserId, FromBuf, &FromLen);
	FString FromUserId(UTF8_TO_TCHAR(FromBuf));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: Request-to-Join from %s"), *FromUserId);

	AsyncTask(ENamedThreads::GameThread, [WeakSelf = TWeakObjectPtr<UEEOSCustomInvitesSubsystem>(Self), FromUserId]()
	{
		if (UEEOSCustomInvitesSubsystem* S = WeakSelf.Get())
		{
			S->HandleRequestToJoinReceived(FromUserId);
		}
	});
}

static void EOS_CALL OnSendInviteCompleteStatic(const EOS_CustomInvites_SendCustomInviteCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;

	struct FSendContext { TWeakObjectPtr<UEEOSCustomInvitesSubsystem> Self; FString UserId; };
	TUniquePtr<FSendContext> C(static_cast<FSendContext*>(Data->ClientData));
	bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: SendCustomInvite to %s — %s"),
		*C->UserId, bSuccess ? TEXT("Success") : ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));

	AsyncTask(ENamedThreads::GameThread, [WeakSelf = C->Self, UserId = MoveTemp(C->UserId), bSuccess]()
	{
		if (UEEOSCustomInvitesSubsystem* S = WeakSelf.Get())
		{
			S->OnCustomInviteSent.Broadcast(bSuccess, UserId);
		}
	});
}

static void EOS_CALL OnSendRTJCompleteStatic(const EOS_CustomInvites_SendRequestToJoinCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;

	struct FRTJContext { TWeakObjectPtr<UEEOSCustomInvitesSubsystem> Self; FString UserId; };
	TUniquePtr<FRTJContext> C(static_cast<FRTJContext*>(Data->ClientData));
	bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: SendRequestToJoin to %s — %s"),
		*C->UserId, bSuccess ? TEXT("Success") : ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));

	AsyncTask(ENamedThreads::GameThread, [WeakSelf = C->Self, UserId = MoveTemp(C->UserId), bSuccess]()
	{
		if (UEEOSCustomInvitesSubsystem* S = WeakSelf.Get())
		{
			S->OnRequestToJoinSent.Broadcast(bSuccess, UserId);
		}
	});
}

static void EOS_CALL OnAcceptRTJCompleteStatic(const EOS_CustomInvites_AcceptRequestToJoinCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;

	struct FAcceptCtx { TWeakObjectPtr<UEEOSCustomInvitesSubsystem> Self; FString UserId; };
	TUniquePtr<FAcceptCtx> C(static_cast<FAcceptCtx*>(Data->ClientData));
	bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);

	if (!bSuccess)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: AcceptRequestToJoin for %s failed — %s"),
			*C->UserId, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
	}

	// bAccepted reflects the real SDK result — a failed accept must not report as accepted
	AsyncTask(ENamedThreads::GameThread, [WeakSelf = C->Self, UserId = MoveTemp(C->UserId), bSuccess]()
	{
		if (UEEOSCustomInvitesSubsystem* S = WeakSelf.Get())
		{
			S->OnRequestToJoinResponded.Broadcast(bSuccess, UserId);
		}
	});
}

static void EOS_CALL OnRejectRTJCompleteStatic(const EOS_CustomInvites_RejectRequestToJoinCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;

	struct FRejectCtx { TWeakObjectPtr<UEEOSCustomInvitesSubsystem> Self; FString UserId; };
	TUniquePtr<FRejectCtx> C(static_cast<FRejectCtx*>(Data->ClientData));

	if (Data->ResultCode != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: RejectRequestToJoin for %s failed — %s"),
			*C->UserId, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
	}

	// bAccepted = false: the local user responded with a rejection
	AsyncTask(ENamedThreads::GameThread, [WeakSelf = C->Self, UserId = MoveTemp(C->UserId)]()
	{
		if (UEEOSCustomInvitesSubsystem* S = WeakSelf.Get())
		{
			S->OnRequestToJoinResponded.Broadcast(false, UserId);
		}
	});
}

// ── Registration ────────────────────────────────────────────────────────────

void UEEOSCustomInvitesSubsystem::RegisterNotificationCallbacks()
{
	EOS_HCustomInvites Handle = GetCustomInvitesHandle();
	if (!Handle) return;

	EOS_CustomInvites_AddNotifyCustomInviteReceivedOptions ReceivedOpts = {};
	ReceivedOpts.ApiVersion = EOS_CUSTOMINVITES_ADDNOTIFYCUSTOMINVITERECEIVED_API_LATEST;
	NotifyCustomInviteReceivedId = EOS_CustomInvites_AddNotifyCustomInviteReceived(Handle, &ReceivedOpts, this, &OnInviteReceivedStatic);

	EOS_CustomInvites_AddNotifyCustomInviteAcceptedOptions AcceptedOpts = {};
	AcceptedOpts.ApiVersion = EOS_CUSTOMINVITES_ADDNOTIFYCUSTOMINVITEACCEPTED_API_LATEST;
	NotifyCustomInviteAcceptedId = EOS_CustomInvites_AddNotifyCustomInviteAccepted(Handle, &AcceptedOpts, this, &OnInviteAcceptedStatic);

	EOS_CustomInvites_AddNotifyCustomInviteRejectedOptions RejectedOpts = {};
	RejectedOpts.ApiVersion = EOS_CUSTOMINVITES_ADDNOTIFYCUSTOMINVITEREJECTED_API_LATEST;
	NotifyCustomInviteRejectedId = EOS_CustomInvites_AddNotifyCustomInviteRejected(Handle, &RejectedOpts, this, &OnInviteRejectedStatic);

	EOS_CustomInvites_AddNotifyRequestToJoinReceivedOptions RTJOpts = {};
	RTJOpts.ApiVersion = EOS_CUSTOMINVITES_ADDNOTIFYREQUESTTOJOINRECEIVED_API_LATEST;
	NotifyRequestToJoinReceivedId = EOS_CustomInvites_AddNotifyRequestToJoinReceived(Handle, &RTJOpts, this, &OnRTJReceivedStatic);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: SDK notification callbacks registered"));
}

void UEEOSCustomInvitesSubsystem::UnregisterNotificationCallbacks()
{
	EOS_HCustomInvites Handle = GetCustomInvitesHandle();
	if (!Handle) return;

	if (NotifyCustomInviteReceivedId != EOS_INVALID_NOTIFICATIONID)
	{
		EOS_CustomInvites_RemoveNotifyCustomInviteReceived(Handle, NotifyCustomInviteReceivedId);
		NotifyCustomInviteReceivedId = EOS_INVALID_NOTIFICATIONID;
	}
	if (NotifyCustomInviteAcceptedId != EOS_INVALID_NOTIFICATIONID)
	{
		EOS_CustomInvites_RemoveNotifyCustomInviteAccepted(Handle, NotifyCustomInviteAcceptedId);
		NotifyCustomInviteAcceptedId = EOS_INVALID_NOTIFICATIONID;
	}
	if (NotifyCustomInviteRejectedId != EOS_INVALID_NOTIFICATIONID)
	{
		EOS_CustomInvites_RemoveNotifyCustomInviteRejected(Handle, NotifyCustomInviteRejectedId);
		NotifyCustomInviteRejectedId = EOS_INVALID_NOTIFICATIONID;
	}
	if (NotifyRequestToJoinReceivedId != EOS_INVALID_NOTIFICATIONID)
	{
		EOS_CustomInvites_RemoveNotifyRequestToJoinReceived(Handle, NotifyRequestToJoinReceivedId);
		NotifyRequestToJoinReceivedId = EOS_INVALID_NOTIFICATIONID;
	}
}
#endif // WITH_EOS_SDK

// ── Initialize / Deinitialize ───────────────────────────────────────────────

void UEEOSCustomInvitesSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if WITH_EOS_SDK
	RegisterNotificationCallbacks();
#endif
}

void UEEOSCustomInvitesSubsystem::Deinitialize()
{
#if WITH_EOS_SDK
	UnregisterNotificationCallbacks();
#endif

	PendingInvites.Empty();
	PendingJoinRequests.Empty();
	CurrentPayload.Empty();
	Super::Deinitialize();
}

// ── Receive Handlers (game thread, called from the SDK notification thunks) ─

void UEEOSCustomInvitesSubsystem::HandleCustomInviteReceived(const FString& SenderId, const FString& CustomInviteId, const FString& Payload)
{
	// A newer invite from the same sender supersedes the old one. The SDK still expects the
	// OLD invite id to be finalized — nothing else ever will once we drop it — so close it
	// out as Canceled before replacing.
	if (const FEEOSPendingCustomInvite* Existing = PendingInvites.Find(SenderId))
	{
		if (!Existing->CustomInviteId.IsEmpty() && Existing->CustomInviteId != CustomInviteId)
		{
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: Invite %s from %s superseded — finalizing old invite as canceled"),
				*Existing->CustomInviteId, *SenderId);
#if WITH_EOS_SDK
			FinalizeInviteWithSdk(SenderId, Existing->CustomInviteId, EOS_EResult::EOS_Canceled);
#endif
		}
	}

	// Record the invite BEFORE broadcasting so a listener can call AcceptCustomInvite
	// synchronously from the event.
	FEEOSPendingCustomInvite& Pending = PendingInvites.FindOrAdd(SenderId);
	Pending.Payload = Payload;
	Pending.CustomInviteId = CustomInviteId;

	OnCustomInviteReceived.Broadcast(SenderId, Payload);
}

void UEEOSCustomInvitesSubsystem::HandleCustomInviteAcceptedOverlay(const FString& SenderId, const FString& CustomInviteId, const FString& Payload)
{
#if WITH_EOS_SDK
	// Overlay accepts still require FinalizeInvite (eos_custominvites.h:68-69). Prefer the
	// invite id carried by the notification; fall back to the recorded pending entry.
	FString InviteIdToFinalize = CustomInviteId;
	if (InviteIdToFinalize.IsEmpty())
	{
		if (const FEEOSPendingCustomInvite* Pending = PendingInvites.Find(SenderId))
		{
			InviteIdToFinalize = Pending->CustomInviteId;
		}
	}

	if (!InviteIdToFinalize.IsEmpty())
	{
		FinalizeInviteWithSdk(SenderId, InviteIdToFinalize, EOS_EResult::EOS_Success);
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: Overlay accept from %s has no CustomInviteId — skipping FinalizeInvite"), *SenderId);
	}
#endif

	// Evict so counts stay honest and a later AcceptCustomInvite can't re-finalize this invite
	PendingInvites.Remove(SenderId);

	OnCustomInviteAccepted.Broadcast(SenderId, Payload);
}

void UEEOSCustomInvitesSubsystem::HandleCustomInviteRejectedOverlay(const FString& SenderId)
{
	// Overlay rejects are auto-finalized inside the SDK (eos_custominvites.h:91-92) —
	// no FinalizeInvite here, just drop the stale pending entry.
	PendingInvites.Remove(SenderId);

	OnCustomInviteRejected.Broadcast(SenderId);
}

void UEEOSCustomInvitesSubsystem::HandleRequestToJoinReceived(const FString& FromUserId)
{
	PendingJoinRequests.AddUnique(FromUserId);
	OnRequestToJoinReceived.Broadcast(FromUserId);
}

// ── Invite Management ────────────────────────────────────────────────────────

void UEEOSCustomInvitesSubsystem::SetCustomInvitePayload(const FString& Payload)
{
	CurrentPayload = Payload;

#if WITH_EOS_SDK
	if (Payload.Len() > EOS_CUSTOMINVITES_MAX_PAYLOAD_LENGTH)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: Payload is %d chars — the SDK maximum is %d, EOS_CustomInvites_SetCustomInvite will reject it"),
			Payload.Len(), EOS_CUSTOMINVITES_MAX_PAYLOAD_LENGTH);
	}

	EOS_HCustomInvites Handle = GetCustomInvitesHandle();
	if (Handle)
	{
		EOS_ProductUserId LocalUserId = GetLocalProductUserId();
		if (LocalUserId)
		{
			EOS_CustomInvites_SetCustomInviteOptions Options = {};
			Options.ApiVersion = EOS_CUSTOMINVITES_SETCUSTOMINVITE_API_LATEST;
			Options.LocalUserId = LocalUserId;
			FTCHARToUTF8 PayloadUtf8(*Payload);
			Options.Payload = PayloadUtf8.Get();

			EOS_EResult Result = EOS_CustomInvites_SetCustomInvite(Handle, &Options);
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: SetCustomInvite (SDK) — %s"),
				ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
			return;
		}
	}
#endif

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: Payload set locally — '%s'"), *Payload);
}

bool UEEOSCustomInvitesSubsystem::SendCustomInvite(const FString& TargetUserId)
{
#if WITH_EOS_SDK
	EOS_HCustomInvites Handle = GetCustomInvitesHandle();
	if (Handle)
	{
		EOS_ProductUserId LocalUserId = GetLocalProductUserId();
		// Composite ids are normalized to the PUID half — see ResolveProductUserId
		EOS_ProductUserId TargetId = ResolveProductUserId(TargetUserId);

		if (LocalUserId && TargetId)
		{
			EOS_CustomInvites_SendCustomInviteOptions Options = {};
			Options.ApiVersion = EOS_CUSTOMINVITES_SENDCUSTOMINVITE_API_LATEST;
			Options.LocalUserId = LocalUserId;
			Options.TargetUserIds = &TargetId;
			Options.TargetUserIdsCount = 1;

			struct FSendContext { TWeakObjectPtr<UEEOSCustomInvitesSubsystem> Self; FString UserId; };
			FSendContext* Ctx = new FSendContext{this, TargetUserId};

			EOS_CustomInvites_SendCustomInvite(Handle, &Options, Ctx, &OnSendInviteCompleteStatic);
			return true;
		}

		if (!TargetId)
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: SendCustomInvite — '%s' has no Product User ID half"), *TargetUserId);
		}
	}
#endif

	// No SDK / no login / bad target id: nothing was sent — never fake success
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: SendCustomInvite to %s failed — EOS SDK unavailable, not logged in, or invalid target id"), *TargetUserId);
	OnCustomInviteSent.Broadcast(false, TargetUserId);
	return false;
}

bool UEEOSCustomInvitesSubsystem::SendCustomInviteBatch(const TArray<FString>& TargetUserIds)
{
	bool bAllStarted = TargetUserIds.Num() > 0;
	for (const FString& UserId : TargetUserIds)
	{
		bAllStarted &= SendCustomInvite(UserId);
	}
	return bAllStarted;
}

bool UEEOSCustomInvitesSubsystem::AcceptCustomInvite(const FString& SenderId)
{
	// Pending invites are keyed by the bare sender PUID recorded from the receive
	// notification — normalize composite inputs so both forms hit the same entry
	const FString SenderKey = UEEOSBlueprintLibrary::ExtractProductUserId(SenderId);
	if (SenderKey.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: AcceptCustomInvite — '%s' has no Product User ID half"), *SenderId);
		return false;
	}

	// Evict-on-accept: removing the entry up front guarantees a second accept (or an accept
	// racing an overlay accept) cannot re-finalize an already-finalized invite id
	FEEOSPendingCustomInvite Invite;
	if (!PendingInvites.RemoveAndCopyValue(SenderKey, Invite))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: No pending invite from %s (already accepted/rejected, or never received)"), *SenderKey);
		return false;
	}

#if WITH_EOS_SDK
	if (!Invite.CustomInviteId.IsEmpty())
	{
		FinalizeInviteWithSdk(SenderKey, Invite.CustomInviteId, EOS_EResult::EOS_Success);
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: Pending invite from %s has no CustomInviteId — skipping FinalizeInvite"), *SenderKey);
	}
#endif

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: Accepted invite from %s"), *SenderKey);
	OnCustomInviteAccepted.Broadcast(SenderKey, Invite.Payload);
	return true;
}

bool UEEOSCustomInvitesSubsystem::RejectCustomInvite(const FString& SenderId)
{
	// Same key normalization as AcceptCustomInvite; a raw input without a PUID half cannot
	// match any recorded invite, so fall back to the raw string for the local-only broadcast
	const FString SenderKey = UEEOSBlueprintLibrary::ExtractProductUserId(SenderId);

	FEEOSPendingCustomInvite Invite;
	const bool bHadPendingInvite = !SenderKey.IsEmpty() && PendingInvites.RemoveAndCopyValue(SenderKey, Invite);

	bool bFinalized = false;
#if WITH_EOS_SDK
	if (bHadPendingInvite && !Invite.CustomInviteId.IsEmpty())
	{
		bFinalized = FinalizeInviteWithSdk(SenderKey, Invite.CustomInviteId, EOS_EResult::EOS_Canceled);
	}
#endif

	if (!bHadPendingInvite)
	{
		UE_LOG(LogExtendedEOS, Verbose, TEXT("EEOSCustomInvites: No pending invite recorded from %s — rejecting locally only"), *SenderId);
	}

	const FString& BroadcastId = !SenderKey.IsEmpty() ? SenderKey : SenderId;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: Rejected invite from %s"), *BroadcastId);
	OnCustomInviteRejected.Broadcast(BroadcastId);
	return bHadPendingInvite && (bFinalized || Invite.CustomInviteId.IsEmpty());
}

// ── Request to Join ──────────────────────────────────────────────────────────

bool UEEOSCustomInvitesSubsystem::SendRequestToJoin(const FString& TargetUserId)
{
#if WITH_EOS_SDK
	EOS_HCustomInvites Handle = GetCustomInvitesHandle();
	if (Handle)
	{
		EOS_ProductUserId LocalUserId = GetLocalProductUserId();
		// Composite ids are normalized to the PUID half — see ResolveProductUserId
		EOS_ProductUserId TargetId = ResolveProductUserId(TargetUserId);

		if (LocalUserId && TargetId)
		{
			EOS_CustomInvites_SendRequestToJoinOptions Options = {};
			Options.ApiVersion = EOS_CUSTOMINVITES_SENDREQUESTTOJOIN_API_LATEST;
			Options.LocalUserId = LocalUserId;
			Options.TargetUserId = TargetId;

			struct FRTJContext { TWeakObjectPtr<UEEOSCustomInvitesSubsystem> Self; FString UserId; };
			FRTJContext* Ctx = new FRTJContext{this, TargetUserId};

			EOS_CustomInvites_SendRequestToJoin(Handle, &Options, Ctx, &OnSendRTJCompleteStatic);
			return true;
		}

		if (!TargetId)
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: SendRequestToJoin — '%s' has no Product User ID half"), *TargetUserId);
		}
	}
#endif

	// No SDK / no login / bad target id: nothing was sent — never fake success
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: SendRequestToJoin to %s failed — EOS SDK unavailable, not logged in, or invalid target id"), *TargetUserId);
	OnRequestToJoinSent.Broadcast(false, TargetUserId);
	return false;
}

bool UEEOSCustomInvitesSubsystem::AcceptRequestToJoin(const FString& FromUserId)
{
	// Pending join requests are recorded as bare PUIDs — normalize composite inputs
	const FString FromKey = UEEOSBlueprintLibrary::ExtractProductUserId(FromUserId);
	if (FromKey.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: AcceptRequestToJoin — '%s' has no Product User ID half"), *FromUserId);
		OnRequestToJoinResponded.Broadcast(false, FromUserId);
		return false;
	}
	PendingJoinRequests.Remove(FromKey);

#if WITH_EOS_SDK
	EOS_HCustomInvites Handle = GetCustomInvitesHandle();
	if (Handle)
	{
		EOS_ProductUserId LocalUserId = GetLocalProductUserId();
		EOS_ProductUserId FromId = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*FromKey));

		if (LocalUserId && FromId)
		{
			EOS_CustomInvites_AcceptRequestToJoinOptions Options = {};
			Options.ApiVersion = EOS_CUSTOMINVITES_ACCEPTREQUESTTOJOIN_API_LATEST;
			Options.LocalUserId = LocalUserId;
			Options.TargetUserId = FromId;

			struct FAcceptCtx { TWeakObjectPtr<UEEOSCustomInvitesSubsystem> Self; FString UserId; };
			FAcceptCtx* Ctx = new FAcceptCtx{this, FromKey};

			EOS_CustomInvites_AcceptRequestToJoin(Handle, &Options, Ctx, &OnAcceptRTJCompleteStatic);
			return true;
		}
	}
#endif

	// No SDK / no login: the accept never reached the sender — never fake success
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: AcceptRequestToJoin from %s failed — EOS SDK unavailable or not logged in"), *FromKey);
	OnRequestToJoinResponded.Broadcast(false, FromKey);
	return false;
}

bool UEEOSCustomInvitesSubsystem::RejectRequestToJoin(const FString& FromUserId)
{
	// Pending join requests are recorded as bare PUIDs — normalize composite inputs
	const FString FromKey = UEEOSBlueprintLibrary::ExtractProductUserId(FromUserId);
	if (FromKey.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: RejectRequestToJoin — '%s' has no Product User ID half"), *FromUserId);
		OnRequestToJoinResponded.Broadcast(false, FromUserId);
		return false;
	}
	PendingJoinRequests.Remove(FromKey);

#if WITH_EOS_SDK
	EOS_HCustomInvites Handle = GetCustomInvitesHandle();
	if (Handle)
	{
		EOS_ProductUserId LocalUserId = GetLocalProductUserId();
		EOS_ProductUserId FromId = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*FromKey));

		if (LocalUserId && FromId)
		{
			EOS_CustomInvites_RejectRequestToJoinOptions Options = {};
			Options.ApiVersion = EOS_CUSTOMINVITES_REJECTREQUESTTOJOIN_API_LATEST;
			Options.LocalUserId = LocalUserId;
			Options.TargetUserId = FromId;

			struct FRejectCtx { TWeakObjectPtr<UEEOSCustomInvitesSubsystem> Self; FString UserId; };
			FRejectCtx* Ctx = new FRejectCtx{this, FromKey};

			EOS_CustomInvites_RejectRequestToJoin(Handle, &Options, Ctx, &OnRejectRTJCompleteStatic);
			return true;
		}
	}
#endif

	// No SDK / no login: the rejection never reached the sender, but the local pending
	// entry is dropped either way — broadcast the rejection response exactly once
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: RejectRequestToJoin from %s — EOS SDK unavailable, rejected locally only"), *FromKey);
	OnRequestToJoinResponded.Broadcast(false, FromKey);
	return false;
}

// ── Queries ──────────────────────────────────────────────────────────────────

FString UEEOSCustomInvitesSubsystem::GetCurrentPayload() const
{
	return CurrentPayload;
}

bool UEEOSCustomInvitesSubsystem::HasPendingInvites() const
{
	return PendingInvites.Num() > 0;
}

int32 UEEOSCustomInvitesSubsystem::GetPendingInviteCount() const
{
	return PendingInvites.Num();
}
