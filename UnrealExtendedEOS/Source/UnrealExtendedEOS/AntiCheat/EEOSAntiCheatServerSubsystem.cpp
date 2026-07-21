// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSAntiCheatServerSubsystem.h"
#include "UnrealExtendedEOS.h"

// DEPRECATED SHIM — see the class comment in the header. Every method logs an
// error pointing at UEEOSAntiCheatSubsystem, performs no SDK call and mutates
// no state (fail-closed). The broken Atoi64 handle bookkeeping and fail-open
// session plumbing that used to live here has been removed so this class can
// no longer half-start an anti-cheat session.

#define EEOS_AC_SERVER_SHIM_ERROR(FuncName) \
	UE_LOG(LogExtendedEOS, Error, TEXT("UEEOSAntiCheatServerSubsystem::" FuncName " — DEPRECATED shim, no-op. This game has no dedicated servers; use UEEOSAntiCheatSubsystem (peer-to-peer mode) instead."))

void UEEOSAntiCheatServerSubsystem::BeginSession()
{
	EEOS_AC_SERVER_SHIM_ERROR("BeginSession");
}

void UEEOSAntiCheatServerSubsystem::EndSession()
{
	EEOS_AC_SERVER_SHIM_ERROR("EndSession");
}

void UEEOSAntiCheatServerSubsystem::RegisterClient(const FString& ClientId)
{
	EEOS_AC_SERVER_SHIM_ERROR("RegisterClient");
}

void UEEOSAntiCheatServerSubsystem::UnregisterClient(const FString& ClientId)
{
	EEOS_AC_SERVER_SHIM_ERROR("UnregisterClient");
}

void UEEOSAntiCheatServerSubsystem::UnregisterAllClients()
{
	EEOS_AC_SERVER_SHIM_ERROR("UnregisterAllClients");
}

void UEEOSAntiCheatServerSubsystem::ReceiveMessageFromClient(const FString& ClientId, const TArray<uint8>& MessageData)
{
	EEOS_AC_SERVER_SHIM_ERROR("ReceiveMessageFromClient");
}

void UEEOSAntiCheatServerSubsystem::SetGameSessionId(const FString& SessionId)
{
	EEOS_AC_SERVER_SHIM_ERROR("SetGameSessionId");
}

void UEEOSAntiCheatServerSubsystem::LogPlayerSpawn(const FString& ClientId)
{
	EEOS_AC_SERVER_SHIM_ERROR("LogPlayerSpawn");
}

void UEEOSAntiCheatServerSubsystem::LogPlayerDespawn(const FString& ClientId)
{
	EEOS_AC_SERVER_SHIM_ERROR("LogPlayerDespawn");
}

bool UEEOSAntiCheatServerSubsystem::IsSessionActive() const
{
	// Fail-closed: this shim never has an active session.
	return false;
}

TArray<FString> UEEOSAntiCheatServerSubsystem::GetRegisteredClients() const
{
	return TArray<FString>();
}

int32 UEEOSAntiCheatServerSubsystem::GetRegisteredClientCount() const
{
	return 0;
}

#undef EEOS_AC_SERVER_SHIM_ERROR
