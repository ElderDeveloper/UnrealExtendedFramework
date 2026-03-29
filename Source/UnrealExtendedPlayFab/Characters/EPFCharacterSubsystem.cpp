// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFCharacterSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

void UEPFCharacterSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UEPFCharacterSubsystem::Deinitialize() { CachedCharacters.Empty(); Super::Deinitialize(); }

// ── Get All Characters ───────────────────────────────────────────────────────

void UEPFCharacterSubsystem::GetAllCharacters()
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("PlayFabId"), GetPlayFabId());

	SendPlayFabRequestDetailed(TEXT("/Client/GetAllUsersCharacters"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				CachedCharacters.Empty();
				const TArray<TSharedPtr<FJsonValue>>* CharsArr = nullptr;
				if (Response->TryGetArrayField(TEXT("Characters"), CharsArr) && CharsArr)
				{
					for (const auto& V : *CharsArr)
					{
						const TSharedPtr<FJsonObject>* Obj = nullptr;
						if (V->TryGetObject(Obj) && Obj)
						{
							FEPFCharacter C;
							C.CharacterId = (*Obj)->GetStringField(TEXT("CharacterId"));
							C.CharacterName = (*Obj)->GetStringField(TEXT("CharacterName"));
							C.CharacterType = (*Obj)->GetStringField(TEXT("CharacterType"));
							CachedCharacters.Add(C);
						}
					}
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFCharacter — %d characters"), CachedCharacters.Num());
			}
			OnCharactersReceived.Broadcast(Result, CachedCharacters);
		}));
}

// ── Grant Character ──────────────────────────────────────────────────────────

void UEPFCharacterSubsystem::GrantCharacter(const FString& CharacterName, const FString& ItemId)
{
	if (CharacterName.IsEmpty() || ItemId.IsEmpty()) { OnCharacterGranted.Broadcast(FEPFResult::Failure(TEXT("CharacterName and ItemId are required")), TEXT("")); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("CharacterName"), CharacterName);
	Body->SetStringField(TEXT("ItemId"), ItemId);
	Body->SetStringField(TEXT("CatalogVersion"), TEXT(""));

	SendPlayFabRequestDetailed(TEXT("/Client/GrantCharacterToUser"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this, CharacterName](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			FString CharId;
			if (Result.bSuccess && Response.IsValid())
			{
				CharId = Response->GetStringField(TEXT("CharacterId"));
				FEPFCharacter C; C.CharacterId = CharId; C.CharacterName = CharacterName;
				CachedCharacters.Add(C);
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFCharacter — Granted: %s (%s)"), *CharacterName, *CharId);
			}
			OnCharacterGranted.Broadcast(Result, CharId);
		}));
}

// ── Delete Character ─────────────────────────────────────────────────────────

void UEPFCharacterSubsystem::DeleteCharacter(const FString& CharacterId, bool bSaveCharacterInventory)
{
	if (CharacterId.IsEmpty()) { OnCharacterDeleted.Broadcast(FEPFResult::Failure(TEXT("CharacterId cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("CharacterId"), CharacterId);
	Body->SetStringField(TEXT("PlayFabId"), GetPlayFabId());
	Body->SetBoolField(TEXT("SaveCharacterInventory"), bSaveCharacterInventory);

	SendPlayFabRequestDetailed(TEXT("/Client/DeleteCharacterFromUser"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this, CharacterId](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) CachedCharacters.RemoveAll([&](const FEPFCharacter& C) { return C.CharacterId == CharacterId; });
			OnCharacterDeleted.Broadcast(Result);
		}));
}

// ── Character Data ───────────────────────────────────────────────────────────

void UEPFCharacterSubsystem::GetCharacterData(const FString& CharacterId, const TArray<FString>& Keys)
{
	if (CharacterId.IsEmpty()) { OnCharacterDataReceived.Broadcast(FEPFResult::Failure(TEXT("CharacterId cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("CharacterId"), CharacterId);
	Body->SetStringField(TEXT("PlayFabId"), GetPlayFabId());
	if (Keys.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> KeysArr;
		for (const auto& K : Keys) KeysArr.Add(MakeShared<FJsonValueString>(K));
		Body->SetArrayField(TEXT("Keys"), KeysArr);
	}

	SendPlayFabRequestDetailed(TEXT("/Client/GetCharacterData"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			TMap<FString, FString> Data;
			if (Result.bSuccess && Response.IsValid())
			{
				const TSharedPtr<FJsonObject>* DataObj = nullptr;
				if (Response->TryGetObjectField(TEXT("Data"), DataObj) && DataObj)
				{
					for (const auto& Pair : (*DataObj)->Values)
					{
						const TSharedPtr<FJsonObject>* ValObj = nullptr;
						if (Pair.Value->TryGetObject(ValObj) && ValObj) Data.Add(Pair.Key, (*ValObj)->GetStringField(TEXT("Value")));
					}
				}
			}
			OnCharacterDataReceived.Broadcast(Result);
		}));
}

void UEPFCharacterSubsystem::UpdateCharacterData(const FString& CharacterId, const TMap<FString, FString>& Data)
{
	if (CharacterId.IsEmpty() || Data.Num() == 0) { OnCharacterDataUpdated.Broadcast(FEPFResult::Failure(TEXT("CharacterId and Data are required"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("CharacterId"), CharacterId);
	TSharedPtr<FJsonObject> DataObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Data) DataObj->SetStringField(Pair.Key, Pair.Value);
	Body->SetObjectField(TEXT("Data"), DataObj);

	SendPlayFabRequestDetailed(TEXT("/Client/UpdateCharacterData"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>) { OnCharacterDataUpdated.Broadcast(Result); }));
}

// ── Character Statistics ─────────────────────────────────────────────────────

void UEPFCharacterSubsystem::GetCharacterStatistics(const FString& CharacterId)
{
	if (CharacterId.IsEmpty()) { OnCharacterStatsReceived.Broadcast(FEPFResult::Failure(TEXT("CharacterId cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("CharacterId"), CharacterId);

	SendPlayFabRequestDetailed(TEXT("/Client/GetCharacterStatistics"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			TMap<FString, int32> Stats;
			if (Result.bSuccess && Response.IsValid())
			{
				const TSharedPtr<FJsonObject>* StatsObj = nullptr;
				if (Response->TryGetObjectField(TEXT("CharacterStatistics"), StatsObj) && StatsObj)
				{
					for (const auto& Pair : (*StatsObj)->Values)
					{
						double Val = 0; if (Pair.Value->TryGetNumber(Val)) Stats.Add(Pair.Key, static_cast<int32>(Val));
					}
				}
			}
			OnCharacterStatsReceived.Broadcast(Result);
		}));
}

void UEPFCharacterSubsystem::UpdateCharacterStatistics(const FString& CharacterId, const TMap<FString, int32>& Stats)
{
	if (CharacterId.IsEmpty() || Stats.Num() == 0) { OnCharacterDataUpdated.Broadcast(FEPFResult::Failure(TEXT("CharacterId and Stats are required"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("CharacterId"), CharacterId);
	TSharedPtr<FJsonObject> StatsObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Stats) StatsObj->SetNumberField(Pair.Key, Pair.Value);
	Body->SetObjectField(TEXT("CharacterStatistics"), StatsObj);

	SendPlayFabRequestDetailed(TEXT("/Client/UpdateCharacterStatistics"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>) { OnCharacterDataUpdated.Broadcast(Result); }));
}

TArray<FEPFCharacter> UEPFCharacterSubsystem::GetCachedCharacters() const { return CachedCharacters; }
int32 UEPFCharacterSubsystem::GetCharacterCount() const { return CachedCharacters.Num(); }
bool UEPFCharacterSubsystem::FindCharacter(const FString& CharacterId, FEPFCharacter& OutCharacter) const
{
	for (const auto& C : CachedCharacters) { if (C.CharacterId == CharacterId) { OutCharacter = C; return true; } }
	return false;
}

// ── Character Read-Only Data ─────────────────────────────────────────────────

void UEPFCharacterSubsystem::GetCharacterReadOnlyData(const FString& CharacterId, const TArray<FString>& Keys)
{
	if (CharacterId.IsEmpty()) { OnCharacterDataReceived.Broadcast(FEPFResult::Failure(TEXT("CharacterId cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("CharacterId"), CharacterId);
	Body->SetStringField(TEXT("PlayFabId"), GetPlayFabId());
	if (Keys.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> KeysArr;
		for (const auto& K : Keys) KeysArr.Add(MakeShared<FJsonValueString>(K));
		Body->SetArrayField(TEXT("Keys"), KeysArr);
	}

	SendPlayFabRequestDetailed(TEXT("/Client/GetCharacterReadOnlyData"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			OnCharacterDataReceived.Broadcast(Result);
		}));
}

// ── Character Leaderboards ───────────────────────────────────────────────────

void UEPFCharacterSubsystem::GetCharacterLeaderboard(const FString& StatisticName, int32 StartPosition, int32 MaxResultsCount)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("StatisticName"), StatisticName);
	Body->SetNumberField(TEXT("StartPosition"), StartPosition);
	Body->SetNumberField(TEXT("MaxResultsCount"), MaxResultsCount);

	SendPlayFabRequestDetailed(TEXT("/Client/GetCharacterLeaderboard"), Body, true,
		FOnPlayFabResponseDetailed::CreateUObject(this, &UEPFCharacterSubsystem::ParseCharacterLeaderboard));
}

void UEPFCharacterSubsystem::GetLeaderboardAroundCharacter(const FString& StatisticName, const FString& CharacterId, int32 MaxResultsCount)
{
	if (CharacterId.IsEmpty()) { OnCharacterLeaderboardReceived.Broadcast(FEPFResult::Failure(TEXT("CharacterId cannot be empty")), TArray<FEPFLeaderboardEntry>()); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("StatisticName"), StatisticName);
	Body->SetStringField(TEXT("CharacterId"), CharacterId);
	Body->SetNumberField(TEXT("MaxResultsCount"), MaxResultsCount);

	SendPlayFabRequestDetailed(TEXT("/Client/GetLeaderboardAroundCharacter"), Body, true,
		FOnPlayFabResponseDetailed::CreateUObject(this, &UEPFCharacterSubsystem::ParseCharacterLeaderboard));
}

void UEPFCharacterSubsystem::GetLeaderboardForUserCharacters(const FString& StatisticName)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("StatisticName"), StatisticName);

	SendPlayFabRequestDetailed(TEXT("/Client/GetLeaderboardForUserCharacters"), Body, true,
		FOnPlayFabResponseDetailed::CreateUObject(this, &UEPFCharacterSubsystem::ParseCharacterLeaderboard));
}

void UEPFCharacterSubsystem::ParseCharacterLeaderboard(const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
{
	TArray<FEPFLeaderboardEntry> Entries;
	if (Result.bSuccess && Response.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* LeaderArr = nullptr;
		if (Response->TryGetArrayField(TEXT("Leaderboard"), LeaderArr) && LeaderArr)
		{
			for (const auto& V : *LeaderArr)
			{
				const TSharedPtr<FJsonObject>* Obj = nullptr;
				if (V->TryGetObject(Obj) && Obj)
				{
					FEPFLeaderboardEntry E;
					E.Position = static_cast<int32>((*Obj)->GetNumberField(TEXT("Position")));
					E.PlayFabId = (*Obj)->GetStringField(TEXT("PlayFabId"));
					E.DisplayName = (*Obj)->GetStringField(TEXT("DisplayName"));
					E.StatValue = static_cast<int32>((*Obj)->GetNumberField(TEXT("StatValue")));
					Entries.Add(E);
				}
			}
		}
	}
	OnCharacterLeaderboardReceived.Broadcast(Result, Entries);
}

