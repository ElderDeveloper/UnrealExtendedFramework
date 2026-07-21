// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFLeaderboardSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
	int32 ParseLeaderboardPrimaryScore(const TArray<FString>& Scores)
	{
		if (Scores.IsEmpty())
		{
			return 0;
		}

		const int64 ParsedValue = FCString::Atoi64(*Scores[0]);
		return static_cast<int32>(FMath::Clamp<int64>(ParsedValue, MIN_int32, MAX_int32));
	}
}

void UEPFLeaderboardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFLeaderboardSubsystem::Deinitialize()
{
	CachedEntries.Empty();
	Super::Deinitialize();
}

void UEPFLeaderboardSubsystem::GetLeaderboard(const FString& StatisticName, int32 StartPosition, int32 MaxResultsCount)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("LeaderboardName"), StatisticName);
	Body->SetNumberField(TEXT("PageSize"), FMath::Max(MaxResultsCount, 1));
	Body->SetNumberField(TEXT("StartingPosition"), FMath::Max(StartPosition + 1, 1));

	SendPlayFabRequestDetailed(
		TEXT("/Leaderboard/GetLeaderboard"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateUObject(this, &UEPFLeaderboardSubsystem::ParseLeaderboardResponse)
	);
}

void UEPFLeaderboardSubsystem::GetLeaderboardAroundPlayer(const FString& StatisticName, int32 MaxResultsCount)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("LeaderboardName"), StatisticName);
	Body->SetNumberField(TEXT("MaxSurroundingEntries"), FMath::Max(MaxResultsCount, 1));

	TSharedRef<FJsonObject> Entity = MakeShared<FJsonObject>();
	Entity->SetStringField(TEXT("Id"), GetEntityId());
	Entity->SetStringField(TEXT("Type"), GetEntityType());
	Body->SetObjectField(TEXT("Entity"), Entity);

	SendPlayFabRequestDetailed(
		TEXT("/Leaderboard/GetLeaderboardAroundEntity"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateUObject(this, &UEPFLeaderboardSubsystem::ParseLeaderboardResponse)
	);
}

void UEPFLeaderboardSubsystem::GetFriendsLeaderboard(const FString& StatisticName, int32 MaxResultsCount)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("LeaderboardName"), StatisticName);
	Body->SetNumberField(TEXT("PageSize"), FMath::Max(MaxResultsCount, 1));

	TSharedRef<FJsonObject> Entity = MakeShared<FJsonObject>();
	Entity->SetStringField(TEXT("Id"), GetEntityId());
	Entity->SetStringField(TEXT("Type"), GetEntityType());
	Body->SetObjectField(TEXT("Entity"), Entity);

	SendPlayFabRequestDetailed(
		TEXT("/Leaderboard/GetFriendLeaderboardForEntity"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateUObject(this, &UEPFLeaderboardSubsystem::ParseLeaderboardResponse)
	);
}

void UEPFLeaderboardSubsystem::ParseLeaderboardResponse(const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
{
	if (Result.bSuccess && Response.IsValid())
	{
		CachedEntries.Empty();
		const TArray<TSharedPtr<FJsonValue>>* LeaderboardArray = nullptr;
		if (Response->TryGetArrayField(TEXT("Rankings"), LeaderboardArray) && LeaderboardArray)
		{
			for (const TSharedPtr<FJsonValue>& EntryValue : *LeaderboardArray)
			{
				const TSharedPtr<FJsonObject>* EntryObj = nullptr;
				if (EntryValue.IsValid() && EntryValue->TryGetObject(EntryObj) && EntryObj)
				{
					FEPFLeaderboardEntry Entry;
					double Rank = 0;
					(*EntryObj)->TryGetNumberField(TEXT("Rank"), Rank);
					Entry.Position = static_cast<int32>(Rank);
					(*EntryObj)->TryGetStringField(TEXT("DisplayName"), Entry.DisplayName);
					(*EntryObj)->TryGetStringField(TEXT("Metadata"), Entry.Metadata);

					FString LastUpdatedRaw;
					if ((*EntryObj)->TryGetStringField(TEXT("LastUpdated"), LastUpdatedRaw))
					{
						FDateTime::ParseIso8601(*LastUpdatedRaw, Entry.LastUpdated);
					}

					const TSharedPtr<FJsonObject>* EntityObj = nullptr;
					if ((*EntryObj)->TryGetObjectField(TEXT("Entity"), EntityObj) && EntityObj)
					{
						(*EntityObj)->TryGetStringField(TEXT("Id"), Entry.EntityId);
						(*EntityObj)->TryGetStringField(TEXT("Type"), Entry.EntityType);
						Entry.PlayFabId = Entry.EntityId;
					}

					const TArray<TSharedPtr<FJsonValue>>* ScoresArray = nullptr;
					if ((*EntryObj)->TryGetArrayField(TEXT("Scores"), ScoresArray) && ScoresArray)
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
								Entry.Scores.Add(ScoreString);
							}
						}
					}

					Entry.StatValue = ParseLeaderboardPrimaryScore(Entry.Scores);
					CachedEntries.Add(Entry);
				}
			}
		}
		OnLeaderboardReceived.Broadcast(Result, CachedEntries);
	}
	else
	{
		OnLeaderboardReceived.Broadcast(Result, CachedEntries);
	}
}

TArray<FEPFLeaderboardEntry> UEPFLeaderboardSubsystem::GetCachedEntries() const
{
	return CachedEntries;
}

int32 UEPFLeaderboardSubsystem::GetLocalPlayerPosition() const
{
	const FString LocalEntityId = GetEntityId();
	for (const FEPFLeaderboardEntry& Entry : CachedEntries)
	{
		if (Entry.EntityId == LocalEntityId || Entry.PlayFabId == LocalEntityId)
		{
			return Entry.Position;
		}
	}
	return -1;
}

int32 UEPFLeaderboardSubsystem::GetEntryCount() const
{
	return CachedEntries.Num();
}
