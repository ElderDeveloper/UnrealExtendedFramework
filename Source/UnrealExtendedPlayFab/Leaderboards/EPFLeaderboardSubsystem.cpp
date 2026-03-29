// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFLeaderboardSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

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
	Body->SetStringField(TEXT("StatisticName"), StatisticName);
	Body->SetNumberField(TEXT("StartPosition"), StartPosition);
	Body->SetNumberField(TEXT("MaxResultsCount"), MaxResultsCount);

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetLeaderboard"),
		Body,
		EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateUObject(this, &UEPFLeaderboardSubsystem::ParseLeaderboardResponse)
	);
}

void UEPFLeaderboardSubsystem::GetLeaderboardAroundPlayer(const FString& StatisticName, int32 MaxResultsCount)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("StatisticName"), StatisticName);
	Body->SetNumberField(TEXT("MaxResultsCount"), MaxResultsCount);

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetLeaderboardAroundPlayer"),
		Body,
		EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateUObject(this, &UEPFLeaderboardSubsystem::ParseLeaderboardResponse)
	);
}

void UEPFLeaderboardSubsystem::GetFriendsLeaderboard(const FString& StatisticName, int32 MaxResultsCount)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("StatisticName"), StatisticName);
	Body->SetNumberField(TEXT("MaxResultsCount"), MaxResultsCount);

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetFriendLeaderboard"),
		Body,
		EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateUObject(this, &UEPFLeaderboardSubsystem::ParseLeaderboardResponse)
	);
}

void UEPFLeaderboardSubsystem::ParseLeaderboardResponse(const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
{
	if (Result.bSuccess && Response.IsValid())
	{
		CachedEntries.Empty();
		const TArray<TSharedPtr<FJsonValue>>* LeaderboardArray = nullptr;
		if (Response->TryGetArrayField(TEXT("Leaderboard"), LeaderboardArray) && LeaderboardArray)
		{
			for (const auto& EntryValue : *LeaderboardArray)
			{
				const TSharedPtr<FJsonObject>* EntryObj = nullptr;
				if (EntryValue->TryGetObject(EntryObj) && EntryObj)
				{
					FEPFLeaderboardEntry Entry;
					// Position and StatValue use TryGetNumber to avoid crashes on missing fields.
					// DisplayName is only present when the title has ShowDisplayName enabled.
					double Position = 0, StatValue = 0;
					(*EntryObj)->TryGetNumberField(TEXT("Position"), Position);
					(*EntryObj)->TryGetNumberField(TEXT("StatValue"), StatValue);
					Entry.Position = static_cast<int32>(Position);
					Entry.StatValue = static_cast<int32>(StatValue);
					(*EntryObj)->TryGetStringField(TEXT("PlayFabId"), Entry.PlayFabId);
					(*EntryObj)->TryGetStringField(TEXT("DisplayName"), Entry.DisplayName);
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
	const FString LocalId = GetPlayFabId();
	for (const FEPFLeaderboardEntry& Entry : CachedEntries)
	{
		if (Entry.PlayFabId == LocalId)
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
