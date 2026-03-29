// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSCustomInvitesSubsystem.h"
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

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: Invite received from %s"), *SenderId);

	AsyncTask(ENamedThreads::GameThread, [Self, SenderId, Payload]()
	{
		Self->OnCustomInviteReceived.Broadcast(SenderId, Payload);
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

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: Invite accepted from %s"), *SenderId);

	AsyncTask(ENamedThreads::GameThread, [Self, SenderId, Payload]()
	{
		Self->OnCustomInviteAccepted.Broadcast(SenderId, Payload);
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

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: Invite rejected by %s"), *SenderId);

	AsyncTask(ENamedThreads::GameThread, [Self, SenderId]()
	{
		Self->OnCustomInviteRejected.Broadcast(SenderId);
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

	AsyncTask(ENamedThreads::GameThread, [Self, FromUserId]()
	{
		Self->OnRequestToJoinReceived.Broadcast(FromUserId);
	});
}

static void EOS_CALL OnSendInviteCompleteStatic(const EOS_CustomInvites_SendCustomInviteCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;

	struct FSendContext { UEEOSCustomInvitesSubsystem* Self; FString UserId; };
	FSendContext* C = static_cast<FSendContext*>(Data->ClientData);
	bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: SendCustomInvite to %s — %s"),
		*C->UserId, bSuccess ? TEXT("Success") : ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));

	AsyncTask(ENamedThreads::GameThread, [C, bSuccess]()
	{
		C->Self->OnCustomInviteSent.Broadcast(bSuccess, C->UserId);
		delete C;
	});
}

static void EOS_CALL OnSendRTJCompleteStatic(const EOS_CustomInvites_SendRequestToJoinCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;

	struct FRTJContext { UEEOSCustomInvitesSubsystem* Self; FString UserId; };
	FRTJContext* C = static_cast<FRTJContext*>(Data->ClientData);
	bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);

	AsyncTask(ENamedThreads::GameThread, [C, bSuccess]()
	{
		C->Self->OnCustomInviteSent.Broadcast(bSuccess, C->UserId);
		delete C;
	});
}

static void EOS_CALL OnAcceptRTJCompleteStatic(const EOS_CustomInvites_AcceptRequestToJoinCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;

	struct FAcceptCtx { UEEOSCustomInvitesSubsystem* Self; FString UserId; };
	FAcceptCtx* C = static_cast<FAcceptCtx*>(Data->ClientData);
	bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);

	AsyncTask(ENamedThreads::GameThread, [C, bSuccess]()
	{
		C->Self->OnRequestToJoinResponded.Broadcast(true, C->UserId);
		delete C;
	});
}

static void EOS_CALL OnRejectRTJCompleteStatic(const EOS_CustomInvites_RejectRequestToJoinCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;

	struct FRejectCtx { UEEOSCustomInvitesSubsystem* Self; FString UserId; };
	FRejectCtx* C = static_cast<FRejectCtx*>(Data->ClientData);

	AsyncTask(ENamedThreads::GameThread, [C]()
	{
		C->Self->OnRequestToJoinResponded.Broadcast(false, C->UserId);
		delete C;
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

// ── Invite Management ────────────────────────────────────────────────────────

void UEEOSCustomInvitesSubsystem::SetCustomInvitePayload(const FString& Payload)
{
	CurrentPayload = Payload;

#if WITH_EOS_SDK
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

void UEEOSCustomInvitesSubsystem::SendCustomInvite(const FString& TargetUserId)
{
#if WITH_EOS_SDK
	EOS_HCustomInvites Handle = GetCustomInvitesHandle();
	if (Handle)
	{
		EOS_ProductUserId LocalUserId = GetLocalProductUserId();
		EOS_ProductUserId TargetId = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*TargetUserId));

		if (LocalUserId && TargetId)
		{
			EOS_CustomInvites_SendCustomInviteOptions Options = {};
			Options.ApiVersion = EOS_CUSTOMINVITES_SENDCUSTOMINVITE_API_LATEST;
			Options.LocalUserId = LocalUserId;
			Options.TargetUserIds = &TargetId;
			Options.TargetUserIdsCount = 1;

			struct FSendContext { UEEOSCustomInvitesSubsystem* Self; FString UserId; };
			FSendContext* Ctx = new FSendContext{this, TargetUserId};

			EOS_CustomInvites_SendCustomInvite(Handle, &Options, Ctx, &OnSendInviteCompleteStatic);
			return;
		}
	}
#endif

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: SendCustomInvite (local) to %s"), *TargetUserId);
	OnCustomInviteSent.Broadcast(true, TargetUserId);
}

void UEEOSCustomInvitesSubsystem::SendCustomInviteBatch(const TArray<FString>& TargetUserIds)
{
	for (const FString& UserId : TargetUserIds)
	{
		SendCustomInvite(UserId);
	}
}

void UEEOSCustomInvitesSubsystem::AcceptCustomInvite(const FString& SenderId)
{
	FString Payload;
	if (PendingInvites.RemoveAndCopyValue(SenderId, Payload))
	{
#if WITH_EOS_SDK
		EOS_HCustomInvites Handle = GetCustomInvitesHandle();
		if (Handle)
		{
			EOS_ProductUserId LocalUserId = GetLocalProductUserId();
			EOS_ProductUserId SenderPUID = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*SenderId));
			if (LocalUserId && SenderPUID)
			{
				EOS_CustomInvites_FinalizeInviteOptions Options = {};
				Options.ApiVersion = EOS_CUSTOMINVITES_FINALIZEINVITE_API_LATEST;
				Options.LocalUserId = LocalUserId;
				Options.TargetUserId = SenderPUID;
				Options.CustomInviteId = nullptr;
				Options.ProcessingResult = EOS_EResult::EOS_Success;

				EOS_CustomInvites_FinalizeInvite(Handle, &Options);
			}
		}
#endif

		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: Accepted invite from %s"), *SenderId);
		OnCustomInviteAccepted.Broadcast(SenderId, Payload);
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSCustomInvites: No pending invite from %s"), *SenderId);
	}
}

void UEEOSCustomInvitesSubsystem::RejectCustomInvite(const FString& SenderId)
{
	PendingInvites.Remove(SenderId);

#if WITH_EOS_SDK
	EOS_HCustomInvites Handle = GetCustomInvitesHandle();
	if (Handle)
	{
		EOS_ProductUserId LocalUserId = GetLocalProductUserId();
		EOS_ProductUserId SenderPUID = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*SenderId));
		if (LocalUserId && SenderPUID)
		{
			EOS_CustomInvites_FinalizeInviteOptions Options = {};
			Options.ApiVersion = EOS_CUSTOMINVITES_FINALIZEINVITE_API_LATEST;
			Options.LocalUserId = LocalUserId;
			Options.TargetUserId = SenderPUID;
			Options.CustomInviteId = nullptr;
			Options.ProcessingResult = EOS_EResult::EOS_Canceled;

			EOS_CustomInvites_FinalizeInvite(Handle, &Options);
		}
	}
#endif

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: Rejected invite from %s"), *SenderId);
	OnCustomInviteRejected.Broadcast(SenderId);
}

// ── Request to Join ──────────────────────────────────────────────────────────

void UEEOSCustomInvitesSubsystem::SendRequestToJoin(const FString& TargetUserId)
{
#if WITH_EOS_SDK
	EOS_HCustomInvites Handle = GetCustomInvitesHandle();
	if (Handle)
	{
		EOS_ProductUserId LocalUserId = GetLocalProductUserId();
		EOS_ProductUserId TargetId = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*TargetUserId));

		if (LocalUserId && TargetId)
		{
			EOS_CustomInvites_SendRequestToJoinOptions Options = {};
			Options.ApiVersion = EOS_CUSTOMINVITES_SENDREQUESTTOJOIN_API_LATEST;
			Options.LocalUserId = LocalUserId;
			Options.TargetUserId = TargetId;

			struct FRTJContext { UEEOSCustomInvitesSubsystem* Self; FString UserId; };
			FRTJContext* Ctx = new FRTJContext{this, TargetUserId};

			EOS_CustomInvites_SendRequestToJoin(Handle, &Options, Ctx, &OnSendRTJCompleteStatic);
			return;
		}
	}
#endif

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: SendRequestToJoin (local) to %s"), *TargetUserId);
}

void UEEOSCustomInvitesSubsystem::AcceptRequestToJoin(const FString& FromUserId)
{
	PendingJoinRequests.Remove(FromUserId);

#if WITH_EOS_SDK
	EOS_HCustomInvites Handle = GetCustomInvitesHandle();
	if (Handle)
	{
		EOS_ProductUserId LocalUserId = GetLocalProductUserId();
		EOS_ProductUserId FromId = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*FromUserId));

		if (LocalUserId && FromId)
		{
			EOS_CustomInvites_AcceptRequestToJoinOptions Options = {};
			Options.ApiVersion = EOS_CUSTOMINVITES_ACCEPTREQUESTTOJOIN_API_LATEST;
			Options.LocalUserId = LocalUserId;
			Options.TargetUserId = FromId;

			struct FAcceptCtx { UEEOSCustomInvitesSubsystem* Self; FString UserId; };
			FAcceptCtx* Ctx = new FAcceptCtx{this, FromUserId};

			EOS_CustomInvites_AcceptRequestToJoin(Handle, &Options, Ctx, &OnAcceptRTJCompleteStatic);
			return;
		}
	}
#endif

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: Accepted RTJ from %s"), *FromUserId);
	OnRequestToJoinResponded.Broadcast(true, FromUserId);
}

void UEEOSCustomInvitesSubsystem::RejectRequestToJoin(const FString& FromUserId)
{
	PendingJoinRequests.Remove(FromUserId);

#if WITH_EOS_SDK
	EOS_HCustomInvites Handle = GetCustomInvitesHandle();
	if (Handle)
	{
		EOS_ProductUserId LocalUserId = GetLocalProductUserId();
		EOS_ProductUserId FromId = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*FromUserId));

		if (LocalUserId && FromId)
		{
			EOS_CustomInvites_RejectRequestToJoinOptions Options = {};
			Options.ApiVersion = EOS_CUSTOMINVITES_REJECTREQUESTTOJOIN_API_LATEST;
			Options.LocalUserId = LocalUserId;
			Options.TargetUserId = FromId;

			struct FRejectCtx { UEEOSCustomInvitesSubsystem* Self; FString UserId; };
			FRejectCtx* Ctx = new FRejectCtx{this, FromUserId};

			EOS_CustomInvites_RejectRequestToJoin(Handle, &Options, Ctx, &OnRejectRTJCompleteStatic);
			return;
		}
	}
#endif

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSCustomInvites: Rejected RTJ from %s"), *FromUserId);
	OnRequestToJoinResponded.Broadcast(false, FromUserId);
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
