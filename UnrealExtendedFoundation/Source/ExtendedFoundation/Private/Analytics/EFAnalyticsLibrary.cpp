// Copyright Moon Punch Games. All Rights Reserved.

#include "Analytics/EFAnalyticsLibrary.h"

#include "Analytics/EFAnalyticsSubsystem.h"
#include "StructUtils/InstancedStruct.h"

void UEFAnalyticsLibrary::RecordAnalyticsEvent(const UObject* WorldContextObject, const FString& Name, const TMap<FString, FString>& Attributes)
{
	if (UEFAnalyticsSubsystem* Analytics = UEFAnalyticsSubsystem::Get(WorldContextObject))
	{
		Analytics->RecordEvent(Name, Attributes);
	}
}

void UEFAnalyticsLibrary::RecordAnalyticsStructEvent(const UObject* WorldContextObject, const FString& Name, const FInstancedStruct& Payload)
{
	if (UEFAnalyticsSubsystem* Analytics = UEFAnalyticsSubsystem::Get(WorldContextObject))
	{
		Analytics->RecordStructEvent(Name, Payload.GetScriptStruct(), Payload.GetMemory());
	}
}

void UEFAnalyticsLibrary::SetAnalyticsConsent(const UObject* WorldContextObject, EEFAnalyticsConsent Consent)
{
	if (UEFAnalyticsSubsystem* Analytics = UEFAnalyticsSubsystem::Get(WorldContextObject))
	{
		Analytics->SetConsent(Consent);
	}
}

EEFAnalyticsConsent UEFAnalyticsLibrary::GetAnalyticsConsent(const UObject* WorldContextObject)
{
	const UEFAnalyticsSubsystem* Analytics = UEFAnalyticsSubsystem::Get(WorldContextObject);
	return Analytics ? Analytics->GetConsent() : EEFAnalyticsConsent::Unknown;
}

void UEFAnalyticsLibrary::SetAnalyticsUserId(const UObject* WorldContextObject, const FString& UserId)
{
	if (UEFAnalyticsSubsystem* Analytics = UEFAnalyticsSubsystem::Get(WorldContextObject))
	{
		Analytics->SetUserId(UserId);
	}
}

void UEFAnalyticsLibrary::SetAnalyticsGlobalAttribute(const UObject* WorldContextObject, const FString& Key, const FString& Value)
{
	if (UEFAnalyticsSubsystem* Analytics = UEFAnalyticsSubsystem::Get(WorldContextObject))
	{
		Analytics->SetGlobalAttribute(Key, Value);
	}
}

void UEFAnalyticsLibrary::FlushAnalytics(const UObject* WorldContextObject)
{
	if (UEFAnalyticsSubsystem* Analytics = UEFAnalyticsSubsystem::Get(WorldContextObject))
	{
		Analytics->Flush();
	}
}

TArray<FEFAnalyticsEvent> UEFAnalyticsLibrary::GetRecentAnalyticsEvents(const UObject* WorldContextObject)
{
	const UEFAnalyticsSubsystem* Analytics = UEFAnalyticsSubsystem::Get(WorldContextObject);
	return Analytics ? Analytics->GetRecentEvents() : TArray<FEFAnalyticsEvent>();
}
