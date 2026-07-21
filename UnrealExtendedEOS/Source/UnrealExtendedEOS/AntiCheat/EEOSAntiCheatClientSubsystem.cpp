// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSAntiCheatClientSubsystem.h"
#include "UnrealExtendedEOS.h"

// DEPRECATED SHIM — see the class comment in the header. Every method logs an
// error pointing at UEEOSAntiCheatSubsystem, performs no SDK call and mutates
// no state (fail-closed). The broken ClientServer/P2P mixed-mode plumbing that
// used to live here has been removed so this class can no longer half-start an
// anti-cheat session.

#define EEOS_AC_CLIENT_SHIM_ERROR(FuncName) \
	UE_LOG(LogExtendedEOS, Error, TEXT("UEEOSAntiCheatClientSubsystem::" FuncName " — DEPRECATED shim, no-op. Use UEEOSAntiCheatSubsystem (peer-to-peer mode) instead."))

void UEEOSAntiCheatClientSubsystem::BeginSession()
{
	EEOS_AC_CLIENT_SHIM_ERROR("BeginSession");
}

void UEEOSAntiCheatClientSubsystem::EndSession()
{
	EEOS_AC_CLIENT_SHIM_ERROR("EndSession");
}

void UEEOSAntiCheatClientSubsystem::ReceiveMessageFromServer(const TArray<uint8>& MessageData)
{
	EEOS_AC_CLIENT_SHIM_ERROR("ReceiveMessageFromServer");
}

void UEEOSAntiCheatClientSubsystem::ReceiveMessageFromPeer(const FString& PeerId, const TArray<uint8>& MessageData)
{
	EEOS_AC_CLIENT_SHIM_ERROR("ReceiveMessageFromPeer");
}

void UEEOSAntiCheatClientSubsystem::RegisterPeer(const FString& PeerId)
{
	EEOS_AC_CLIENT_SHIM_ERROR("RegisterPeer");
}

void UEEOSAntiCheatClientSubsystem::UnregisterPeer(const FString& PeerId)
{
	EEOS_AC_CLIENT_SHIM_ERROR("UnregisterPeer");
}

bool UEEOSAntiCheatClientSubsystem::IsSessionActive() const
{
	// Fail-closed: this shim never has an active session.
	return false;
}

#undef EEOS_AC_CLIENT_SHIM_ERROR
