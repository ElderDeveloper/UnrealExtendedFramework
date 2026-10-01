// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#pragma once

#include "OnlineSessionSettings.h"

/** Holds one target across the asynchronous leave-then-join sequence. */
struct FEEOSLobbyJoinRequest
{
	enum class EPhase : uint8 { Idle, Leaving, Joining };

	bool Begin(const FOnlineSessionSearchResult& Target, bool bHasExistingSession)
	{
		if (IsActive()) return false;
		Result = Target;
		Phase = bHasExistingSession ? EPhase::Leaving : EPhase::Joining;
		return true;
	}

	bool CompleteLeave(bool bSuccess, bool bSessionStillExists)
	{
		if (!IsLeaving()) return false;
		// Keep the target for the caller's terminal error log; Reset ends the request.
		if (!bSuccess || bSessionStillExists) return false;
		Phase = EPhase::Joining;
		return true;
	}

	void Reset()
	{
		Phase = EPhase::Idle;
		Result = FOnlineSessionSearchResult();
	}

	bool IsActive() const { return Phase != EPhase::Idle; }
	bool IsLeaving() const { return Phase == EPhase::Leaving; }
	bool IsJoining() const { return Phase == EPhase::Joining; }
	const FOnlineSessionSearchResult& GetResult() const { return Result; }

private:
	EPhase Phase = EPhase::Idle;
	FOnlineSessionSearchResult Result;
};
