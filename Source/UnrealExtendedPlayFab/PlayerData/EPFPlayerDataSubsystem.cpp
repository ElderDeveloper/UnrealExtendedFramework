// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFPlayerDataSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

void UEPFPlayerDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFPlayerDataSubsystem::Deinitialize()
{
	CachedData.Empty();
	Super::Deinitialize();
}

void UEPFPlayerDataSubsystem::GetPlayerData(const TArray<FString>& Keys)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> KeysArray;
	for (const FString& Key : Keys)
	{
		KeysArray.Add(MakeShared<FJsonValueString>(Key));
	}
	Body->SetArrayField(TEXT("Keys"), KeysArray);

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetUserData"),
		Body,
		EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this, Keys](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				// Remove the specifically-requested keys before merging so
				// keys deleted server-side don't ghost inside the local cache.
				for (const FString& Key : Keys) { CachedData.Remove(Key); }
				const TSharedPtr<FJsonObject>* DataObj = nullptr;
				if (Response->TryGetObjectField(TEXT("Data"), DataObj) && DataObj)
				{
					for (const auto& Pair : (*DataObj)->Values)
					{
						const TSharedPtr<FJsonObject>* EntryObj = nullptr;
						if (Pair.Value->TryGetObject(EntryObj) && EntryObj)
						{
							CachedData.Add(Pair.Key, (*EntryObj)->GetStringField(TEXT("Value")));
						}
					}
				}
				OnPlayerDataReceived.Broadcast(FEPFResult::Success());
			}
			else
			{
				OnPlayerDataReceived.Broadcast(Result);
			}
		})
	);
}

void UEPFPlayerDataSubsystem::GetAllPlayerData()
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetUserData"),
		Body,
		EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				CachedData.Empty();
				const TSharedPtr<FJsonObject>* DataObj = nullptr;
				if (Response->TryGetObjectField(TEXT("Data"), DataObj) && DataObj)
				{
					for (const auto& Pair : (*DataObj)->Values)
					{
						const TSharedPtr<FJsonObject>* EntryObj = nullptr;
						if (Pair.Value->TryGetObject(EntryObj) && EntryObj)
						{
							CachedData.Add(Pair.Key, (*EntryObj)->GetStringField(TEXT("Value")));
						}
					}
				}
				OnPlayerDataReceived.Broadcast(FEPFResult::Success());
			}
			else
			{
				OnPlayerDataReceived.Broadcast(Result);
			}
		})
	);
}

void UEPFPlayerDataSubsystem::SetPlayerData(const TMap<FString, FString>& Data, bool bPublic)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	TSharedRef<FJsonObject> DataObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Data)
	{
		DataObj->SetStringField(Pair.Key, Pair.Value);
	}
	Body->SetObjectField(TEXT("Data"), DataObj);
	// Permission controls whether other players can read this data via GetPlayerProfile.
	// "Public" enables it; "Private" (default) restricts it to the owning player.
	Body->SetStringField(TEXT("Permission"), bPublic ? TEXT("Public") : TEXT("Private"));

	SendPlayFabRequestDetailed(
		TEXT("/Client/UpdateUserData"),
		Body,
		EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this, Data](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess) 			{
				// Update local cache
				for (const auto& Pair : Data)
				{
					CachedData.Add(Pair.Key, Pair.Value);
				}
			}
			OnPlayerDataUpdated.Broadcast(Result);
		})
	);
}

void UEPFPlayerDataSubsystem::DeletePlayerData(const FString& Key)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> KeysToRemove;
	KeysToRemove.Add(MakeShared<FJsonValueString>(Key));
	Body->SetArrayField(TEXT("KeysToRemove"), KeysToRemove);

	SendPlayFabRequestDetailed(
		TEXT("/Client/UpdateUserData"),
		Body,
		EEPFAuthMode::SessionTicket,
		FOnPlayFabResponseDetailed::CreateLambda([this, Key](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess) 			{
				CachedData.Remove(Key);
			}
			OnPlayerDataUpdated.Broadcast(Result);
		})
	);
}

FString UEPFPlayerDataSubsystem::GetCachedValue(const FString& Key) const
{
	const FString* Value = CachedData.Find(Key);
	return Value ? *Value : FString();
}

bool UEPFPlayerDataSubsystem::HasKey(const FString& Key) const
{
	return CachedData.Contains(Key);
}

TMap<FString, FString> UEPFPlayerDataSubsystem::GetAllCachedData() const
{
	return CachedData;
}
