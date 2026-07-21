// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSStatsSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineStatsInterface.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "UnrealExtendedEOS.h"

#include "eos_sdk.h"
#include "eos_stats.h"

void UEEOSStatsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSStatsSubsystem::Deinitialize()
{
	CachedStats.Empty();
	Super::Deinitialize();
}

bool UEEOSStatsSubsystem::IngestStat(const FString& StatName, int32 Amount)
{
	TMap<FString, int32> SingleStat;
	SingleStat.Add(StatName, Amount);
	return IngestStatsInternal(TEXT("IngestStat"), SingleStat);
}

bool UEEOSStatsSubsystem::IngestStatsBatch(const TMap<FString, int32>& StatsToIngest)
{
	return IngestStatsInternal(TEXT("IngestStatsBatch"), StatsToIngest);
}

bool UEEOSStatsSubsystem::IngestStatsInternal(const TCHAR* FunctionName, const TMap<FString, int32>& StatsToIngest)
{
	if (StatsToIngest.Num() == 0)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSStatsSubsystem::%s — No stats to ingest"), FunctionName);
		OnStatIngested.Broadcast(false);
		return false;
	}

	if (StatsToIngest.Num() > EOS_STATS_MAX_INGEST_STATS)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSStatsSubsystem::%s — %d stats exceeds the SDK limit of %d per ingest"),
			FunctionName, StatsToIngest.Num(), EOS_STATS_MAX_INGEST_STATS);
		OnStatIngested.Broadcast(false);
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(FunctionName);
		OnStatIngested.Broadcast(false);
		return false;
	}

	// The OSS stats path is not used because FOnlineStatsEOS::UpdateStats() executes its
	// completion delegate synchronously with Success without waiting for the SDK
	// (OnlineStatsEOS.cpp:368 — "Mabye these wrote correctly...") — ingesting through the
	// raw SDK is the only way to broadcast the real async result. A silently failed ingest
	// would otherwise report success while achievements backed by the stat never unlock.
	// Same pattern as UEEOSLeaderboardSubsystem::UploadScore.
	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	EOS_HStats StatsHandle = PlatformHandle ? EOS_Platform_GetStatsInterface(PlatformHandle) : nullptr;
	if (!StatsHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSStatsSubsystem::%s — Stats interface not available"), FunctionName);
		OnStatIngested.Broadcast(false);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	FUniqueNetIdPtr LocalUserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LocalUserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSStatsSubsystem::%s — User not logged in"), FunctionName);
		OnStatIngested.Broadcast(false);
		return false;
	}

	// ToString() is the composite "<EpicAccountId>|<ProductUserId>" — the ingest needs the
	// bare PUID, and EOS_ProductUserId_FromString performs NO validation, so guard the string.
	const FString LocalPUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(LocalUserId->ToString());
	if (LocalPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSStatsSubsystem::%s — Logged-in user has no Product User ID (no Connect session)"), FunctionName);
		OnStatIngested.Broadcast(false);
		return false;
	}
	const EOS_ProductUserId LocalPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*LocalPUIDStr));

	// Build the EOS_Stats_IngestData array with stable UTF-8 name storage.
	// Two-pass: first convert every name into stable heap storage, THEN take the pointers.
	// (Storing converters in a growing array relocates them past the inline slack and
	// dangles every previously-taken char* — same class of bug fixed in UserInfo.)
	TArray<TArray<ANSICHAR>> NameStorage;
	NameStorage.Reserve(StatsToIngest.Num());
	TArray<int32> Amounts;
	Amounts.Reserve(StatsToIngest.Num());
	for (const TPair<FString, int32>& Pair : StatsToIngest)
	{
		// EOS stat names are upper case in the Dev Portal; mirror the engine's stats path,
		// which uppercases the outgoing name and warns (OnlineStatsEOS.cpp:294–300).
		const FString StatNameUpper = Pair.Key.ToUpper();
		if (StatNameUpper != Pair.Key)
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSStatsSubsystem::%s — Stat name '%s' is not upper case; ingesting as '%s' (EOS stat names are upper case in the Dev Portal)"),
				FunctionName, *Pair.Key, *StatNameUpper);
		}

		FTCHARToUTF8 Converted(*StatNameUpper);
		TArray<ANSICHAR>& Stored = NameStorage.Emplace_GetRef();
		Stored.Append(Converted.Get(), Converted.Length());
		Stored.Add('\0');

		Amounts.Add(Pair.Value);
	}

	// The inner arrays' heap buffers are stable now — safe to take raw pointers. Both
	// containers stay alive until this function returns, which is after EOS_Stats_IngestStat
	// has copied the strings (the SDK copies its inputs synchronously).
	TArray<EOS_Stats_IngestData> IngestData;
	IngestData.Reserve(NameStorage.Num());
	for (int32 Index = 0; Index < NameStorage.Num(); ++Index)
	{
		EOS_Stats_IngestData& Entry = IngestData.Emplace_GetRef();
		Entry = {};
		Entry.ApiVersion = EOS_STATS_INGESTDATA_API_LATEST;
		Entry.StatName = NameStorage[Index].GetData();
		Entry.IngestAmount = Amounts[Index];
	}

	EOS_Stats_IngestStatOptions Options = {};
	Options.ApiVersion = EOS_STATS_INGESTSTAT_API_LATEST;
	Options.LocalUserId = LocalPUID;
	Options.TargetUserId = LocalPUID;
	Options.Stats = IngestData.GetData();
	Options.StatsCount = static_cast<uint32_t>(IngestData.Num());

	struct FIngestStatsContext
	{
		// Weak — the EOS platform outlives this subsystem, so the callback can fire after GC
		TWeakObjectPtr<UEEOSStatsSubsystem> Self;
		int32 NumStats = 0;
	};

	FIngestStatsContext* Context = new FIngestStatsContext();
	Context->Self = this;
	Context->NumStats = IngestData.Num();

	EOS_Stats_IngestStat(StatsHandle, &Options, Context,
		[](const EOS_Stats_IngestStatCompleteCallbackInfo* Data)
		{
			TUniquePtr<FIngestStatsContext> Ctx(static_cast<FIngestStatsContext*>(Data->ClientData));
			if (!Ctx) return;

			UEEOSStatsSubsystem* Self = Ctx->Self.Get();
			if (!Self) return;

			const bool bSuccess = Data->ResultCode == EOS_EResult::EOS_Success;
			if (bSuccess)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSStatsSubsystem: Ingested %d stat(s) successfully"), Ctx->NumStats);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSStatsSubsystem: EOS_Stats_IngestStat failed for %d stat(s): %s"),
					Ctx->NumStats, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}
			Self->OnStatIngested.Broadcast(bSuccess);
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSStatsSubsystem::%s — Ingesting %d stat(s)..."), FunctionName, IngestData.Num());
	return true;
}

bool UEEOSStatsSubsystem::QueryStats(const FString& UserId, const TArray<FString>& StatNames)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryStats"));
		OnStatsQueried.Broadcast(false, TArray<FEEOSStat>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineStatsPtr StatsInterface = EOSSub->GetStatsInterface();
	if (!StatsInterface.IsValid())
	{
		OnStatsQueried.Broadcast(false, TArray<FEEOSStat>());
		return false;
	}

	// Accept a bare Product User ID: the EOS net id parser expects the composite
	// "<EpicAccountId>|<ProductUserId>", so an input without a '|' is treated as the PUID
	// half with an empty Epic half — mirrors UserInfo's bare-EAS treatment, PUID side.
	FString CompositeId = UserId;
	if (!CompositeId.Contains(TEXT("|")))
	{
		CompositeId = TEXT("|") + CompositeId;
	}

	// Must be constructed by the identity interface: the EOS OSS downcasts incoming ids to
	// FUniqueNetIdEOS, so a generic FUniqueNetIdString here is undefined behavior.
	const FUniqueNetIdPtr NetId = EOSSub->GetIdentityInterface()->CreateUniquePlayerId(CompositeId);
	const FUniqueNetIdPtr LocalUserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!NetId.IsValid() || !LocalUserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSStatsSubsystem::QueryStats — Invalid user id or not logged in"));
		OnStatsQueried.Broadcast(false, TArray<FEEOSStat>());
		return false;
	}

	// The engine skips users whose parsed id has a null PUID and fires NO completion at all
	// when zero users are eligible (OnlineStatsEOS.cpp:133-210) — an id without a PUID half
	// would hang this query forever. Require a non-empty PUID half specifically; pointer
	// validity of the parsed id is not enough.
	if (UEEOSBlueprintLibrary::ExtractProductUserId(NetId->ToString()).IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSStatsSubsystem::QueryStats — User id '%s' has no Product User ID half; EOS stats are keyed by PUID and the engine would never complete this query"), *UserId);
		OnStatsQueried.Broadcast(false, TArray<FEEOSStat>());
		return false;
	}

	TArray<FUniqueNetIdRef> Users;
	Users.Add(NetId.ToSharedRef());

	StatsInterface->QueryStats(LocalUserId.ToSharedRef(), Users, StatNames,
		FOnlineStatsQueryUsersStatsComplete::CreateWeakLambda(this, [this, QueriedId = NetId.ToSharedRef()](const FOnlineError& Error, const TArray<TSharedRef<const FOnlineStatsUserStats>>& UsersStats)
		{
			CachedStats.Empty();
			bool bQueriedUserPresent = false;

			// The engine's completion payload is its interface-wide ACCUMULATED cache — it
			// contains every user ever queried through the interface, and the engine computes
			// its success flag as cache-non-empty (OnlineStatsEOS.cpp:202-210). Filter to the
			// user THIS query asked about, and require that user's presence for success —
			// otherwise query A then B would flatten both users into the cache and a failed
			// query after any success would report stale success.
			for (const TSharedRef<const FOnlineStatsUserStats>& UserStats : UsersStats)
			{
				if (*UserStats->Account != *QueriedId)
				{
					continue;
				}

				bQueriedUserPresent = true;
				for (const TPair<FString, FOnlineStatValue>& Pair : UserStats->Stats)
				{
					FEEOSStat Stat;
					Stat.StatName = Pair.Key;
					Pair.Value.GetValue(Stat.Value);
					CachedStats.Add(Stat);
				}
				break;
			}

			const bool bSuccess = Error.WasSuccessful() && bQueriedUserPresent;
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSStatsSubsystem: Query stats %s — %d stats returned"), bSuccess ? TEXT("succeeded") : TEXT("failed"), CachedStats.Num());
			OnStatsQueried.Broadcast(bSuccess, CachedStats);
		}));
	return true;
}

bool UEEOSStatsSubsystem::QueryLocalStats(const TArray<FString>& StatNames)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryLocalStats"));
		OnStatsQueried.Broadcast(false, TArray<FEEOSStat>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (UserId.IsValid())
	{
		return QueryStats(UserId->ToString(), StatNames);
	}

	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSStatsSubsystem::QueryLocalStats — User not logged in"));
	OnStatsQueried.Broadcast(false, TArray<FEEOSStat>());
	return false;
}

TArray<FEEOSStat> UEEOSStatsSubsystem::GetCachedStats() const
{
	return CachedStats;
}

int32 UEEOSStatsSubsystem::GetStatValue(const FString& StatName) const
{
	for (const auto& Stat : CachedStats)
	{
		if (Stat.StatName == StatName)
		{
			return Stat.Value;
		}
	}
	return 0;
}

bool UEEOSStatsSubsystem::HasStat(const FString& StatName) const
{
	for (const auto& Stat : CachedStats)
	{
		if (Stat.StatName == StatName)
		{
			return true;
		}
	}
	return false;
}
