// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "GameServerStats/ESteamGameServerStatsSubsystem.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"
#include "Containers/Queue.h"

namespace
{
	void LogGameServerStatsUnavailable(const TCHAR* Context)
	{
		UE_LOG(LogExtendedSteam, Warning,
			TEXT("%s: Steam game server is not available (InitializeSteamGameServer has not succeeded)"), Context);
	}
}

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"
THIRD_PARTY_INCLUDES_END

/** Parameters captured to (re-)issue a queued RequestUserStats / StoreUserStats call. */
struct FESteamPendingGSUserStats
{
	FESteamId User;
};

/**
 * Native call-result trackers for the game server stats interface; alive only while the game
 * server API is initialized. GSStatsReceived_t / GSStatsStored_t call results originate on the
 * game server pipe and are dispatched by SteamGameServer_RunCallbacks, so both CCallResults are
 * flagged with SetGameserverFlag() (the flag persists across the re-issue Set() calls).
 *
 * Same-type async requests are serialized via a per-operation FIFO queue: while a given op's
 * CCallResult is in flight, further requests of that type are enqueued and issued in order as
 * each completion arrives, so none are dropped. Queued-but-unissued requests are abandoned
 * cleanly when the holder is destroyed on game server shutdown / Deinitialize.
 */
class FESteamGameServerStatsCallbacks
{
public:
	explicit FESteamGameServerStatsCallbacks(UESteamGameServerStatsSubsystem* InOwner)
		: Owner(InOwner)
		// GSStatsUnloaded_t is dispatched on the game server pipe, so the callback is registered
		// with the CCallback gameserver template form (bGameServer = true), matching the game
		// server flag the CCallResults above set.
		, StatsUnloadedCallback(this, &FESteamGameServerStatsCallbacks::HandleStatsUnloaded)
	{
		StatsReceivedResult.SetGameserverFlag();
		StatsStoredResult.SetGameserverFlag();
	}

	// Each Enqueue* issues the Steam call immediately when its operation is idle, otherwise it
	// queues the request. Returns true when the request was issued or queued; false only on the
	// immediate path when the Steam call could not be issued (preserves the public methods'
	// historical "return false when the request could not be issued" contract).

	bool EnqueueRequestUserStats(const FESteamPendingGSUserStats& Request)
	{
		if (bRequestUserStatsBusy)
		{
			RequestUserStatsQueue.Enqueue(Request);
			return true;
		}
		return IssueRequestUserStats(Request);
	}

	bool EnqueueStoreUserStats(const FESteamPendingGSUserStats& Request)
	{
		if (bStoreUserStatsBusy)
		{
			StoreUserStatsQueue.Enqueue(Request);
			return true;
		}
		return IssueStoreUserStats(Request);
	}

private:
	// ---- RequestUserStats (serialized) ----

	bool IssueRequestUserStats(const FESteamPendingGSUserStats& Request)
	{
		UESteamGameServerStatsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsGameServerStatsAvailable())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamGameServerStats()->RequestUserStats(CSteamID(Request.User.Value));
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		StatsReceivedResult.Set(Call, this, &FESteamGameServerStatsCallbacks::HandleStatsReceived);
		bRequestUserStatsBusy = true;
		return true;
	}

	void HandleStatsReceived(GSStatsReceived_t* Data, bool bIOFailure)
	{
		if (UESteamGameServerStatsSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnUserStatsReceived.Broadcast(bSuccess, FESteamId(Data->m_steamIDUser.ConvertToUint64()));
		}
		bRequestUserStatsBusy = false;
		DrainRequestUserStatsQueue();
	}

	void DrainRequestUserStatsQueue()
	{
		while (!bRequestUserStatsBusy)
		{
			FESteamPendingGSUserStats Request;
			if (!RequestUserStatsQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueRequestUserStats(Request))
			{
				// Game server went away while draining: fail this queued request instead of dropping it.
				if (UESteamGameServerStatsSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnUserStatsReceived.Broadcast(false, Request.User);
				}
			}
		}
	}

	// ---- StoreUserStats (serialized) ----

	bool IssueStoreUserStats(const FESteamPendingGSUserStats& Request)
	{
		UESteamGameServerStatsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsGameServerStatsAvailable())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamGameServerStats()->StoreUserStats(CSteamID(Request.User.Value));
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		StatsStoredResult.Set(Call, this, &FESteamGameServerStatsCallbacks::HandleStatsStored);
		bStoreUserStatsBusy = true;
		return true;
	}

	void HandleStatsStored(GSStatsStored_t* Data, bool bIOFailure)
	{
		if (UESteamGameServerStatsSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnUserStatsStored.Broadcast(bSuccess, FESteamId(Data->m_steamIDUser.ConvertToUint64()));
		}
		bStoreUserStatsBusy = false;
		DrainStoreUserStatsQueue();
	}

	void DrainStoreUserStatsQueue()
	{
		while (!bStoreUserStatsBusy)
		{
			FESteamPendingGSUserStats Request;
			if (!StoreUserStatsQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueStoreUserStats(Request))
			{
				if (UESteamGameServerStatsSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnUserStatsStored.Broadcast(false, Request.User);
				}
			}
		}
	}

	// ---- Continuous game server callback (not request/response) ----

	void HandleStatsUnloaded(GSStatsUnloaded_t* Data)
	{
		if (UESteamGameServerStatsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnStatsUnloaded.Broadcast(FESteamId(Data->m_steamIDUser.ConvertToUint64()));
		}
	}

	TWeakObjectPtr<UESteamGameServerStatsSubsystem> Owner;

	CCallResult<FESteamGameServerStatsCallbacks, GSStatsReceived_t> StatsReceivedResult;
	CCallResult<FESteamGameServerStatsCallbacks, GSStatsStored_t> StatsStoredResult;

	CCallback<FESteamGameServerStatsCallbacks, GSStatsUnloaded_t, true> StatsUnloadedCallback;

	// In-flight flags + FIFO queues, one per serialized operation type.
	bool bRequestUserStatsBusy = false;
	bool bStoreUserStatsBusy = false;

	TQueue<FESteamPendingGSUserStats> RequestUserStatsQueue;
	TQueue<FESteamPendingGSUserStats> StoreUserStatsQueue;
};
#else
class FESteamGameServerStatsCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamGameServerStatsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// The base class hooks (HandleSteamClientInitialized/Shutdown) follow the Steam CLIENT API;
	// the game server stats interface follows the game server lifecycle instead.
	if (FExtendedSteamSharedModule::IsModuleAvailable())
	{
		FExtendedSteamSharedModule& Shared = FExtendedSteamSharedModule::Get();
		GameServerInitializedHandle = Shared.OnSteamGameServerInitialized.AddUObject(this, &UESteamGameServerStatsSubsystem::HandleGameServerInitialized);
		GameServerShutdownHandle = Shared.OnSteamGameServerShutdown.AddUObject(this, &UESteamGameServerStatsSubsystem::HandleGameServerShutdown);

		if (Shared.IsSteamGameServerInitialized())
		{
			HandleGameServerInitialized();
		}
	}
}

void UESteamGameServerStatsSubsystem::Deinitialize()
{
	if (FExtendedSteamSharedModule::IsModuleAvailable())
	{
		FExtendedSteamSharedModule& Shared = FExtendedSteamSharedModule::Get();
		Shared.OnSteamGameServerInitialized.Remove(GameServerInitializedHandle);
		Shared.OnSteamGameServerShutdown.Remove(GameServerShutdownHandle);
	}

	Callbacks.Reset();
	Super::Deinitialize();
}

void UESteamGameServerStatsSubsystem::HandleGameServerInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamGameServerStatsCallbacks>(this);
	}
#endif
}

void UESteamGameServerStatsSubsystem::HandleGameServerShutdown()
{
	Callbacks.Reset();
}

bool UESteamGameServerStatsSubsystem::IsGameServerStatsAvailable() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return FExtendedSteamSharedModule::IsModuleAvailable()
		&& FExtendedSteamSharedModule::Get().IsSteamGameServerInitialized()
		&& SteamGameServerStats() != nullptr;
#else
	return false;
#endif
}

bool UESteamGameServerStatsSubsystem::RequestUserStats(FESteamId User)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsGameServerStatsAvailable() || !Callbacks)
	{
		LogGameServerStatsUnavailable(TEXT("RequestUserStats"));
		return false;
	}

	FESteamPendingGSUserStats Request;
	Request.User = User;
	return Callbacks->EnqueueRequestUserStats(Request);
#else
	LogGameServerStatsUnavailable(TEXT("RequestUserStats"));
	return false;
#endif
}

bool UESteamGameServerStatsSubsystem::GetUserStatInt(FESteamId User, const FString& StatName, int32& OutValue) const
{
	OutValue = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerStatsAvailable())
	{
		return SteamGameServerStats()->GetUserStat(CSteamID(User.Value), TCHAR_TO_UTF8(*StatName), &OutValue);
	}
#endif
	return false;
}

bool UESteamGameServerStatsSubsystem::GetUserStatFloat(FESteamId User, const FString& StatName, float& OutValue) const
{
	OutValue = 0.0f;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerStatsAvailable())
	{
		return SteamGameServerStats()->GetUserStat(CSteamID(User.Value), TCHAR_TO_UTF8(*StatName), &OutValue);
	}
#endif
	return false;
}

bool UESteamGameServerStatsSubsystem::SetUserStatInt(FESteamId User, const FString& StatName, int32 Value)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerStatsAvailable())
	{
		return SteamGameServerStats()->SetUserStat(CSteamID(User.Value), TCHAR_TO_UTF8(*StatName), Value);
	}
#endif
	return false;
}

bool UESteamGameServerStatsSubsystem::SetUserStatFloat(FESteamId User, const FString& StatName, float Value)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerStatsAvailable())
	{
		return SteamGameServerStats()->SetUserStat(CSteamID(User.Value), TCHAR_TO_UTF8(*StatName), Value);
	}
#endif
	return false;
}

bool UESteamGameServerStatsSubsystem::UpdateUserAvgRateStat(FESteamId User, const FString& StatName, float CountThisSession, double SessionLength)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerStatsAvailable())
	{
		return SteamGameServerStats()->UpdateUserAvgRateStat(CSteamID(User.Value), TCHAR_TO_UTF8(*StatName), CountThisSession, SessionLength);
	}
#endif
	return false;
}

bool UESteamGameServerStatsSubsystem::GetUserAchievement(FESteamId User, const FString& AchievementName, bool& bOutAchieved) const
{
	bOutAchieved = false;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerStatsAvailable())
	{
		return SteamGameServerStats()->GetUserAchievement(CSteamID(User.Value), TCHAR_TO_UTF8(*AchievementName), &bOutAchieved);
	}
#endif
	return false;
}

bool UESteamGameServerStatsSubsystem::SetUserAchievement(FESteamId User, const FString& AchievementName)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerStatsAvailable())
	{
		return SteamGameServerStats()->SetUserAchievement(CSteamID(User.Value), TCHAR_TO_UTF8(*AchievementName));
	}
#endif
	return false;
}

bool UESteamGameServerStatsSubsystem::ClearUserAchievement(FESteamId User, const FString& AchievementName)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerStatsAvailable())
	{
		return SteamGameServerStats()->ClearUserAchievement(CSteamID(User.Value), TCHAR_TO_UTF8(*AchievementName));
	}
#endif
	return false;
}

bool UESteamGameServerStatsSubsystem::StoreUserStats(FESteamId User)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsGameServerStatsAvailable() || !Callbacks)
	{
		LogGameServerStatsUnavailable(TEXT("StoreUserStats"));
		return false;
	}

	FESteamPendingGSUserStats Request;
	Request.User = User;
	return Callbacks->EnqueueStoreUserStats(Request);
#else
	LogGameServerStatsUnavailable(TEXT("StoreUserStats"));
	return false;
#endif
}
