// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Achievements/OnlineAchievementsExtendedSteam.h"
#include "Core/OnlineSubsystemExtendedSteam.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace ExtendedSteamAchievements
{
	/** True while the shared module has the Steam client API up. */
	static bool IsSteamClientUp()
	{
#if WITH_EXTENDEDSTEAM_SDK
		return FExtendedSteamSharedModule::IsModuleAvailable()
			&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();
#else
		return false;
#endif
	}

	/**
	 * The common OSS convention treats an achievement stat as "unlocked" at 100.0 progress.
	 * Bools unlock when true; numeric types when the value reaches 100.
	 */
	static bool DoesVariantUnlock(const FVariantData& Value)
	{
		switch (Value.GetType())
		{
		case EOnlineKeyValuePairDataType::Bool:
		{
			bool bValue = false;
			Value.GetValue(bValue);
			return bValue;
		}
		case EOnlineKeyValuePairDataType::Float:
		{
			float FloatValue = 0.0f;
			Value.GetValue(FloatValue);
			return FloatValue >= 100.0f;
		}
		case EOnlineKeyValuePairDataType::Double:
		{
			double DoubleValue = 0.0;
			Value.GetValue(DoubleValue);
			return DoubleValue >= 100.0;
		}
		case EOnlineKeyValuePairDataType::Int32:
		{
			int32 IntValue = 0;
			Value.GetValue(IntValue);
			return IntValue >= 100;
		}
		case EOnlineKeyValuePairDataType::UInt32:
		{
			uint32 UIntValue = 0;
			Value.GetValue(UIntValue);
			return UIntValue >= 100u;
		}
		case EOnlineKeyValuePairDataType::Int64:
		{
			int64 Int64Value = 0;
			Value.GetValue(Int64Value);
			return Int64Value >= 100;
		}
		default:
			return false;
		}
	}
}

#if WITH_EXTENDEDSTEAM_SDK
/**
 * Native Steam callback listeners for the achievements interface; alive for the lifetime of
 * the owning FOnlineAchievementsExtendedSteam (which the subsystem destroys on Shutdown).
 */
class FOnlineAchievementsExtendedSteamCallbacks
{
public:
	explicit FOnlineAchievementsExtendedSteamCallbacks(FOnlineAchievementsExtendedSteam* InOwner)
		: Owner(InOwner)
		, UserStatsStoredCallback(this, &FOnlineAchievementsExtendedSteamCallbacks::OnUserStatsStored)
		, UserAchievementStoredCallback(this, &FOnlineAchievementsExtendedSteamCallbacks::OnUserAchievementStored)
#if !ESTEAM_SDK_AT_LEAST(161)
		, UserStatsReceivedCallback(this, &FOnlineAchievementsExtendedSteamCallbacks::OnUserStatsReceived)
#endif
	{
	}

private:
	/** True when the callback belongs to this app id (StoreStats callbacks are app-global). */
	static bool IsForThisApp(uint64 GameId)
	{
		return SteamUtils() != nullptr && CGameID(GameId).AppID() == SteamUtils()->GetAppID();
	}

	void OnUserStatsStored(UserStatsStored_t* Data)
	{
		if (Data == nullptr)
		{
			return;
		}

		if (IsForThisApp(Data->m_nGameID))
		{
			Owner->HandleUserStatsStored(Data->m_eResult == k_EResultOK);
		}
	}

	void OnUserAchievementStored(UserAchievementStored_t* Data)
	{
		if (Data == nullptr)
		{
			return;
		}

		if (IsForThisApp(Data->m_nGameID))
		{
			// Both progress fields zero means the achievement fully unlocked (progress-only
			// notifications from IndicateAchievementProgress are not unlock events).
			const bool bFullyUnlocked = Data->m_nCurProgress == 0 && Data->m_nMaxProgress == 0;
			Owner->HandleUserAchievementStored(FString(UTF8_TO_TCHAR(Data->m_rgchAchievementName)), bFullyUnlocked);
		}
	}

#if !ESTEAM_SDK_AT_LEAST(161)
	void OnUserStatsReceived(UserStatsReceived_t* Data)
	{
		if (Data == nullptr)
		{
			return;
		}

		if (IsForThisApp(Data->m_nGameID)
			&& SteamUser() != nullptr
			&& Data->m_steamIDUser == SteamUser()->GetSteamID())
		{
			Owner->HandleUserStatsReceived(Data->m_eResult == k_EResultOK);
		}
	}
#endif

	/** Owner owns this holder, so a raw back pointer is safe. */
	FOnlineAchievementsExtendedSteam* Owner = nullptr;

	CCallback<FOnlineAchievementsExtendedSteamCallbacks, UserStatsStored_t> UserStatsStoredCallback;
	CCallback<FOnlineAchievementsExtendedSteamCallbacks, UserAchievementStored_t> UserAchievementStoredCallback;
#if !ESTEAM_SDK_AT_LEAST(161)
	CCallback<FOnlineAchievementsExtendedSteamCallbacks, UserStatsReceived_t> UserStatsReceivedCallback;
#endif
};
#else
class FOnlineAchievementsExtendedSteamCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

FOnlineAchievementsExtendedSteam::FOnlineAchievementsExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem)
	: Subsystem(InSubsystem)
{
#if WITH_EXTENDEDSTEAM_SDK
	// Create the callback holder unconditionally: registering Steam callback listeners is valid before
	// the client API is fully up, and WriteAchievements gates only on SteamUserStats() (not on the
	// holder), so the UserStatsStored_t listener must exist even when this interface was constructed
	// before Steam finished initializing — otherwise a later StoreStats would never complete.
	Callbacks = MakeShared<FOnlineAchievementsExtendedSteamCallbacks>(this);
#endif
}

FOnlineAchievementsExtendedSteam::~FOnlineAchievementsExtendedSteam() = default;

uint64 FOnlineAchievementsExtendedSteam::GetLocalSteamId64() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamAchievements::IsSteamClientUp() && SteamUser() != nullptr)
	{
		return SteamUser()->GetSteamID().ConvertToUint64();
	}
#endif
	return 0;
}

bool FOnlineAchievementsExtendedSteam::IsLocalPlayer(const FUniqueNetId& PlayerId) const
{
	const uint64 LocalSteamId64 = GetLocalSteamId64();
	return LocalSteamId64 != 0
		&& PlayerId.IsValid()
		&& PlayerId == *FUniqueNetIdExtendedSteam::Create(LocalSteamId64);
}

bool FOnlineAchievementsExtendedSteam::CacheAchievements()
{
#if WITH_EXTENDEDSTEAM_SDK
	ISteamUserStats* Stats = ExtendedSteamAchievements::IsSteamClientUp() ? SteamUserStats() : nullptr;
	if (Stats == nullptr)
	{
		return false;
	}

	const uint32 NumAchievements = Stats->GetNumAchievements();

	CachedAchievements.Reset(NumAchievements);
	for (uint32 Index = 0; Index < NumAchievements; ++Index)
	{
		const char* ApiName = Stats->GetAchievementName(Index);
		if (ApiName == nullptr || ApiName[0] == '\0')
		{
			continue;
		}

		bool bAchieved = false;
		uint32 UnlockTime = 0;
		Stats->GetAchievementAndUnlockTime(ApiName, &bAchieved, &UnlockTime);

		FOnlineAchievement& Achievement = CachedAchievements.AddDefaulted_GetRef();
		Achievement.Id = FString(UTF8_TO_TCHAR(ApiName));
		// Steam only exposes a binary unlocked state through this API; map it to the 0/100 convention.
		Achievement.Progress = bAchieved ? 100.0 : 0.0;
	}

	bAchievementsCached = true;
	return true;
#else
	return false;
#endif
}

bool FOnlineAchievementsExtendedSteam::CacheAchievementDescriptions()
{
#if WITH_EXTENDEDSTEAM_SDK
	ISteamUserStats* Stats = ExtendedSteamAchievements::IsSteamClientUp() ? SteamUserStats() : nullptr;
	if (Stats == nullptr)
	{
		return false;
	}

	const uint32 NumAchievements = Stats->GetNumAchievements();

	CachedAchievementDescs.Reset();
	CachedAchievementDescs.Reserve(NumAchievements);
	for (uint32 Index = 0; Index < NumAchievements; ++Index)
	{
		const char* ApiName = Stats->GetAchievementName(Index);
		if (ApiName == nullptr || ApiName[0] == '\0')
		{
			continue;
		}

		const char* DisplayName = Stats->GetAchievementDisplayAttribute(ApiName, "name");
		const char* Description = Stats->GetAchievementDisplayAttribute(ApiName, "desc");
		const char* Hidden = Stats->GetAchievementDisplayAttribute(ApiName, "hidden");

		bool bAchieved = false;
		uint32 UnlockTime = 0;
		Stats->GetAchievementAndUnlockTime(ApiName, &bAchieved, &UnlockTime);

		FOnlineAchievementDesc Desc;
		Desc.Title = FText::FromString(FString(UTF8_TO_TCHAR(DisplayName != nullptr ? DisplayName : "")));
		// Steam has a single localized description; use it for both the locked and unlocked text.
		Desc.LockedDesc = FText::FromString(FString(UTF8_TO_TCHAR(Description != nullptr ? Description : "")));
		Desc.UnlockedDesc = Desc.LockedDesc;
		Desc.bIsHidden = Hidden != nullptr && FCStringAnsi::Strcmp(Hidden, "1") == 0;
		Desc.UnlockTime = bAchieved && UnlockTime != 0 ? FDateTime::FromUnixTimestamp(static_cast<int64>(UnlockTime)) : FDateTime(0);

		CachedAchievementDescs.Add(FString(UTF8_TO_TCHAR(ApiName)), MoveTemp(Desc));
	}

	bDescriptionsCached = true;
	return true;
#else
	return false;
#endif
}

void FOnlineAchievementsExtendedSteam::QueryAchievements(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate)
{
	if (!IsLocalPlayer(PlayerId))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("QueryAchievements: only the local Steam user is supported (got %s, subsystem instance %s)"),
			*PlayerId.ToDebugString(), Subsystem != nullptr ? *Subsystem->GetInstanceName().ToString() : TEXT("<none>"));
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

#if ESTEAM_SDK_AT_LEAST(161)
	// 1.61+: the Steam client synchronized the local user's stats/achievements before the game
	// process started, so the achievement list is readable right away - cache it and complete
	// synchronously.
	const bool bSuccess = CacheAchievements();
	Delegate.ExecuteIfBound(PlayerId, bSuccess);
#elif WITH_EXTENDEDSTEAM_SDK
	// Pre-1.61: stats must be requested explicitly; completion arrives via UserStatsReceived_t.
	ISteamUserStats* Stats = ExtendedSteamAchievements::IsSteamClientUp() ? SteamUserStats() : nullptr;
	if (Stats == nullptr)
	{
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	PendingQueryDelegates.Add(Delegate);
	if (PendingQueryDelegates.Num() == 1 && !Stats->RequestCurrentStats())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("QueryAchievements: RequestCurrentStats failed"));
		CompletePendingQueries(false);
	}
#else
	Delegate.ExecuteIfBound(PlayerId, false);
#endif
}

void FOnlineAchievementsExtendedSteam::QueryAchievementDescriptions(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate)
{
	if (!IsLocalPlayer(PlayerId))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("QueryAchievementDescriptions: only the local Steam user is supported (got %s)"), *PlayerId.ToDebugString());
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	// Display attributes come from the app schema the Steam client already holds; the read is
	// synchronous on every SDK version once stats are synced (automatic since 1.61; on older
	// SDKs call QueryAchievements first).
	const bool bSuccess = CacheAchievementDescriptions();
	Delegate.ExecuteIfBound(PlayerId, bSuccess);
}

EOnlineCachedResult::Type FOnlineAchievementsExtendedSteam::GetCachedAchievement(const FUniqueNetId& PlayerId, const FString& AchievementId, FOnlineAchievement& OutAchievement)
{
	if (!bAchievementsCached || !IsLocalPlayer(PlayerId))
	{
		return EOnlineCachedResult::NotFound;
	}

	for (const FOnlineAchievement& Achievement : CachedAchievements)
	{
		if (Achievement.Id == AchievementId)
		{
			OutAchievement = Achievement;
			return EOnlineCachedResult::Success;
		}
	}
	return EOnlineCachedResult::NotFound;
}

EOnlineCachedResult::Type FOnlineAchievementsExtendedSteam::GetCachedAchievements(const FUniqueNetId& PlayerId, TArray<FOnlineAchievement>& OutAchievements)
{
	if (!bAchievementsCached || !IsLocalPlayer(PlayerId))
	{
		return EOnlineCachedResult::NotFound;
	}

	OutAchievements = CachedAchievements;
	return EOnlineCachedResult::Success;
}

EOnlineCachedResult::Type FOnlineAchievementsExtendedSteam::GetCachedAchievementDescription(const FString& AchievementId, FOnlineAchievementDesc& OutAchievementDesc)
{
	if (!bDescriptionsCached)
	{
		return EOnlineCachedResult::NotFound;
	}

	if (const FOnlineAchievementDesc* Desc = CachedAchievementDescs.Find(AchievementId))
	{
		OutAchievementDesc = *Desc;
		return EOnlineCachedResult::Success;
	}
	return EOnlineCachedResult::NotFound;
}

void FOnlineAchievementsExtendedSteam::WriteAchievements(const FUniqueNetId& PlayerId, FOnlineAchievementsWriteRef& WriteObject, const FOnAchievementsWrittenDelegate& Delegate)
{
	if (!IsLocalPlayer(PlayerId))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("WriteAchievements: only the local Steam user is supported (got %s)"), *PlayerId.ToDebugString());
		WriteObject->WriteState = EOnlineAsyncTaskState::Failed;
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	if (bWriteInFlight)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("WriteAchievements: a write is already in flight; sequence writes through the completion delegate"));
		WriteObject->WriteState = EOnlineAsyncTaskState::Failed;
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

#if WITH_EXTENDEDSTEAM_SDK
	ISteamUserStats* Stats = ExtendedSteamAchievements::IsSteamClientUp() ? SteamUserStats() : nullptr;
	if (Stats == nullptr)
	{
		WriteObject->WriteState = EOnlineAsyncTaskState::Failed;
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	WriteObject->WriteState = EOnlineAsyncTaskState::InProgress;

	int32 NumUnlocked = 0;
	bool bAllSucceeded = true;
	for (const TPair<FString, FVariantData>& Stat : WriteObject->Properties)
	{
		if (!ExtendedSteamAchievements::DoesVariantUnlock(Stat.Value))
		{
			// Steam has no partial-progress storage on achievements themselves; progress below
			// the unlock threshold is ignored (use stats + IndicateAchievementProgress for toasts).
			continue;
		}

		if (Stats->SetAchievement(TCHAR_TO_UTF8(*Stat.Key)))
		{
			++NumUnlocked;
		}
		else
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("WriteAchievements: SetAchievement failed for '%s' (is it defined in the app schema?)"), *Stat.Key);
			bAllSucceeded = false;
		}
	}

	if (NumUnlocked == 0)
	{
		// Nothing reached the unlock threshold (or every SetAchievement failed): complete
		// synchronously without a round trip to Steam.
		WriteObject->WriteState = bAllSucceeded ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;
		Delegate.ExecuteIfBound(PlayerId, bAllSucceeded);
		return;
	}

	// One StoreStats commits the whole batch; the delegate fires when UserStatsStored_t confirms.
	bWriteInFlight = true;
	PendingWriteObject = WriteObject;
	PendingWriteDelegate = Delegate;

	if (!Stats->StoreStats())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("WriteAchievements: StoreStats failed"));
		CompletePendingWrite(false);
	}
#else
	WriteObject->WriteState = EOnlineAsyncTaskState::Failed;
	Delegate.ExecuteIfBound(PlayerId, false);
#endif
}

#if !UE_BUILD_SHIPPING
bool FOnlineAchievementsExtendedSteam::ResetAchievements(const FUniqueNetId& PlayerId)
{
	if (!IsLocalPlayer(PlayerId))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ResetAchievements: only the local Steam user is supported (got %s)"), *PlayerId.ToDebugString());
		return false;
	}

#if WITH_EXTENDEDSTEAM_SDK
	ISteamUserStats* Stats = ExtendedSteamAchievements::IsSteamClientUp() ? SteamUserStats() : nullptr;
	if (Stats == nullptr)
	{
		return false;
	}

	// Development helper: wipes stats AND achievements for the local user.
	const bool bReset = Stats->ResetAllStats(true);
	if (bReset)
	{
		// Invalidate the caches; the next QueryAchievements re-reads the cleared state.
		bAchievementsCached = false;
		CachedAchievements.Reset();
	}
	return bReset;
#else
	return false;
#endif
}
#endif // !UE_BUILD_SHIPPING

void FOnlineAchievementsExtendedSteam::CompletePendingQueries(bool bSuccess)
{
	if (bSuccess)
	{
		bSuccess = CacheAchievements();
	}

	TArray<FOnQueryAchievementsCompleteDelegate> Delegates = MoveTemp(PendingQueryDelegates);
	PendingQueryDelegates.Reset();

	const FUniqueNetIdExtendedSteamPtr LocalId = FUniqueNetIdExtendedSteam::Create(GetLocalSteamId64());
	for (const FOnQueryAchievementsCompleteDelegate& Delegate : Delegates)
	{
		Delegate.ExecuteIfBound(LocalId.IsValid() ? *LocalId : *FUniqueNetIdExtendedSteam::EmptyId(), bSuccess);
	}
}

void FOnlineAchievementsExtendedSteam::CompletePendingWrite(bool bSuccess)
{
	if (!bWriteInFlight)
	{
		return;
	}

	bWriteInFlight = false;

	if (PendingWriteObject.IsValid())
	{
		PendingWriteObject->WriteState = bSuccess ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;
		PendingWriteObject.Reset();
	}

	FOnAchievementsWrittenDelegate Delegate = MoveTemp(PendingWriteDelegate);
	PendingWriteDelegate.Unbind();

	const FUniqueNetIdExtendedSteamPtr LocalId = FUniqueNetIdExtendedSteam::Create(GetLocalSteamId64());
	Delegate.ExecuteIfBound(LocalId.IsValid() ? *LocalId : *FUniqueNetIdExtendedSteam::EmptyId(), bSuccess);
}

void FOnlineAchievementsExtendedSteam::HandleUserStatsStored(bool bSuccess)
{
	// UserStatsStored_t fires for every StoreStats of this app, including ones issued by other
	// plugin features; only consume it while our own write is pending.
	CompletePendingWrite(bSuccess);
}

void FOnlineAchievementsExtendedSteam::HandleUserAchievementStored(const FString& AchievementApiName, bool bFullyUnlocked)
{
	if (!bFullyUnlocked)
	{
		return;
	}

	// Keep the cache coherent without forcing a re-query.
	for (FOnlineAchievement& Achievement : CachedAchievements)
	{
		if (Achievement.Id == AchievementApiName)
		{
			Achievement.Progress = 100.0;
			break;
		}
	}

	const FUniqueNetIdExtendedSteamPtr LocalId = FUniqueNetIdExtendedSteam::Create(GetLocalSteamId64());
	if (LocalId.IsValid() && LocalId->IsValid())
	{
		TriggerOnAchievementUnlockedDelegates(*LocalId, AchievementApiName);
	}
}

void FOnlineAchievementsExtendedSteam::HandleUserStatsReceived(bool bSuccess)
{
	// Pre-1.61 RequestCurrentStats completion; a no-op when no query is pending (the callback
	// also fires for stats loads triggered elsewhere).
	if (PendingQueryDelegates.Num() > 0)
	{
		CompletePendingQueries(bSuccess);
	}
}
