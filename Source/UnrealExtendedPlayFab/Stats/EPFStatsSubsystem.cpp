// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFStatsSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

void UEPFStatsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFStatsSubsystem::Deinitialize()
{
	CachedStats.Empty();
	Super::Deinitialize();
}

void UEPFStatsSubsystem::UpdateStats(const TMap<FString, int32>& StatsToUpdate)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> StatsArray;
	for (const auto& Pair : StatsToUpdate)
	{
		TSharedPtr<FJsonObject> StatObj = MakeShared<FJsonObject>();
		StatObj->SetStringField(TEXT("StatisticName"), Pair.Key);
		StatObj->SetNumberField(TEXT("Value"), Pair.Value);
		StatsArray.Add(MakeShared<FJsonValueObject>(StatObj));
	}
	Body->SetArrayField(TEXT("Statistics"), StatsArray);

	SendPlayFabRequestDetailed(
		TEXT("/Client/UpdatePlayerStatistics"),
		Body,
		true,
		FOnPlayFabResponseDetailed::CreateLambda([this, StatsToUpdate](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess)
			{
				// Update local cache
				for (const auto& Pair : StatsToUpdate)
				{
					bool bFound = false;
					for (FEPFStatistic& Stat : CachedStats)
					{
						if (Stat.StatName == Pair.Key)
						{
							Stat.Value = Pair.Value;
							bFound = true;
							break;
						}
					}
					if (!bFound)
					{
						FEPFStatistic NewStat;
						NewStat.StatName = Pair.Key;
						NewStat.Value = Pair.Value;
						CachedStats.Add(NewStat);
					}
				}
			}
			OnStatsUpdated.Broadcast(Result);
		})
	);
}

void UEPFStatsSubsystem::UpdateStat(const FString& StatName, int32 Value)
{
	TMap<FString, int32> SingleStat;
	SingleStat.Add(StatName, Value);
	UpdateStats(SingleStat);
}

void UEPFStatsSubsystem::GetStats(const TArray<FString>& StatNames)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> NamesArray;
	for (const FString& Name : StatNames)
	{
		NamesArray.Add(MakeShared<FJsonValueString>(Name));
	}
	Body->SetArrayField(TEXT("StatisticNames"), NamesArray);

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetPlayerStatistics"),
		Body,
		EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this, StatNames](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				// Remove the requested stat names from cache first so deleted
				// server-side stats don't survive as stale entries.
				for (const FString& Name : StatNames)
				{
					CachedStats.RemoveAll([&Name](const FEPFStatistic& S){ return S.StatName == Name; });
				}
				const TArray<TSharedPtr<FJsonValue>>* StatsArray = nullptr;
				if (Response->TryGetArrayField(TEXT("Statistics"), StatsArray) && StatsArray)
				{
					for (const auto& StatValue : *StatsArray)
					{
						const TSharedPtr<FJsonObject>* StatObj = nullptr;
						if (StatValue->TryGetObject(StatObj) && StatObj)
						{
							FEPFStatistic Stat;
							Stat.StatName = (*StatObj)->GetStringField(TEXT("StatisticName"));
							Stat.Value = (*StatObj)->GetIntegerField(TEXT("Value"));
							Stat.Version = (*StatObj)->GetIntegerField(TEXT("Version"));
							CachedStats.Add(Stat);
						}
					}
				}
				OnStatsReceived.Broadcast(Result, CachedStats);
			}
			else
			{
				OnStatsReceived.Broadcast(Result, CachedStats);
			}
		})
	);
}

void UEPFStatsSubsystem::GetAllStats()
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetPlayerStatistics"),
		Body,
		EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* StatsArray = nullptr;
				if (Response->TryGetArrayField(TEXT("Statistics"), StatsArray) && StatsArray)
				{
					CachedStats.Empty();
					for (const auto& StatValue : *StatsArray)
					{
						const TSharedPtr<FJsonObject>* StatObj = nullptr;
						if (StatValue->TryGetObject(StatObj) && StatObj)
						{
							FEPFStatistic Stat;
							Stat.StatName = (*StatObj)->GetStringField(TEXT("StatisticName"));
							Stat.Value = (*StatObj)->GetIntegerField(TEXT("Value"));
							Stat.Version = (*StatObj)->GetIntegerField(TEXT("Version"));
							CachedStats.Add(Stat);
						}
					}
				}
				OnStatsReceived.Broadcast(Result, CachedStats);
			}
			else
			{
				OnStatsReceived.Broadcast(Result, CachedStats);
			}
		})
	);
}

TArray<FEPFStatistic> UEPFStatsSubsystem::GetCachedStats() const
{
	return CachedStats;
}

int32 UEPFStatsSubsystem::GetCachedStatValue(const FString& StatName) const
{
	for (const FEPFStatistic& Stat : CachedStats)
	{
		if (Stat.StatName == StatName)
		{
			return Stat.Value;
		}
	}
	return -1;
}

int32 UEPFStatsSubsystem::GetCachedStatValueSafe(const FString& StatName, bool& bFound) const
{
	for (const FEPFStatistic& Stat : CachedStats)
	{
		if (Stat.StatName == StatName)
		{
			bFound = true;
			return Stat.Value;
		}
	}
	bFound = false;
	return 0;
}

bool UEPFStatsSubsystem::HasStat(const FString& StatName) const
{
	for (const FEPFStatistic& Stat : CachedStats)
	{
		if (Stat.StatName == StatName)
		{
			return true;
		}
	}
	return false;
}
