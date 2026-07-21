// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFSharedDataSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

void UEPFSharedDataSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UEPFSharedDataSubsystem::Deinitialize() { CachedGroupData.Empty(); Super::Deinitialize(); }

// ── Create ───────────────────────────────────────────────────────────────────

void UEPFSharedDataSubsystem::CreateSharedGroup(const FString& SharedGroupId)
{
	if (SharedGroupId.IsEmpty()) { OnGroupCreated.Broadcast(FEPFResult::Failure(TEXT("SharedGroupId cannot be empty")), TEXT("")); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("SharedGroupId"), SharedGroupId);

	SendPlayFabRequestDetailed(TEXT("/Client/CreateSharedGroup"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			FString Id;
			if (Result.bSuccess && Response.IsValid()) Id = Response->GetStringField(TEXT("SharedGroupId"));
			OnGroupCreated.Broadcast(Result, Id);
		}));
}

// ── Get ──────────────────────────────────────────────────────────────────────

void UEPFSharedDataSubsystem::GetSharedGroupData(const FString& SharedGroupId, const TArray<FString>& Keys)
{
	if (SharedGroupId.IsEmpty()) { OnDataReceived.Broadcast(FEPFResult::Failure(TEXT("SharedGroupId cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("SharedGroupId"), SharedGroupId);
	Body->SetBoolField(TEXT("GetMembers"), false);

	if (Keys.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> KeysArr;
		for (const auto& K : Keys) KeysArr.Add(MakeShared<FJsonValueString>(K));
		Body->SetArrayField(TEXT("Keys"), KeysArr);
	}

	SendPlayFabRequestDetailed(TEXT("/Client/GetSharedGroupData"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this, SharedGroupId](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
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
						if (Pair.Value->TryGetObject(ValObj) && ValObj)
						{
							Data.Add(FString(Pair.Key.Len(), *Pair.Key), (*ValObj)->GetStringField(TEXT("Value")));
						}
					}
				}
				CachedGroupData.Add(SharedGroupId, Data);
			}
			OnDataReceived.Broadcast(Result);
		}));
}

// ── Update ───────────────────────────────────────────────────────────────────

void UEPFSharedDataSubsystem::UpdateSharedGroupData(const FString& SharedGroupId, const TMap<FString, FString>& Data)
{
	if (SharedGroupId.IsEmpty() || Data.Num() == 0) { OnDataUpdated.Broadcast(FEPFResult::Failure(TEXT("SharedGroupId and Data are required"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("SharedGroupId"), SharedGroupId);

	TSharedPtr<FJsonObject> DataObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Data) DataObj->SetStringField(Pair.Key, Pair.Value);
	Body->SetObjectField(TEXT("Data"), DataObj);

	SendPlayFabRequestDetailed(TEXT("/Client/UpdateSharedGroupData"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this, SharedGroupId, Data](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess)
			{
				TMap<FString, FString>& Cached = CachedGroupData.FindOrAdd(SharedGroupId);
				for (const auto& Pair : Data) Cached.Add(Pair.Key, Pair.Value);
			}
			OnDataUpdated.Broadcast(Result);
		}));
}

// ── Remove ───────────────────────────────────────────────────────────────────

void UEPFSharedDataSubsystem::RemoveSharedGroupData(const FString& SharedGroupId, const TArray<FString>& KeysToRemove)
{
	if (SharedGroupId.IsEmpty() || KeysToRemove.Num() == 0) { OnDataUpdated.Broadcast(FEPFResult::Failure(TEXT("SharedGroupId and KeysToRemove are required"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("SharedGroupId"), SharedGroupId);

	TArray<TSharedPtr<FJsonValue>> KeysArr;
	for (const auto& K : KeysToRemove) KeysArr.Add(MakeShared<FJsonValueString>(K));
	Body->SetArrayField(TEXT("KeysToRemove"), KeysArr);

	SendPlayFabRequestDetailed(TEXT("/Client/UpdateSharedGroupData"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this, SharedGroupId, KeysToRemove](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess)
			{
				TMap<FString, FString>* Cached = CachedGroupData.Find(SharedGroupId);
				if (Cached) { for (const auto& K : KeysToRemove) Cached->Remove(K); }
			}
			OnDataUpdated.Broadcast(Result);
		}));
}

// ── Members ──────────────────────────────────────────────────────────────────

void UEPFSharedDataSubsystem::AddSharedGroupMembers(const FString& SharedGroupId, const TArray<FString>& PlayFabIds)
{
	if (SharedGroupId.IsEmpty() || PlayFabIds.Num() == 0) { OnMembersChanged.Broadcast(FEPFResult::Failure(TEXT("SharedGroupId and PlayFabIds are required"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("SharedGroupId"), SharedGroupId);
	TArray<TSharedPtr<FJsonValue>> Ids;
	for (const auto& Id : PlayFabIds) Ids.Add(MakeShared<FJsonValueString>(Id));
	Body->SetArrayField(TEXT("PlayFabIds"), Ids);

	SendPlayFabRequestDetailed(TEXT("/Client/AddSharedGroupMembers"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>) { OnMembersChanged.Broadcast(Result); }));
}

void UEPFSharedDataSubsystem::RemoveSharedGroupMembers(const FString& SharedGroupId, const TArray<FString>& PlayFabIds)
{
	if (SharedGroupId.IsEmpty() || PlayFabIds.Num() == 0) { OnMembersChanged.Broadcast(FEPFResult::Failure(TEXT("SharedGroupId and PlayFabIds are required"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("SharedGroupId"), SharedGroupId);
	TArray<TSharedPtr<FJsonValue>> Ids;
	for (const auto& Id : PlayFabIds) Ids.Add(MakeShared<FJsonValueString>(Id));
	Body->SetArrayField(TEXT("PlayFabIds"), Ids);

	SendPlayFabRequestDetailed(TEXT("/Client/RemoveSharedGroupMembers"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>) { OnMembersChanged.Broadcast(Result); }));
}

// ── Queries ──────────────────────────────────────────────────────────────────

TMap<FString, FString> UEPFSharedDataSubsystem::GetCachedData(const FString& SharedGroupId) const
{
	const TMap<FString, FString>* Found = CachedGroupData.Find(SharedGroupId);
	return Found ? *Found : TMap<FString, FString>();
}
