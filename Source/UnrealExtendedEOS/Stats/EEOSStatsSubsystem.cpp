// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSStatsSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineStatsInterface.h"
#include "UnrealExtendedEOS.h"

void UEEOSStatsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSStatsSubsystem::Deinitialize()
{
	CachedStats.Empty();
	Super::Deinitialize();
}

void UEEOSStatsSubsystem::IngestStat(const FString& StatName, int32 Amount)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("IngestStat"));
		OnStatIngested.Broadcast(false);
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineStatsPtr StatsInterface = EOSSub->GetStatsInterface();
	if (!StatsInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSStatsSubsystem::IngestStat — Stats interface not available"));
		OnStatIngested.Broadcast(false);
		return;
	}

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSStatsSubsystem::IngestStat — User not logged in"));
		OnStatIngested.Broadcast(false);
		return;
	}

	TMap<FString, FOnlineStatUpdate> StatUpdates;
	StatUpdates.Add(StatName, FOnlineStatUpdate(Amount, FOnlineStatUpdate::EOnlineStatModificationType::Sum));

	FOnlineStatsUserUpdatedStats UserStats(UserId->AsShared(), StatUpdates);

	TArray<FOnlineStatsUserUpdatedStats> UpdatedStats;
	UpdatedStats.Add(MoveTemp(UserStats));

	StatsInterface->UpdateStats(UserId->AsShared(), UpdatedStats,
		FOnlineStatsUpdateStatsComplete::CreateLambda([this](const FOnlineError& Error)
		{
			const bool bSuccess = Error.WasSuccessful();
			if (bSuccess)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSStatsSubsystem: Stat ingested successfully"));
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSStatsSubsystem: Failed to ingest stat — %s"), *Error.GetErrorMessage().ToString());
			}
			OnStatIngested.Broadcast(bSuccess);
		}));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSStatsSubsystem::IngestStat — Ingesting '%s' += %d"), *StatName, Amount);
}

void UEEOSStatsSubsystem::QueryStats(const FString& UserId, const TArray<FString>& StatNames)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryStats"));
		OnStatsQueried.Broadcast(false, TArray<FEEOSStat>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineStatsPtr StatsInterface = EOSSub->GetStatsInterface();
	if (!StatsInterface.IsValid())
	{
		OnStatsQueried.Broadcast(false, TArray<FEEOSStat>());
		return;
	}

	const FUniqueNetIdRef NetId = FUniqueNetIdString::Create(UserId, EOS_SUBSYSTEM);
	TArray<FUniqueNetIdRef> Users;
	Users.Add(NetId);

	StatsInterface->QueryStats(EOSSub->GetIdentityInterface()->GetUniquePlayerId(0)->AsShared(), Users, StatNames,
		FOnlineStatsQueryUsersStatsComplete::CreateLambda([this](const FOnlineError& Error, const TArray<TSharedRef<const FOnlineStatsUserStats>>& UsersStats)
		{
			CachedStats.Empty();
			const bool bSuccess = Error.WasSuccessful();

			if (bSuccess && UsersStats.Num() > 0)
			{
				for (const auto& UserStats : UsersStats)
				{
					for (const auto& Pair : UserStats->Stats)
					{
						FEEOSStat Stat;
						Stat.StatName = Pair.Key;
						Pair.Value.GetValue(Stat.Value);
						CachedStats.Add(Stat);
					}
				}
			}

			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSStatsSubsystem: Query stats %s — %d stats returned"), bSuccess ? TEXT("succeeded") : TEXT("failed"), CachedStats.Num());
			OnStatsQueried.Broadcast(bSuccess, CachedStats);
		}));
}

void UEEOSStatsSubsystem::QueryLocalStats(const TArray<FString>& StatNames)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryLocalStats"));
		OnStatsQueried.Broadcast(false, TArray<FEEOSStat>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (UserId.IsValid())
	{
		QueryStats(UserId->ToString(), StatNames);
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSStatsSubsystem::QueryLocalStats — User not logged in"));
		OnStatsQueried.Broadcast(false, TArray<FEEOSStat>());
	}
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

void UEEOSStatsSubsystem::IngestStatsBatch(const TMap<FString, int32>& StatsToIngest)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("IngestStatsBatch"));
		OnStatIngested.Broadcast(false);
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineStatsPtr StatsInterface = EOSSub->GetStatsInterface();
	if (!StatsInterface.IsValid())
	{
		OnStatIngested.Broadcast(false);
		return;
	}

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		OnStatIngested.Broadcast(false);
		return;
	}

	TMap<FString, FOnlineStatUpdate> StatUpdates;
	for (const auto& Pair : StatsToIngest)
	{
		StatUpdates.Add(Pair.Key, FOnlineStatUpdate(Pair.Value, FOnlineStatUpdate::EOnlineStatModificationType::Sum));
	}

	FOnlineStatsUserUpdatedStats UserStats(UserId->AsShared(), StatUpdates);
	TArray<FOnlineStatsUserUpdatedStats> UpdatedStats;
	UpdatedStats.Add(MoveTemp(UserStats));

	StatsInterface->UpdateStats(UserId->AsShared(), UpdatedStats,
		FOnlineStatsUpdateStatsComplete::CreateLambda([this, StatsToIngest](const FOnlineError& Error)
		{
			const bool bSuccess = Error.WasSuccessful();
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSStatsSubsystem: Batch ingest %d stats — %s"), StatsToIngest.Num(), bSuccess ? TEXT("succeeded") : TEXT("failed"));
			OnStatIngested.Broadcast(bSuccess);
		}));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSStatsSubsystem::IngestStatsBatch — Ingesting %d stats"), StatsToIngest.Num());
}
