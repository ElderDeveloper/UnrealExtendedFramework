// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSAntiCheatClientSubsystem.h"
#include "UnrealExtendedEOS.h"

#if WITH_EOS_SDK
#include "IEOSSDKManager.h"
#include "eos_anticheatclient.h"
#include "eos_sdk.h"

// ── Static helper ───────────────────────────────────────────────────────────

static EOS_HAntiCheatClient GetAntiCheatClientHandle()
{
	IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
	if (!SDKManager) return nullptr;

	TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
	if (Platforms.Num() == 0) return nullptr;

	EOS_HPlatform PlatformHandle = *Platforms[0];
	return EOS_Platform_GetAntiCheatClientInterface(PlatformHandle);
}

// ── Static EOS_CALL callbacks ───────────────────────────────────────────────

static void EOS_CALL OnACClientMessageToServer(const EOS_AntiCheatClient_OnMessageToServerCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	UEEOSAntiCheatClientSubsystem* Self = static_cast<UEEOSAntiCheatClientSubsystem*>(Data->ClientData);

	TArray<uint8> MsgData;
	MsgData.Append(static_cast<const uint8*>(Data->MessageData), Data->MessageDataSizeBytes);

	AsyncTask(ENamedThreads::GameThread, [Self, MsgData]()
	{
		Self->OnMessageToServer.Broadcast(MsgData);
	});
}

static void EOS_CALL OnACClientIntegrityViolated(const EOS_AntiCheatClient_OnClientIntegrityViolatedCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	UEEOSAntiCheatClientSubsystem* Self = static_cast<UEEOSAntiCheatClientSubsystem*>(Data->ClientData);

	FString Message(UTF8_TO_TCHAR(Data->ViolationMessage ? Data->ViolationMessage : ""));

	AsyncTask(ENamedThreads::GameThread, [Self, Message]()
	{
		Self->OnIntegrityViolated.Broadcast(Message);
	});
}

static void EOS_CALL OnACClientPeerActionRequired(const EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	UEEOSAntiCheatClientSubsystem* Self = static_cast<UEEOSAntiCheatClientSubsystem*>(Data->ClientData);

	EEOSAntiCheatAction Action = EEOSAntiCheatAction::None;
	if (Data->ClientAction == EOS_EAntiCheatCommonClientAction::EOS_ACCCA_RemovePlayer)
	{
		Action = EEOSAntiCheatAction::RemovePlayer;
	}
	FString Msg(UTF8_TO_TCHAR(Data->ActionReasonDetailsString ? Data->ActionReasonDetailsString : ""));

	AsyncTask(ENamedThreads::GameThread, [Self, Action, Msg]()
	{
		Self->OnClientActionRequired.Broadcast(Action, Msg);
	});
}

static void EOS_CALL OnACClientPeerAuthChanged(const EOS_AntiCheatCommon_OnClientAuthStatusChangedCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	UEEOSAntiCheatClientSubsystem* Self = static_cast<UEEOSAntiCheatClientSubsystem*>(Data->ClientData);

	FString PeerId = FString::Printf(TEXT("%llu"), (uint64)Data->ClientHandle);
	bool bAuth = (Data->ClientAuthStatus == EOS_EAntiCheatCommonClientAuthStatus::EOS_ACCCAS_RemoteAuthComplete);

	AsyncTask(ENamedThreads::GameThread, [Self, PeerId, bAuth]()
	{
		Self->OnPeerAuthStatusChanged.Broadcast(PeerId, bAuth);
	});
}

#endif // WITH_EOS_SDK

// ── Initialize / Deinitialize ───────────────────────────────────────────────

void UEEOSAntiCheatClientSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSAntiCheatClientSubsystem::Deinitialize()
{
	if (bSessionActive)
	{
		EndSession();
	}
	Super::Deinitialize();
}

// ── Session Management ──────────────────────────────────────────────────────

void UEEOSAntiCheatClientSubsystem::BeginSession()
{
	if (bSessionActive)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatClient: Session already active"));
		return;
	}

#if WITH_EOS_SDK
	EOS_HAntiCheatClient Handle = GetAntiCheatClientHandle();
	if (Handle)
	{
		// Register notification callbacks before beginning session
		EOS_AntiCheatClient_AddNotifyMessageToServerOptions MsgOpts = {};
		MsgOpts.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYMESSAGETOSERVER_API_LATEST;
		NotifyMessageToServerId = EOS_AntiCheatClient_AddNotifyMessageToServer(Handle, &MsgOpts, this, &OnACClientMessageToServer);

		EOS_AntiCheatClient_AddNotifyClientIntegrityViolatedOptions IntOpts = {};
		IntOpts.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYCLIENTINTEGRITYVIOLATED_API_LATEST;
		NotifyIntegrityViolatedId = EOS_AntiCheatClient_AddNotifyClientIntegrityViolated(Handle, &IntOpts, this, &OnACClientIntegrityViolated);

		EOS_AntiCheatClient_AddNotifyPeerActionRequiredOptions ActionOpts = {};
		ActionOpts.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYPEERACTIONREQUIRED_API_LATEST;
		NotifyPeerActionRequiredId = EOS_AntiCheatClient_AddNotifyPeerActionRequired(Handle, &ActionOpts, this, &OnACClientPeerActionRequired);

		EOS_AntiCheatClient_AddNotifyPeerAuthStatusChangedOptions AuthOpts = {};
		AuthOpts.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYPEERAUTHSTATUSCHANGED_API_LATEST;
		NotifyPeerAuthChangedId = EOS_AntiCheatClient_AddNotifyPeerAuthStatusChanged(Handle, &AuthOpts, this, &OnACClientPeerAuthChanged);

		// Begin session
		EOS_AntiCheatClient_BeginSessionOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCLIENT_BEGINSESSION_API_LATEST;
		Options.Mode = EOS_EAntiCheatClientMode::EOS_ACCM_ClientServer;

		// Try to get the local Product User ID
		IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
		if (SDKManager)
		{
			TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
			if (Platforms.Num() > 0)
			{
				EOS_HPlatform PlatformHandle = *Platforms[0];
				EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
				if (ConnectHandle)
				{
					Options.LocalUserId = EOS_Connect_GetLoggedInUserByIndex(ConnectHandle, 0);
				}
			}
		}

		EOS_EResult Result = EOS_AntiCheatClient_BeginSession(Handle, &Options);
		if (Result == EOS_EResult::EOS_Success)
		{
			bSessionActive = true;
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatClient: Session started (SDK)"));
			return;
		}
		else
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatClient: BeginSession failed — %s"),
				ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		}
	}
#endif

	bSessionActive = true;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatClient: Session started (local fallback)"));
}

void UEEOSAntiCheatClientSubsystem::EndSession()
{
	if (!bSessionActive) return;

#if WITH_EOS_SDK
	EOS_HAntiCheatClient Handle = GetAntiCheatClientHandle();
	if (Handle)
	{
		EOS_AntiCheatClient_EndSessionOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCLIENT_ENDSESSION_API_LATEST;
		EOS_AntiCheatClient_EndSession(Handle, &Options);

		// Remove notification callbacks
		if (NotifyMessageToServerId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_AntiCheatClient_RemoveNotifyMessageToServer(Handle, NotifyMessageToServerId);
			NotifyMessageToServerId = EOS_INVALID_NOTIFICATIONID;
		}
		if (NotifyIntegrityViolatedId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_AntiCheatClient_RemoveNotifyClientIntegrityViolated(Handle, NotifyIntegrityViolatedId);
			NotifyIntegrityViolatedId = EOS_INVALID_NOTIFICATIONID;
		}
		if (NotifyPeerAuthChangedId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_AntiCheatClient_RemoveNotifyPeerAuthStatusChanged(Handle, NotifyPeerAuthChangedId);
			NotifyPeerAuthChangedId = EOS_INVALID_NOTIFICATIONID;
		}
		if (NotifyPeerActionRequiredId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_AntiCheatClient_RemoveNotifyPeerActionRequired(Handle, NotifyPeerActionRequiredId);
			NotifyPeerActionRequiredId = EOS_INVALID_NOTIFICATIONID;
		}

		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatClient: Session ended (SDK)"));
	}
#endif

	bSessionActive = false;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatClient: Session ended"));
}

// ── Message Passing ─────────────────────────────────────────────────────────

void UEEOSAntiCheatClientSubsystem::ReceiveMessageFromServer(const TArray<uint8>& MessageData)
{
	if (!bSessionActive) return;

#if WITH_EOS_SDK
	EOS_HAntiCheatClient Handle = GetAntiCheatClientHandle();
	if (Handle)
	{
		EOS_AntiCheatClient_ReceiveMessageFromServerOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCLIENT_RECEIVEMESSAGEFROMSERVER_API_LATEST;
		Options.Data = MessageData.GetData();
		Options.DataLengthBytes = MessageData.Num();

		EOS_AntiCheatClient_ReceiveMessageFromServer(Handle, &Options);
	}
#endif
}

void UEEOSAntiCheatClientSubsystem::ReceiveMessageFromPeer(const FString& PeerId, const TArray<uint8>& MessageData)
{
	if (!bSessionActive) return;

#if WITH_EOS_SDK
	EOS_HAntiCheatClient Handle = GetAntiCheatClientHandle();
	if (Handle)
	{
		EOS_AntiCheatClient_ReceiveMessageFromPeerOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCLIENT_RECEIVEMESSAGEFROMPEER_API_LATEST;
		Options.Data = MessageData.GetData();
		Options.DataLengthBytes = MessageData.Num();

		EOS_ProductUserId PeerPUID = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PeerId));
		Options.PeerHandle = (EOS_AntiCheatCommon_ClientHandle)PeerPUID;

		EOS_AntiCheatClient_ReceiveMessageFromPeer(Handle, &Options);
	}
#endif
}

// ── P2P Peer Management ─────────────────────────────────────────────────────

void UEEOSAntiCheatClientSubsystem::RegisterPeer(const FString& PeerId)
{
	if (!bSessionActive) return;

#if WITH_EOS_SDK
	EOS_HAntiCheatClient Handle = GetAntiCheatClientHandle();
	if (Handle)
	{
		EOS_AntiCheatClient_RegisterPeerOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCLIENT_REGISTERPEER_API_LATEST;

		EOS_ProductUserId PeerPUID = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PeerId));
		Options.PeerHandle = (EOS_AntiCheatCommon_ClientHandle)PeerPUID;
		Options.PeerProductUserId = PeerPUID;
		Options.ClientType = EOS_EAntiCheatCommonClientType::EOS_ACCCT_ProtectedClient;
		Options.ClientPlatform = EOS_EAntiCheatCommonClientPlatform::EOS_ACCCP_Windows;
		Options.AuthenticationTimeout = 60;

		EOS_EResult Result = EOS_AntiCheatClient_RegisterPeer(Handle, &Options);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatClient: RegisterPeer %s — %s"),
			*PeerId, ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
	}
#endif
}

void UEEOSAntiCheatClientSubsystem::UnregisterPeer(const FString& PeerId)
{
	if (!bSessionActive) return;

#if WITH_EOS_SDK
	EOS_HAntiCheatClient Handle = GetAntiCheatClientHandle();
	if (Handle)
	{
		EOS_AntiCheatClient_UnregisterPeerOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCLIENT_UNREGISTERPEER_API_LATEST;

		EOS_ProductUserId PeerPUID = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PeerId));
		Options.PeerHandle = (EOS_AntiCheatCommon_ClientHandle)PeerPUID;

		EOS_AntiCheatClient_UnregisterPeer(Handle, &Options);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatClient: UnregisterPeer %s"), *PeerId);
	}
#endif
}

// ── Queries ─────────────────────────────────────────────────────────────────

bool UEEOSAntiCheatClientSubsystem::IsSessionActive() const
{
	return bSessionActive;
}
