// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSAntiCheatServerSubsystem.h"
#include "UnrealExtendedEOS.h"

#if WITH_EOS_SDK
#include "IEOSSDKManager.h"
#include "eos_anticheatserver.h"
#include "eos_sdk.h"

// ── Static helper ───────────────────────────────────────────────────────────

static EOS_HAntiCheatServer GetAntiCheatServerHandle()
{
	IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
	if (!SDKManager) return nullptr;

	TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
	if (Platforms.Num() == 0) return nullptr;

	EOS_HPlatform PlatformHandle = *Platforms[0];
	return EOS_Platform_GetAntiCheatServerInterface(PlatformHandle);
}

// ── Static EOS_CALL callbacks ───────────────────────────────────────────────

static void EOS_CALL OnACServerMessageToClient(const EOS_AntiCheatCommon_OnMessageToClientCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	UEEOSAntiCheatServerSubsystem* Self = static_cast<UEEOSAntiCheatServerSubsystem*>(Data->ClientData);

	TArray<uint8> MsgData;
	MsgData.Append(static_cast<const uint8*>(Data->MessageData), Data->MessageDataSizeBytes);

	FString ClientId = FString::Printf(TEXT("%llu"), (uint64)Data->ClientHandle);

	AsyncTask(ENamedThreads::GameThread, [Self, ClientId, MsgData]()
	{
		Self->OnMessageToClient.Broadcast(ClientId, MsgData);
	});
}

static void EOS_CALL OnACServerClientActionRequired(const EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	UEEOSAntiCheatServerSubsystem* Self = static_cast<UEEOSAntiCheatServerSubsystem*>(Data->ClientData);

	FString ClientId = FString::Printf(TEXT("%llu"), (uint64)Data->ClientHandle);
	EEOSAntiCheatAction Action = EEOSAntiCheatAction::None;
	if (Data->ClientAction == EOS_EAntiCheatCommonClientAction::EOS_ACCCA_RemovePlayer)
	{
		Action = EEOSAntiCheatAction::RemovePlayer;
	}

	AsyncTask(ENamedThreads::GameThread, [Self, ClientId, Action]()
	{
		Self->OnClientActionRequired.Broadcast(ClientId, Action);
	});
}

static void EOS_CALL OnACServerClientAuthChanged(const EOS_AntiCheatCommon_OnClientAuthStatusChangedCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	UEEOSAntiCheatServerSubsystem* Self = static_cast<UEEOSAntiCheatServerSubsystem*>(Data->ClientData);

	FString ClientId = FString::Printf(TEXT("%llu"), (uint64)Data->ClientHandle);
	bool bAuth = (Data->ClientAuthStatus == EOS_EAntiCheatCommonClientAuthStatus::EOS_ACCCAS_RemoteAuthComplete);

	AsyncTask(ENamedThreads::GameThread, [Self, ClientId, bAuth]()
	{
		Self->OnClientAuthStatusChanged.Broadcast(ClientId, bAuth);
	});
}

#endif // WITH_EOS_SDK

// ── Initialize / Deinitialize ───────────────────────────────────────────────

void UEEOSAntiCheatServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSAntiCheatServerSubsystem::Deinitialize()
{
	if (bSessionActive)
	{
		EndSession();
	}
	RegisteredClients.Empty();
	Super::Deinitialize();
}

// ── Session Management ──────────────────────────────────────────────────────

void UEEOSAntiCheatServerSubsystem::BeginSession()
{
	if (bSessionActive)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatServer: Session already active"));
		return;
	}

#if WITH_EOS_SDK
	EOS_HAntiCheatServer Handle = GetAntiCheatServerHandle();
	if (Handle)
	{
		// Register required notification callbacks
		EOS_AntiCheatServer_AddNotifyMessageToClientOptions MsgOpts = {};
		MsgOpts.ApiVersion = EOS_ANTICHEATSERVER_ADDNOTIFYMESSAGETOCLIENT_API_LATEST;
		NotifyMsgToClientId = EOS_AntiCheatServer_AddNotifyMessageToClient(Handle, &MsgOpts, this, &OnACServerMessageToClient);

		EOS_AntiCheatServer_AddNotifyClientActionRequiredOptions ActionOpts = {};
		ActionOpts.ApiVersion = EOS_ANTICHEATSERVER_ADDNOTIFYCLIENTACTIONREQUIRED_API_LATEST;
		NotifyClientActionId = EOS_AntiCheatServer_AddNotifyClientActionRequired(Handle, &ActionOpts, this, &OnACServerClientActionRequired);

		EOS_AntiCheatServer_AddNotifyClientAuthStatusChangedOptions AuthOpts = {};
		AuthOpts.ApiVersion = EOS_ANTICHEATSERVER_ADDNOTIFYCLIENTAUTHSTATUSCHANGED_API_LATEST;
		NotifyClientAuthId = EOS_AntiCheatServer_AddNotifyClientAuthStatusChanged(Handle, &AuthOpts, this, &OnACServerClientAuthChanged);

		EOS_AntiCheatServer_BeginSessionOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATSERVER_BEGINSESSION_API_LATEST;
		Options.RegisterTimeoutSeconds = 60;
		Options.bEnableGameplayData = EOS_TRUE;
		Options.ServerName = "UnrealExtendedEOS";
		Options.LocalUserId = nullptr;

		EOS_EResult Result = EOS_AntiCheatServer_BeginSession(Handle, &Options);
		if (Result == EOS_EResult::EOS_Success)
		{
			bSessionActive = true;
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatServer: Session started (SDK)"));
			return;
		}
		else
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatServer: BeginSession failed — %s"),
				ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		}
	}
#endif

	bSessionActive = true;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatServer: Session started (local fallback)"));
}

void UEEOSAntiCheatServerSubsystem::EndSession()
{
	if (!bSessionActive) return;

#if WITH_EOS_SDK
	EOS_HAntiCheatServer Handle = GetAntiCheatServerHandle();
	if (Handle)
	{
		EOS_AntiCheatServer_EndSessionOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATSERVER_ENDSESSION_API_LATEST;
		EOS_AntiCheatServer_EndSession(Handle, &Options);

		if (NotifyMsgToClientId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_AntiCheatServer_RemoveNotifyMessageToClient(Handle, NotifyMsgToClientId);
			NotifyMsgToClientId = EOS_INVALID_NOTIFICATIONID;
		}
		if (NotifyClientActionId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_AntiCheatServer_RemoveNotifyClientActionRequired(Handle, NotifyClientActionId);
			NotifyClientActionId = EOS_INVALID_NOTIFICATIONID;
		}
		if (NotifyClientAuthId != EOS_INVALID_NOTIFICATIONID)
		{
			EOS_AntiCheatServer_RemoveNotifyClientAuthStatusChanged(Handle, NotifyClientAuthId);
			NotifyClientAuthId = EOS_INVALID_NOTIFICATIONID;
		}
	}
#endif

	bSessionActive = false;
	RegisteredClients.Empty();
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatServer: Session ended"));
}

// ── Client Management ───────────────────────────────────────────────────────

void UEEOSAntiCheatServerSubsystem::RegisterClient(const FString& ClientId)
{
	if (!bSessionActive) return;

	RegisteredClients.AddUnique(ClientId);

#if WITH_EOS_SDK
	EOS_HAntiCheatServer Handle = GetAntiCheatServerHandle();
	if (Handle)
	{
		EOS_AntiCheatServer_RegisterClientOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATSERVER_REGISTERCLIENT_API_LATEST;
		Options.ClientHandle = (EOS_AntiCheatCommon_ClientHandle)(uint64)FCString::Atoi64(*ClientId);
		Options.ClientType = EOS_EAntiCheatCommonClientType::EOS_ACCCT_ProtectedClient;
		Options.ClientPlatform = EOS_EAntiCheatCommonClientPlatform::EOS_ACCCP_Windows;

		EOS_ProductUserId ClientPUID = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*ClientId));
		Options.UserId = ClientPUID;

		EOS_EResult Result = EOS_AntiCheatServer_RegisterClient(Handle, &Options);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatServer: RegisterClient %s — %s"),
			*ClientId, ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
	}
#endif
}

void UEEOSAntiCheatServerSubsystem::UnregisterClient(const FString& ClientId)
{
	RegisteredClients.Remove(ClientId);

#if WITH_EOS_SDK
	EOS_HAntiCheatServer Handle = GetAntiCheatServerHandle();
	if (Handle)
	{
		EOS_AntiCheatServer_UnregisterClientOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATSERVER_UNREGISTERCLIENT_API_LATEST;
		Options.ClientHandle = (EOS_AntiCheatCommon_ClientHandle)(uint64)FCString::Atoi64(*ClientId);

		EOS_AntiCheatServer_UnregisterClient(Handle, &Options);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatServer: UnregisterClient %s"), *ClientId);
	}
#endif
}

void UEEOSAntiCheatServerSubsystem::UnregisterAllClients()
{
	TArray<FString> ClientsCopy = RegisteredClients;
	for (const FString& ClientId : ClientsCopy)
	{
		UnregisterClient(ClientId);
	}
}

// ── Message Passing ─────────────────────────────────────────────────────────

void UEEOSAntiCheatServerSubsystem::ReceiveMessageFromClient(const FString& ClientId, const TArray<uint8>& MessageData)
{
	if (!bSessionActive) return;

#if WITH_EOS_SDK
	EOS_HAntiCheatServer Handle = GetAntiCheatServerHandle();
	if (Handle)
	{
		EOS_AntiCheatServer_ReceiveMessageFromClientOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATSERVER_RECEIVEMESSAGEFROMCLIENT_API_LATEST;
		Options.ClientHandle = (EOS_AntiCheatCommon_ClientHandle)(uint64)FCString::Atoi64(*ClientId);
		Options.Data = MessageData.GetData();
		Options.DataLengthBytes = MessageData.Num();

		EOS_AntiCheatServer_ReceiveMessageFromClient(Handle, &Options);
	}
#endif
}

// ── Cerberus Gameplay Events ────────────────────────────────────────────────

void UEEOSAntiCheatServerSubsystem::SetGameSessionId(const FString& SessionId)
{
	if (!bSessionActive) return;

#if WITH_EOS_SDK
	EOS_HAntiCheatServer Handle = GetAntiCheatServerHandle();
	if (Handle)
	{
		EOS_AntiCheatCommon_SetGameSessionIdOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCOMMON_SETGAMESESSIONID_API_LATEST;
		FTCHARToUTF8 SessionIdUtf8(*SessionId);
		Options.GameSessionId = SessionIdUtf8.Get();

		EOS_AntiCheatServer_SetGameSessionId(Handle, &Options);
	}
#endif

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatServer: GameSessionId set to '%s'"), *SessionId);
}

void UEEOSAntiCheatServerSubsystem::LogPlayerSpawn(const FString& ClientId)
{
	if (!bSessionActive) return;

#if WITH_EOS_SDK
	EOS_HAntiCheatServer Handle = GetAntiCheatServerHandle();
	if (Handle)
	{
		EOS_AntiCheatCommon_LogPlayerSpawnOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCOMMON_LOGPLAYERSPAWN_API_LATEST;
		Options.SpawnedPlayerHandle = (EOS_AntiCheatCommon_ClientHandle)(uint64)FCString::Atoi64(*ClientId);
		Options.TeamId = 0;
		Options.CharacterId = 0;

		EOS_AntiCheatServer_LogPlayerSpawn(Handle, &Options);
	}
#endif
}

void UEEOSAntiCheatServerSubsystem::LogPlayerDespawn(const FString& ClientId)
{
	if (!bSessionActive) return;

#if WITH_EOS_SDK
	EOS_HAntiCheatServer Handle = GetAntiCheatServerHandle();
	if (Handle)
	{
		EOS_AntiCheatCommon_LogPlayerDespawnOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCOMMON_LOGPLAYERDESPAWN_API_LATEST;
		Options.DespawnedPlayerHandle = (EOS_AntiCheatCommon_ClientHandle)(uint64)FCString::Atoi64(*ClientId);

		EOS_AntiCheatServer_LogPlayerDespawn(Handle, &Options);
	}
#endif
}

// ── Queries ─────────────────────────────────────────────────────────────────

bool UEEOSAntiCheatServerSubsystem::IsSessionActive() const
{
	return bSessionActive;
}

TArray<FString> UEEOSAntiCheatServerSubsystem::GetRegisteredClients() const
{
	return RegisteredClients;
}

int32 UEEOSAntiCheatServerSubsystem::GetRegisteredClientCount() const
{
	return RegisteredClients.Num();
}
