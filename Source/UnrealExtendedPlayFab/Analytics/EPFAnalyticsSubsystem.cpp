// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAnalyticsSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

void UEPFAnalyticsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EventsLoggedCount = 0;
}

void UEPFAnalyticsSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UEPFAnalyticsSubsystem::LogEvent(const FString& EventName, const TMap<FString, FString>& Body)
{
	TSharedRef<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("EventName"), EventName);

	TSharedRef<FJsonObject> BodyObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Body)
	{
		BodyObj->SetStringField(Pair.Key, Pair.Value);
	}
	RequestBody->SetObjectField(TEXT("Body"), BodyObj);

	SendPlayFabRequestDetailed(
		TEXT("/Client/WritePlayerEvent"),
		RequestBody,
		true,
		FOnPlayFabResponseDetailed::CreateLambda([this, EventName](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess)
			{
				EventsLoggedCount++;
				UE_LOG(LogExtendedPlayFab, Verbose, TEXT("EPFAnalyticsSubsystem — Event logged: %s"), *EventName);
			}
			else
			{
				UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFAnalyticsSubsystem — Failed to log event: %s"), *EventName);
			}
			OnEventLogged.Broadcast(Result);
		})
	);
}

void UEPFAnalyticsSubsystem::LogSimpleEvent(const FString& EventName)
{
	TMap<FString, FString> EmptyBody;
	LogEvent(EventName, EmptyBody);
}

void UEPFAnalyticsSubsystem::LogEvents(const TArray<FEPFAnalyticsEvent>& Events)
{
	for (const FEPFAnalyticsEvent& Event : Events)
	{
		LogEvent(Event.EventName, Event.Body);
	}
}

int32 UEPFAnalyticsSubsystem::GetEventsLoggedCount() const
{
	return EventsLoggedCount;
}

void UEPFAnalyticsSubsystem::LogCharacterEvent(const FString& CharacterId, const FString& EventName, const TMap<FString, FString>& Body)
{
	if (CharacterId.IsEmpty() || EventName.IsEmpty()) { OnEventLogged.Broadcast(FEPFResult::Failure(TEXT("CharacterId and EventName are required"))); return; }

	TSharedRef<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("CharacterId"), CharacterId);
	RequestBody->SetStringField(TEXT("EventName"), EventName);

	TSharedRef<FJsonObject> BodyObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Body) BodyObj->SetStringField(Pair.Key, Pair.Value);
	RequestBody->SetObjectField(TEXT("Body"), BodyObj);

	SendPlayFabRequestDetailed(TEXT("/Client/WriteCharacterEvent"), RequestBody, true,
		FOnPlayFabResponseDetailed::CreateLambda([this, EventName](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) { EventsLoggedCount++; UE_LOG(LogExtendedPlayFab, Verbose, TEXT("EPFAnalytics — Character event: %s"), *EventName); }
			OnEventLogged.Broadcast(Result);
		}));
}

void UEPFAnalyticsSubsystem::LogTitleEvent(const FString& EventName, const TMap<FString, FString>& Body)
{
	if (EventName.IsEmpty()) { OnEventLogged.Broadcast(FEPFResult::Failure(TEXT("EventName cannot be empty"))); return; }

	TSharedRef<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("EventName"), EventName);

	TSharedRef<FJsonObject> BodyObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Body) BodyObj->SetStringField(Pair.Key, Pair.Value);
	RequestBody->SetObjectField(TEXT("Body"), BodyObj);

	SendPlayFabRequestDetailed(TEXT("/Client/WriteTitleEvent"), RequestBody, true,
		FOnPlayFabResponseDetailed::CreateLambda([this, EventName](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) { EventsLoggedCount++; UE_LOG(LogExtendedPlayFab, Verbose, TEXT("EPFAnalytics — Title event: %s"), *EventName); }
			OnEventLogged.Broadcast(Result);
		}));
}

void UEPFAnalyticsSubsystem::ReportDeviceInfo()
{
	TSharedRef<FJsonObject> RequestBody = MakeShared<FJsonObject>();

	TSharedRef<FJsonObject> InfoObj = MakeShared<FJsonObject>();
	InfoObj->SetStringField(TEXT("OS"), FPlatformMisc::GetOSVersion());
	InfoObj->SetStringField(TEXT("Device"), FPlatformProperties::PlatformName());
	RequestBody->SetObjectField(TEXT("Info"), InfoObj);

	SendPlayFabRequestDetailed(TEXT("/Client/ReportDeviceInfo"), RequestBody, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAnalytics — Device info reported"));
			OnEventLogged.Broadcast(Result);
		}));
}

