// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFTitleDataSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

void UEPFTitleDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFTitleDataSubsystem::Deinitialize()
{
	CachedData.Empty();
	Super::Deinitialize();
}

void UEPFTitleDataSubsystem::GetTitleData(const TArray<FString>& Keys)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	if (Keys.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> JsonKeys;
		for (const FString& Key : Keys)
		{
			JsonKeys.Add(MakeShared<FJsonValueString>(Key));
		}
		Body->SetArrayField(TEXT("Keys"), JsonKeys);
	}

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetTitleData"),
		Body,
		true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				const TSharedPtr<FJsonObject>* DataObj = nullptr;
				if (Response->TryGetObjectField(TEXT("Data"), DataObj) && DataObj)
				{
					for (const auto& Pair : (*DataObj)->Values)
					{
						FString Value;
						if (Pair.Value->TryGetString(Value))
						{
							CachedData.Add(Pair.Key, Value);
						}
					}
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFTitleDataSubsystem — Received %d keys"), CachedData.Num());
				OnTitleDataReceived.Broadcast(Result);
			}
			else
			{
				UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFTitleDataSubsystem — Failed to fetch title data"));
				OnTitleDataReceived.Broadcast(Result);
			}
		})
	);
}

void UEPFTitleDataSubsystem::GetAllTitleData()
{
	GetTitleData(TArray<FString>());
}

void UEPFTitleDataSubsystem::GetTitleInternalData(const TArray<FString>& Keys)
{
	// ⚠ /Client/GetTitleInternalData does NOT exist in the PlayFab Client API.
	// Internal data is server-only (/Server/GetTitleInternalData requires a developer secret key).
	// To read internal title data from a client, route the call through a CloudScript function.
	UE_LOG(LogExtendedPlayFab, Error, TEXT(
		"UEPFTitleDataSubsystem::GetTitleInternalData — This endpoint is not accessible from the "
		"client API. Use a CloudScript (Azure Function) to read internal title data and return it "
		"to the client instead."));
	OnTitleDataReceived.Broadcast(FEPFResult::Failure(
		TEXT("GetTitleInternalData is not available on the Client API. Use CloudScript instead.")));
}

FString UEPFTitleDataSubsystem::GetCachedValue(const FString& Key) const
{
	const FString* Found = CachedData.Find(Key);
	return Found ? *Found : FString();
}

int32 UEPFTitleDataSubsystem::GetCachedInt(const FString& Key, int32 DefaultValue) const
{
	const FString* Found = CachedData.Find(Key);
	if (!Found || Found->IsEmpty()) return DefaultValue;
	if (Found->IsNumeric()) return FCString::Atoi(**Found);
	return DefaultValue;
}

float UEPFTitleDataSubsystem::GetCachedFloat(const FString& Key, float DefaultValue) const
{
	const FString* Found = CachedData.Find(Key);
	if (!Found || Found->IsEmpty()) return DefaultValue;
	if (Found->IsNumeric()) return FCString::Atof(**Found);
	return DefaultValue;
}

bool UEPFTitleDataSubsystem::GetCachedBool(const FString& Key, bool DefaultValue) const
{
	const FString* Found = CachedData.Find(Key);
	if (!Found || Found->IsEmpty()) return DefaultValue;
	if (Found->Equals(TEXT("true"), ESearchCase::IgnoreCase) || *Found == TEXT("1")) return true;
	if (Found->Equals(TEXT("false"), ESearchCase::IgnoreCase) || *Found == TEXT("0")) return false;
	return DefaultValue;
}

bool UEPFTitleDataSubsystem::HasKey(const FString& Key) const
{
	return CachedData.Contains(Key);
}

TMap<FString, FString> UEPFTitleDataSubsystem::GetAllCachedData() const
{
	return CachedData;
}
