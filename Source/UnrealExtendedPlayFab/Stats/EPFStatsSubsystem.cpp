// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFStatsSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
	void AssignStatisticFromJson(const TSharedPtr<FJsonObject>& StatObj, FEPFStatistic& OutStat)
	{
		StatObj->TryGetStringField(TEXT("Name"), OutStat.StatName);
		StatObj->TryGetStringField(TEXT("Metadata"), OutStat.Metadata);
		StatObj->TryGetNumberField(TEXT("Version"), OutStat.Version);

		OutStat.Scores.Reset();
		const TArray<TSharedPtr<FJsonValue>>* ScoresArray = nullptr;
		if (StatObj->TryGetArrayField(TEXT("Scores"), ScoresArray) && ScoresArray)
		{
			for (const TSharedPtr<FJsonValue>& ScoreValue : *ScoresArray)
			{
				if (!ScoreValue.IsValid())
				{
					continue;
				}

				FString ScoreString;
				if (!ScoreValue->TryGetString(ScoreString))
				{
					double ScoreNumber = 0.0;
					if (ScoreValue->TryGetNumber(ScoreNumber))
					{
						ScoreString = FString::Printf(TEXT("%.0f"), ScoreNumber);
					}
				}

				if (!ScoreString.IsEmpty())
				{
					OutStat.Scores.Add(ScoreString);
				}
			}
		}
	}
}

void UEPFStatsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFStatsSubsystem::Deinitialize()
{
	CachedStats.Empty();
	Super::Deinitialize();
}

void UEPFStatsSubsystem::UpdateStats(const TMap<FString, int32>& StatsToUpdate, const FString& TransactionId)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> StatsArray;
	for (const auto& Pair : StatsToUpdate)
	{
		TSharedPtr<FJsonObject> StatObj = MakeShared<FJsonObject>();
		StatObj->SetStringField(TEXT("Name"), Pair.Key);

		TArray<TSharedPtr<FJsonValue>> Scores;
		Scores.Add(MakeShared<FJsonValueString>(FString::FromInt(Pair.Value)));
		StatObj->SetArrayField(TEXT("Scores"), Scores);
		StatsArray.Add(MakeShared<FJsonValueObject>(StatObj));
	}
	Body->SetArrayField(TEXT("Statistics"), StatsArray);
	if (!TransactionId.IsEmpty())
	{
		Body->SetStringField(TEXT("TransactionId"), TransactionId);
	}

	SendPlayFabRequestDetailed(
		TEXT("/Statistic/UpdateStatistics"),
		Body,
		EEPFAuthMode::EntityToken,
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
						if (Stat.StatName == FString(Pair.Key.Len(), *Pair.Key))
						{
							Stat.Value = Pair.Value;
							Stat.Scores = { FString::FromInt(Pair.Value) };
							bFound = true;
							break;
						}
					}
					if (!bFound)
					{
						FEPFStatistic NewStat;
						NewStat.StatName = FString(Pair.Key.Len(), *Pair.Key);
						NewStat.Value = Pair.Value;
						NewStat.Scores = { FString::FromInt(Pair.Value) };
						CachedStats.Add(NewStat);
					}
				}

				if (Response.IsValid())
				{
					ParseStatisticsResponse(Result, Response, nullptr);
				}
			}
			OnStatsUpdated.Broadcast(Result);
		})
	);
}

void UEPFStatsSubsystem::UpdateStat(const FString& StatName, int32 Value, const FString& TransactionId)
{
	TMap<FString, int32> SingleStat;
	SingleStat.Add(StatName, Value);
	UpdateStats(SingleStat, TransactionId);
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
		TEXT("/Statistic/GetStatistics"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this, StatNames](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			ParseStatisticsResponse(Result, Response, &StatNames);
		})
	);
}

void UEPFStatsSubsystem::GetAllStats()
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	SendPlayFabRequestDetailed(
		TEXT("/Statistic/GetStatistics"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			ParseStatisticsResponse(Result, Response, nullptr);
		})
	);
}

void UEPFStatsSubsystem::ParseStatisticsResponse(const FEPFResult& Result, TSharedPtr<FJsonObject> Response, const TArray<FString>* RequestedNames)
{
	if (Result.bSuccess && Response.IsValid())
	{
		if (RequestedNames)
		{
			for (const FString& Name : *RequestedNames)
			{
				CachedStats.RemoveAll([&Name](const FEPFStatistic& S){ return S.StatName == Name; });
			}
		}
		else
		{
			CachedStats.Empty();
		}

		const TSharedPtr<FJsonObject>* StatsObject = nullptr;
		if (Response->TryGetObjectField(TEXT("Statistics"), StatsObject) && StatsObject)
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*StatsObject)->Values)
			{
				const TSharedPtr<FJsonObject>* StatObj = nullptr;
				if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(StatObj) || !StatObj)
				{
					continue;
				}

				FEPFStatistic Stat;
				AssignStatisticFromJson(*StatObj, Stat);
				if (Stat.StatName.IsEmpty())
				{
					Stat.StatName = FString(Pair.Key.Len(), *Pair.Key);
				}
				Stat.Value = ParsePrimaryScore(Stat.Scores);
				if (!Stat.StatName.IsEmpty())
				{
					CachedStats.Add(MoveTemp(Stat));
				}
			}
		}
	}

	OnStatsReceived.Broadcast(Result, CachedStats);
}

int32 UEPFStatsSubsystem::ParsePrimaryScore(const TArray<FString>& Scores)
{
	if (Scores.IsEmpty())
	{
		return 0;
	}

	const int64 ParsedValue = FCString::Atoi64(*Scores[0]);
	return static_cast<int32>(FMath::Clamp<int64>(ParsedValue, MIN_int32, MAX_int32));
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
