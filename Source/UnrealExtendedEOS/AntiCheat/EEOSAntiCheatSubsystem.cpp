// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSAntiCheatSubsystem.h"
#include "Shared/EEOSSettings.h"
#include "UnrealExtendedEOS.h"
#include "OnlineSubsystemUtils.h"

#include "eos_anticheatclient.h"
#include "eos_anticheatclient_types.h"
#include "eos_sdk.h"

void UEEOSAntiCheatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UEEOSSettings* Settings = GetEOSSettings();
	if (Settings && !Settings->bEnableAntiCheat)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatSubsystem: Anti-cheat is disabled in settings"));
		return;
	}

	// Register client integrity violation notification
	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (PlatformHandle)
	{
		EOS_HAntiCheatClient ACHandle = EOS_Platform_GetAntiCheatClientInterface(PlatformHandle);
		if (ACHandle)
		{
			// Integrity violation callback — fires when the local client has a file integrity issue
			EOS_AntiCheatClient_AddNotifyClientIntegrityViolatedOptions IntegrityOptions = {};
			IntegrityOptions.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYCLIENTINTEGRITYVIOLATED_API_LATEST;

			IntegrityViolatedNotifId = EOS_AntiCheatClient_AddNotifyClientIntegrityViolated(ACHandle, &IntegrityOptions, this,
				[](const EOS_AntiCheatClient_OnClientIntegrityViolatedCallbackInfo* Data)
				{
					UEEOSAntiCheatSubsystem* Self = static_cast<UEEOSAntiCheatSubsystem*>(Data->ClientData);
					if (!Self) return;

					FString ViolationMsg = ANSI_TO_TCHAR(Data->ViolationMessage);
					UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem: CLIENT INTEGRITY VIOLATED — %s"), *ViolationMsg);
					Self->OnIntegrityChanged.Broadcast(false);
					Self->OnClientActionRequired.Broadcast(EEOSAntiCheatAction::RemovePlayer, ViolationMsg);
				});

			// Peer auth status changed callback — fires in P2P mode when a peer's auth changes
			EOS_AntiCheatClient_AddNotifyPeerAuthStatusChangedOptions PeerAuthOptions = {};
			PeerAuthOptions.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYPEERAUTHSTATUSCHANGED_API_LATEST;

			PeerAuthStatusNotifId = EOS_AntiCheatClient_AddNotifyPeerAuthStatusChanged(ACHandle, &PeerAuthOptions, this,
				[](const EOS_AntiCheatCommon_OnClientAuthStatusChangedCallbackInfo* Data)
				{
					UEEOSAntiCheatSubsystem* Self = static_cast<UEEOSAntiCheatSubsystem*>(Data->ClientData);
					if (!Self) return;

					// Convert client handle to a string peer ID for our subsystem's API
					FString PeerId = FString::Printf(TEXT("Peer_%lld"), reinterpret_cast<int64>(Data->ClientHandle));
					bool bAuthenticated = (Data->ClientAuthStatus == EOS_EAntiCheatCommonClientAuthStatus::EOS_ACCCAS_RemoteAuthComplete);

					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatSubsystem: Peer '%s' auth status changed — %s"),
						*PeerId, bAuthenticated ? TEXT("Authenticated") : TEXT("Not Authenticated"));
					Self->OnPeerAuthStatusChanged.Broadcast(PeerId, bAuthenticated);
				});

			// Peer action required callback — fires when an action should be taken against a peer
			EOS_AntiCheatClient_AddNotifyPeerActionRequiredOptions PeerActionOptions = {};
			PeerActionOptions.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYPEERACTIONREQUIRED_API_LATEST;

			PeerActionRequiredNotifId = EOS_AntiCheatClient_AddNotifyPeerActionRequired(ACHandle, &PeerActionOptions, this,
				[](const EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo* Data)
				{
					UEEOSAntiCheatSubsystem* Self = static_cast<UEEOSAntiCheatSubsystem*>(Data->ClientData);
					if (!Self) return;

					FString PeerId = FString::Printf(TEXT("Peer_%lld"), reinterpret_cast<int64>(Data->ClientHandle));
					FString ActionMsg = ANSI_TO_TCHAR(Data->ActionReasonDetailsString);

					EEOSAntiCheatAction Action = EEOSAntiCheatAction::None;
					switch (Data->ClientAction)
					{
					case EOS_EAntiCheatCommonClientAction::EOS_ACCCA_RemovePlayer:
						Action = EEOSAntiCheatAction::RemovePlayer;
						break;
					default:
						Action = EEOSAntiCheatAction::Invalid;
						break;
					}

					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem: Action required for peer '%s' — %s: %s"),
						*PeerId, *UEnum::GetValueAsString(Action), *ActionMsg);
					Self->OnClientActionRequired.Broadcast(Action, ActionMsg);
				});

			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatSubsystem: Registered client-side anti-cheat notifications"));
		}
		else
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem: AntiCheatClient interface not available. "
				"Make sure the EOS Anti-Cheat module is loaded and configured."));
		}
	}
}

void UEEOSAntiCheatSubsystem::Deinitialize()
{
	if (bSessionActive)
	{
		EndSession();
	}

	// Clean up notification callbacks
	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (PlatformHandle)
	{
		EOS_HAntiCheatClient ACHandle = EOS_Platform_GetAntiCheatClientInterface(PlatformHandle);
		if (ACHandle)
		{
			if (IntegrityViolatedNotifId != EOS_INVALID_NOTIFICATIONID)
			{
				EOS_AntiCheatClient_RemoveNotifyClientIntegrityViolated(ACHandle, IntegrityViolatedNotifId);
			}
			if (PeerAuthStatusNotifId != EOS_INVALID_NOTIFICATIONID)
			{
				EOS_AntiCheatClient_RemoveNotifyPeerAuthStatusChanged(ACHandle, PeerAuthStatusNotifId);
			}
			if (PeerActionRequiredNotifId != EOS_INVALID_NOTIFICATIONID)
			{
				EOS_AntiCheatClient_RemoveNotifyPeerActionRequired(ACHandle, PeerActionRequiredNotifId);
			}
		}
	}

	RegisteredPeers.Empty();
	PeerHandleMap.Empty();
	Super::Deinitialize();
}

// ── Session Management ───────────────────────────────────────────────────────

void UEEOSAntiCheatSubsystem::BeginSession()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("BeginSession"));
		return;
	}

	if (bSessionActive)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::BeginSession — Session already active"));
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::BeginSession — Platform handle not available"));
		return;
	}

	EOS_HAntiCheatClient ACHandle = EOS_Platform_GetAntiCheatClientInterface(PlatformHandle);
	if (!ACHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::BeginSession — AntiCheatClient interface not available"));
		return;
	}

	// Get the logged-in Product User ID
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	FUniqueNetIdPtr LocalPlayerId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LocalPlayerId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::BeginSession — No logged-in user (must login via Connect first)"));
		return;
	}

	FString UserIdStr = LocalPlayerId->ToString();
	EOS_ProductUserId LocalPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*UserIdStr));
	if (!EOS_ProductUserId_IsValid(LocalPUID))
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::BeginSession — Invalid Product User ID"));
		return;
	}

	EOS_AntiCheatClient_BeginSessionOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATCLIENT_BEGINSESSION_API_LATEST;
	Options.LocalUserId = LocalPUID;
	Options.Mode = EOS_EAntiCheatClientMode::EOS_ACCM_PeerToPeer; // Default to P2P mode — use ClientServer for dedicated servers

	EOS_EResult Result = EOS_AntiCheatClient_BeginSession(ACHandle, &Options);
	if (Result == EOS_EResult::EOS_Success)
	{
		bSessionActive = true;
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatSubsystem::BeginSession — Anti-cheat session started (P2P mode)"));
	}
	else
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::BeginSession — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
	}
}

void UEEOSAntiCheatSubsystem::EndSession()
{
	if (!bSessionActive)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::EndSession — No active session"));
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (PlatformHandle)
	{
		EOS_HAntiCheatClient ACHandle = EOS_Platform_GetAntiCheatClientInterface(PlatformHandle);
		if (ACHandle)
		{
			EOS_AntiCheatClient_EndSessionOptions Options = {};
			Options.ApiVersion = EOS_ANTICHEATCLIENT_ENDSESSION_API_LATEST;

			EOS_EResult Result = EOS_AntiCheatClient_EndSession(ACHandle, &Options);
			if (Result == EOS_EResult::EOS_Success)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatSubsystem::EndSession — Anti-cheat session ended"));
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::EndSession — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
			}
		}
	}

	bSessionActive = false;
	RegisteredPeers.Empty();
	PeerHandleMap.Empty();
}

// ── Peer Management ──────────────────────────────────────────────────────────

void UEEOSAntiCheatSubsystem::RegisterPeer(const FString& PeerId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("RegisterPeer"));
		return;
	}

	if (!bSessionActive)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::RegisterPeer — No active session"));
		return;
	}

	if (RegisteredPeers.Contains(PeerId))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::RegisterPeer — Peer '%s' already registered"), *PeerId);
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::RegisterPeer — Platform handle not available"));
		return;
	}

	EOS_HAntiCheatClient ACHandle = EOS_Platform_GetAntiCheatClientInterface(PlatformHandle);
	if (!ACHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::RegisterPeer — AntiCheatClient interface not available"));
		return;
	}

	// Convert the PeerId to an EOS Product User ID
	EOS_ProductUserId PeerPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*PeerId));

	// Create a unique client handle for this peer — use a running counter cast to the handle type
	NextPeerHandleValue++;
	EOS_AntiCheatCommon_ClientHandle PeerHandle = reinterpret_cast<EOS_AntiCheatCommon_ClientHandle>(static_cast<intptr_t>(NextPeerHandleValue));

	EOS_AntiCheatClient_RegisterPeerOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATCLIENT_REGISTERPEER_API_LATEST;
	Options.PeerHandle = PeerHandle;
	Options.ClientType = EOS_EAntiCheatCommonClientType::EOS_ACCCT_ProtectedClient;
	Options.ClientPlatform = EOS_EAntiCheatCommonClientPlatform::EOS_ACCCP_Unknown;
	Options.AuthenticationTimeout = EOS_ANTICHEATCLIENT_REGISTERPEER_MIN_AUTHENTICATIONTIMEOUT; // 40 seconds
	Options.AccountId_DEPRECATED = nullptr;
	Options.IpAddress = nullptr;
	Options.PeerProductUserId = PeerPUID;

	EOS_EResult Result = EOS_AntiCheatClient_RegisterPeer(ACHandle, &Options);
	if (Result == EOS_EResult::EOS_Success)
	{
		RegisteredPeers.Add(PeerId);
		PeerHandleMap.Add(PeerId, NextPeerHandleValue);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatSubsystem::RegisterPeer — Registered '%s' via EOS SDK (%d total)"), *PeerId, RegisteredPeers.Num());
		// Note: OnPeerAuthStatusChanged will fire from the SDK notification when auth actually completes
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::RegisterPeer — Failed for '%s': %s"), *PeerId, ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
	}
}

void UEEOSAntiCheatSubsystem::UnregisterPeer(const FString& PeerId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("UnregisterPeer"));
		return;
	}

	if (!RegisteredPeers.Contains(PeerId))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::UnregisterPeer — Peer '%s' not registered"), *PeerId);
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (PlatformHandle)
	{
		EOS_HAntiCheatClient ACHandle = EOS_Platform_GetAntiCheatClientInterface(PlatformHandle);
		if (ACHandle)
		{
			int64* HandleValuePtr = PeerHandleMap.Find(PeerId);
			if (HandleValuePtr)
			{
				EOS_AntiCheatCommon_ClientHandle PeerHandle = reinterpret_cast<EOS_AntiCheatCommon_ClientHandle>(static_cast<intptr_t>(*HandleValuePtr));

				EOS_AntiCheatClient_UnregisterPeerOptions Options = {};
				Options.ApiVersion = EOS_ANTICHEATCLIENT_UNREGISTERPEER_API_LATEST;
				Options.PeerHandle = PeerHandle;

				EOS_EResult Result = EOS_AntiCheatClient_UnregisterPeer(ACHandle, &Options);
				if (Result == EOS_EResult::EOS_Success)
				{
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatSubsystem::UnregisterPeer — Unregistered '%s' via EOS SDK"), *PeerId);
				}
				else
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::UnregisterPeer — Failed for '%s': %s"), *PeerId, ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
				}
			}
		}
	}

	RegisteredPeers.Remove(PeerId);
	PeerHandleMap.Remove(PeerId);
	OnPeerAuthStatusChanged.Broadcast(PeerId, false);
}

void UEEOSAntiCheatSubsystem::UnregisterAllPeers()
{
	TArray<FString> PeersCopy = RegisteredPeers;
	for (const FString& PeerId : PeersCopy)
	{
		UnregisterPeer(PeerId);
	}
}

// ── Player Action Reporting ──────────────────────────────────────────────────

void UEEOSAntiCheatSubsystem::ReportPlayerAction(const FString& PlayerId, const FString& ActionType, const FString& ActionData)
{
	if (!bSessionActive)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::ReportPlayerAction — No active session"));
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::ReportPlayerAction — Platform handle not available"));
		return;
	}

	EOS_HAntiCheatClient ACHandle = EOS_Platform_GetAntiCheatClientInterface(PlatformHandle);
	if (!ACHandle)
	{
		// No anti-cheat client available — log locally for diagnostics only
		UE_LOG(LogExtendedEOS, Verbose, TEXT("EEOSAntiCheatSubsystem::ReportPlayerAction — AntiCheatClient unavailable, logging locally: Player='%s' Action='%s' Data='%s'"),
			*PlayerId, *ActionType, *ActionData);
		return;
	}

	// NOTE: EOS Anti-Cheat does not have a per-action "ReportPlayerAction" API.
	// Game-specific action validation (speed checks, damage limits, etc.) must be
	// done server-side. This function broadcasts the action locally so that game
	// code can react (e.g., a server subsystem can aggregate and validate actions).
	UE_LOG(LogExtendedEOS, Verbose, TEXT("EEOSAntiCheatSubsystem::ReportPlayerAction — Player='%s' Action='%s' Data='%s'"),
		*PlayerId, *ActionType, *ActionData);

	// Broadcast locally so server-side game code can validate
	OnPlayerActionReported.Broadcast(PlayerId, ActionType, ActionData);
}

void UEEOSAntiCheatSubsystem::ReportViolation(const FString& PlayerId, EEOSAntiCheatViolationType ViolationType, const FString& Details)
{
	if (!bSessionActive)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::ReportViolation — No active session"));
		return;
	}

	// EOS anti-cheat detects violations automatically through its client-side module.
	// This method exists for game-specific violation reporting (e.g., server-side speed checks).
	// The violation is broadcast to listeners so the game can take action (kick, ban, etc.)
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::ReportViolation — Player='%s' Type=%d Details='%s'"),
		*PlayerId, static_cast<int32>(ViolationType), *Details);

	OnViolationDetected.Broadcast(PlayerId, ViolationType, Details);
}

void UEEOSAntiCheatSubsystem::RequestIntegrityCheck(const FString& PlayerId)
{
	if (!bSessionActive)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::RequestIntegrityCheck — No active session"));
		return;
	}

	// IMPORTANT: EOS_AntiCheatClient_PollStatus checks LOCAL client integrity only.
	// Remote player integrity checks require EOS_AntiCheatServer on a dedicated/listen server.
	// The PlayerId parameter is accepted for API consistency but this function always checks
	// the local client's anti-cheat status.
	if (!PlayerId.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::RequestIntegrityCheck — "
			TEXT("NOTE: This checks LOCAL client integrity only. PlayerId '%s' is logged for context "
			"but cannot be remotely verified from the client. Use EOS_AntiCheatServer for remote checks.")), *PlayerId);
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::RequestIntegrityCheck — Platform handle not available"));
		OnIntegrityChanged.Broadcast(false);
		return;
	}

	EOS_HAntiCheatClient ACHandle = EOS_Platform_GetAntiCheatClientInterface(PlatformHandle);
	if (!ACHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::RequestIntegrityCheck — AntiCheatClient interface not available"));
		OnIntegrityChanged.Broadcast(false);
		return;
	}

	// Poll local client anti-cheat status
	EOS_AntiCheatClient_PollStatusOptions PollOptions = {};
	PollOptions.ApiVersion = 1;

	EOS_EAntiCheatClientViolationType OutViolationType = EOS_EAntiCheatClientViolationType::EOS_ACCVT_Invalid;
	char OutMessage[256] = {};
	PollOptions.OutMessageLength = sizeof(OutMessage);

	EOS_EResult Result = EOS_AntiCheatClient_PollStatus(ACHandle, &PollOptions, &OutViolationType, OutMessage);
	if (Result == EOS_EResult::EOS_NotFound)
	{
		// No violation found — local integrity is valid
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatSubsystem::RequestIntegrityCheck — Local client integrity OK"));
		OnIntegrityChanged.Broadcast(true);
	}
	else if (Result == EOS_EResult::EOS_Success)
	{
		// Violation found on local client
		FString ViolationMsg = ANSI_TO_TCHAR(OutMessage);
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::RequestIntegrityCheck — LOCAL VIOLATION detected: %s"), *ViolationMsg);
		OnIntegrityChanged.Broadcast(false);
		OnViolationDetected.Broadcast(PlayerId, EEOSAntiCheatViolationType::CustomViolation, ViolationMsg);
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::RequestIntegrityCheck — PollStatus failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		OnIntegrityChanged.Broadcast(false);
	}
}

// ── Queries ──────────────────────────────────────────────────────────────────

bool UEEOSAntiCheatSubsystem::IsSessionActive() const
{
	return bSessionActive;
}

TArray<FString> UEEOSAntiCheatSubsystem::GetRegisteredPeers() const
{
	return RegisteredPeers;
}

int32 UEEOSAntiCheatSubsystem::GetRegisteredPeerCount() const
{
	return RegisteredPeers.Num();
}

bool UEEOSAntiCheatSubsystem::IsPeerRegistered(const FString& PeerId) const
{
	return RegisteredPeers.Contains(PeerId);
}
