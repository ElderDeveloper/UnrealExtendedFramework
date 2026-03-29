// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFBlueprintLibrary.h"
#include "Shared/EPFSettings.h"
#include "Auth/EPFAuthSubsystem.h"
#include "PlayerData/EPFPlayerDataSubsystem.h"
#include "Stats/EPFStatsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"


// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Internal helper
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

template<typename T>
static T* GetSubsystemFromContext(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	if (!World) return nullptr;

	UGameInstance* GI = World->GetGameInstance();
	if (!GI) return nullptr;

	return GI->GetSubsystem<T>();
}


// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Status Queries
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

bool UEPFBlueprintLibrary::IsPlayFabConfigured()
{
	const UEPFSettings* Settings = UEPFSettings::Get();
	return Settings && !Settings->TitleId.IsEmpty();
}

bool UEPFBlueprintLibrary::IsPlayFabLoggedIn(const UObject* WorldContext)
{
	UEPFAuthSubsystem* Auth = GetSubsystemFromContext<UEPFAuthSubsystem>(WorldContext);
	return Auth && Auth->IsLoggedIn();
}

FString UEPFBlueprintLibrary::GetPlayFabId(const UObject* WorldContext)
{
	UEPFAuthSubsystem* Auth = GetSubsystemFromContext<UEPFAuthSubsystem>(WorldContext);
	return Auth ? Auth->GetPlayFabId() : FString();
}

FString UEPFBlueprintLibrary::GetPlayFabDisplayName(const UObject* WorldContext)
{
	UEPFAuthSubsystem* Auth = GetSubsystemFromContext<UEPFAuthSubsystem>(WorldContext);
	return Auth ? Auth->GetDisplayName() : FString();
}


// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Type Conversion Helpers
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

FString UEPFBlueprintLibrary::IntToPlayFabString(int32 Value)
{
	return FString::FromInt(Value);
}

FString UEPFBlueprintLibrary::FloatToPlayFabString(float Value)
{
	return FString::SanitizeFloat(Value);
}

FString UEPFBlueprintLibrary::BoolToPlayFabString(bool bValue)
{
	return bValue ? TEXT("true") : TEXT("false");
}

int32 UEPFBlueprintLibrary::PlayFabStringToInt(const FString& Value, int32 DefaultValue)
{
	if (Value.IsEmpty()) return DefaultValue;
	if (Value.IsNumeric()) return FCString::Atoi(*Value);
	return DefaultValue;
}

float UEPFBlueprintLibrary::PlayFabStringToFloat(const FString& Value, float DefaultValue)
{
	if (Value.IsEmpty()) return DefaultValue;
	if (Value.IsNumeric()) return FCString::Atof(*Value);
	return DefaultValue;
}

bool UEPFBlueprintLibrary::PlayFabStringToBool(const FString& Value, bool DefaultValue)
{
	if (Value.IsEmpty()) return DefaultValue;
	if (Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value == TEXT("1")) return true;
	if (Value.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Value == TEXT("0")) return false;
	return DefaultValue;
}


// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Cached Data Quick Access
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

int32 UEPFBlueprintLibrary::GetCachedPlayerInt(const UObject* WorldContext, const FString& Key, int32 DefaultValue)
{
	UEPFPlayerDataSubsystem* Sub = GetSubsystemFromContext<UEPFPlayerDataSubsystem>(WorldContext);
	if (!Sub) return DefaultValue;
	return PlayFabStringToInt(Sub->GetCachedValue(Key), DefaultValue);
}

float UEPFBlueprintLibrary::GetCachedPlayerFloat(const UObject* WorldContext, const FString& Key, float DefaultValue)
{
	UEPFPlayerDataSubsystem* Sub = GetSubsystemFromContext<UEPFPlayerDataSubsystem>(WorldContext);
	if (!Sub) return DefaultValue;
	return PlayFabStringToFloat(Sub->GetCachedValue(Key), DefaultValue);
}

bool UEPFBlueprintLibrary::GetCachedPlayerBool(const UObject* WorldContext, const FString& Key, bool DefaultValue)
{
	UEPFPlayerDataSubsystem* Sub = GetSubsystemFromContext<UEPFPlayerDataSubsystem>(WorldContext);
	if (!Sub) return DefaultValue;
	return PlayFabStringToBool(Sub->GetCachedValue(Key), DefaultValue);
}

FString UEPFBlueprintLibrary::GetCachedPlayerString(const UObject* WorldContext, const FString& Key)
{
	UEPFPlayerDataSubsystem* Sub = GetSubsystemFromContext<UEPFPlayerDataSubsystem>(WorldContext);
	return Sub ? Sub->GetCachedValue(Key) : FString();
}

int32 UEPFBlueprintLibrary::GetCachedStat(const UObject* WorldContext, const FString& StatName)
{
	UEPFStatsSubsystem* Sub = GetSubsystemFromContext<UEPFStatsSubsystem>(WorldContext);
	return Sub ? Sub->GetCachedStatValue(StatName) : -1;
}


// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Struct Constructors
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

FEPFAnalyticsEvent UEPFBlueprintLibrary::MakeAnalyticsEvent(const FString& EventName, const TMap<FString, FString>& Body)
{
	FEPFAnalyticsEvent Event;
	Event.EventName = EventName;
	Event.Body = Body;
	return Event;
}

void UEPFBlueprintLibrary::MakeStatUpdate(const FString& StatName, int32 Value, FString& OutKey, int32& OutValue)
{
	OutKey = StatName;
	OutValue = Value;
}


// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Result Helpers
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

bool UEPFBlueprintLibrary::WasSuccessful(const FEPFResult& Result)
{
	return Result.bSuccess;
}

FString UEPFBlueprintLibrary::GetErrorMessage(const FEPFResult& Result)
{
	return Result.ErrorMessage;
}
