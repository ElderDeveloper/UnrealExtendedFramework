// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFIdLookupSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

void UEPFIdLookupSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UEPFIdLookupSubsystem::Deinitialize() { Super::Deinitialize(); }


void UEPFIdLookupSubsystem::GetPlayFabIDsFromSteamIDs(const TArray<FString>& SteamIds)
{
	SendIdLookupRequest(
		TEXT("/Client/GetPlayFabIDsFromSteamIDs"),
		TEXT("SteamStringIDs"),
		SteamIds,
		TEXT("SteamStringId"),
		TEXT("Data")
	);
}

void UEPFIdLookupSubsystem::GetPlayFabIDsFromCustomIDs(const TArray<FString>& CustomIds)
{
	SendIdLookupRequest(
		TEXT("/Client/GetPlayFabIDsFromCustomIDs"),
		TEXT("CustomIDs"),
		CustomIds,
		TEXT("CustomId"),
		TEXT("Data")
	);
}

void UEPFIdLookupSubsystem::GetPlayFabIDsFromXboxLiveIDs(const TArray<FString>& XboxLiveIds)
{
	SendIdLookupRequest(
		TEXT("/Client/GetPlayFabIDsFromXboxLiveIDs"),
		TEXT("XboxLiveAccountIDs"),
		XboxLiveIds,
		TEXT("XboxLiveAccountId"),
		TEXT("Data")
	);
}

void UEPFIdLookupSubsystem::GetPlayFabIDsFromPSNAccountIDs(const TArray<FString>& PsnAccountIds)
{
	SendIdLookupRequest(
		TEXT("/Client/GetPlayFabIDsFromPSNAccountIDs"),
		TEXT("PSNAccountIDs"),
		PsnAccountIds,
		TEXT("PSNAccountId"),
		TEXT("Data")
	);
}


void UEPFIdLookupSubsystem::SendIdLookupRequest(const FString& Endpoint, const FString& ArrayFieldName, const TArray<FString>& Ids, const FString& PlatformIdField, const FString& ResponseArrayField)
{
	if (Ids.Num() == 0) { OnIdLookupComplete.Broadcast(FEPFResult::Failure(TEXT("Ids cannot be empty")), TArray<FEPFIdMapping>()); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> IdsArr;
	for (const auto& Id : Ids) IdsArr.Add(MakeShared<FJsonValueString>(Id));
	Body->SetArrayField(ArrayFieldName, IdsArr);

	SendPlayFabRequestDetailed(Endpoint, Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this, PlatformIdField, ResponseArrayField](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			TArray<FEPFIdMapping> Mappings;
			if (Result.bSuccess && Response.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* DataArr = nullptr;
				if (Response->TryGetArrayField(ResponseArrayField, DataArr) && DataArr)
				{
					for (const auto& V : *DataArr)
					{
						const TSharedPtr<FJsonObject>* Obj = nullptr;
						if (V->TryGetObject(Obj) && Obj)
						{
							FEPFIdMapping M;
							M.PlatformId = (*Obj)->GetStringField(PlatformIdField);
							M.PlayFabId = (*Obj)->GetStringField(TEXT("PlayFabId"));
							if (!M.PlayFabId.IsEmpty()) Mappings.Add(M);
						}
					}
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFIdLookup — %d mappings found"), Mappings.Num());
			}
			OnIdLookupComplete.Broadcast(Result, Mappings);
		}));
}
