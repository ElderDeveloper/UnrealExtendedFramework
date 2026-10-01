// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

/** Correlates backend deletion with Unreal's independent local-session teardown. */
struct FEEOSLobbyExitRequest
{
	enum class EContinuation : uint8 { None, Create, Join };
	bool Begin(const FString& InLobbyId, bool bDeleteBackend, EContinuation InContinuation)
	{
	 if (bActive || InLobbyId.IsEmpty()) return false;
	 ++Token;
	 LobbyId = InLobbyId;
	 Continuation = InContinuation;
	 bActive = true;
	 bBackendPending = bDeleteBackend;
		bDeleteRequested = bDeleteBackend;
	 bBackendDeleted = false;
	 return true;
	}
	bool CompleteBackend(uint64 InToken, const FString& InLobbyId, bool bDeleted)
	{
	 if (!Matches(InToken, InLobbyId) || !bBackendPending) return false;
	 bBackendPending = false;
	 bBackendDeleted = bDeleted;
	 return true;
	}
	bool Matches(uint64 InToken, const FString& InLobbyId) const { return bActive && Token == InToken && LobbyId == InLobbyId; }
	bool Succeeded(bool bNativeSuccess, bool bNativeSessionRemains) const
	{
	 return bActive && !bBackendPending && !bNativeSessionRemains && (bDeleteRequested ? bBackendDeleted : bNativeSuccess);
	}
	void Reset() { bActive = false; bBackendPending = false; bBackendDeleted = false; bDeleteRequested = false; LobbyId.Empty(); Continuation = EContinuation::None; }
	bool IsActive() const { return bActive; }
	bool IsBackendPending() const { return bBackendPending; }
	bool WasBackendDeleted() const { return bBackendDeleted; }
	uint64 GetToken() const { return Token; }
	const FString& GetLobbyId() const { return LobbyId; }
	EContinuation GetContinuation() const { return Continuation; }
private:
	uint64 Token = 0;
	FString LobbyId;
	EContinuation Continuation = EContinuation::None;
	bool bActive = false;
	bool bBackendPending = false;
	bool bBackendDeleted = false;
	bool bDeleteRequested = false;
};
