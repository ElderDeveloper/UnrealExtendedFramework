// Copyright Moon Punch Games. All Rights Reserved.

#include "Analytics/EFAnalyticsSubsystem.h"

#include "Analytics/EFAnalyticsSettings.h"
#include "EFAnalyticsSinks.h"
#include "Dom/JsonObject.h"
#include "ExtendedFoundationModule.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "JsonObjectConverter.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Status/EFServiceStatus.h"
#include "Subsystems/EFSubsystems.h"

namespace EFAnalyticsPrivate
{
	constexpr int32 MaxRecentEvents = 100;
	constexpr int32 MaxEventNameLength = 128;

	int32 LogEvents = 0;
	FAutoConsoleVariableRef CVarLogEvents(
		TEXT("Extended.Analytics.Log"),
		LogEvents,
		TEXT("1 logs every analytics event as it is recorded, whether or not it is sent anywhere."));

	const TCHAR* ConsentToString(EEFAnalyticsConsent Consent)
	{
		switch (Consent)
		{
		case EEFAnalyticsConsent::Granted: return TEXT("Granted");
		case EEFAnalyticsConsent::Denied:  return TEXT("Denied");
		default:                           return TEXT("Unknown");
		}
	}

	/** A JSON value as one attribute: text as is, whole numbers without a fraction, objects and arrays as JSON. */
	FString JsonValueToAttribute(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return FString();
		}
		switch (Value->Type)
		{
		case EJson::String:
			return Value->AsString();
		case EJson::Boolean:
			return Value->AsBool() ? TEXT("true") : TEXT("false");
		case EJson::Number:
		{
			const double Number = Value->AsNumber();
			const double Rounded = FMath::RoundToDouble(Number);
			if (Number == Rounded && FMath::Abs(Number) < 9007199254740992.0)
			{
				return LexToString(static_cast<int64>(Rounded));
			}
			return FString::SanitizeFloat(Number);
		}
		case EJson::Object:
		case EJson::Array:
		{
			FString Text;
			const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
			FJsonSerializer::Serialize(Value, FString(), Writer);
			return Text;
		}
		default:
			return FString();
		}
	}

	template <typename TElement>
	void TrimOldest(TArray<TElement>& Array, int32 MaxCount)
	{
		if (MaxCount >= 0 && Array.Num() > MaxCount)
		{
			Array.RemoveAt(0, Array.Num() - MaxCount);
		}
	}
}

UEFAnalyticsSubsystem* UEFAnalyticsSubsystem::Get(const UObject* WorldContextObject)
{
	return EF::GetGameInstanceSubsystem<UEFAnalyticsSubsystem>(WorldContextObject);
}

FString UEFAnalyticsSubsystem::GetConsentFilePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExtendedAnalytics"), TEXT("Consent.txt"));
}

void UEFAnalyticsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UEFMessageSubsystem>();

	const UEFAnalyticsSettings* Settings = UEFAnalyticsSettings::Get();
	Consent = Settings->bRequireConsent ? EEFAnalyticsConsent::Unknown : EEFAnalyticsConsent::Granted;
	LoadConsent();

	SessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	if (Settings->bIncludeBuildAttributes)
	{
		GlobalAttributes.Add(TEXT("project"), FApp::GetProjectName());
		GlobalAttributes.Add(TEXT("build"), FApp::GetBuildVersion());
		GlobalAttributes.Add(TEXT("config"), LexToString(FApp::GetBuildConfiguration()));
		GlobalAttributes.Add(TEXT("platform"), FPlatformProperties::IniPlatformName());
		GlobalAttributes.Add(TEXT("engine"), FEngineVersion::Current().ToString(EVersionComponent::Patch));
	}

	CreateBuiltInSinks();
	BindForwardedMessages();

	FlushTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UEFAnalyticsSubsystem::TickFlush),
		FMath::Max(Settings->FlushIntervalSeconds, 0.5f));

	TWeakObjectPtr<UEFAnalyticsSubsystem> WeakThis(this);
	StatusHandle = FEFServiceStatusRegistry::Register(TEXT("Analytics"), [WeakThis]
	{
		FEFServiceStatus Status;
		const UEFAnalyticsSubsystem* This = WeakThis.Get();
		if (!This || !UEFAnalyticsSettings::Get()->bEnabled)
		{
			Status.State = EEFServiceState::Unavailable;
			Status.Summary = TEXT("disabled");
			return Status;
		}

		const int32 SinkCount = This->Sinks.Num();
		Status.State = SinkCount == 0 ? EEFServiceState::NotConfigured
			: This->IsCollecting() ? EEFServiceState::Ready : EEFServiceState::Degraded;
		Status.Summary = FString::Printf(TEXT("consent %s, %d sink(s), %d event(s) pending"),
			EFAnalyticsPrivate::ConsentToString(This->Consent), SinkCount, This->GetPendingEventCount());
		for (const FSinkEntry& Entry : This->Sinks)
		{
			if (Entry.Sink.IsValid())
			{
				const FString Description = Entry.Sink->DescribeStatus();
				Status.Details.Add(Entry.Sink->GetSinkName().ToString(), FString::Printf(TEXT("%s%s%s"),
					Entry.Sink->IsReady() ? TEXT("ready") : TEXT("not ready"),
					Entry.Waiting.Num() ? *FString::Printf(TEXT(", %d waiting"), Entry.Waiting.Num()) : TEXT(""),
					Description.IsEmpty() ? TEXT("") : *(TEXT("; ") + Description)));
			}
		}
		return Status;
	});

	StartSession();
}

void UEFAnalyticsSubsystem::Deinitialize()
{
	EndSession();

	FEFServiceStatusRegistry::Unregister(StatusHandle);
	StatusHandle = 0;
	if (FlushTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FlushTickerHandle);
		FlushTickerHandle.Reset();
	}
	for (FEFMessageListenerHandle& Handle : ForwardedMessageHandles)
	{
		Handle.Unregister();
	}
	ForwardedMessageHandles.Reset();
	Sinks.Reset();
	Super::Deinitialize();
}

// Session

void UEFAnalyticsSubsystem::StartSession()
{
	bSessionActive = true;
	SessionStartSeconds = FPlatformTime::Seconds();
	if (UEFAnalyticsSettings::Get()->bRecordSessionEvents)
	{
		RecordEvent(TEXT("session_start"), TMap<FString, FString>());
	}
}

void UEFAnalyticsSubsystem::EndSession()
{
	if (!bSessionActive)
	{
		return;
	}
	bSessionActive = false;

	if (UEFAnalyticsSettings::Get()->bRecordSessionEvents)
	{
		TMap<FString, FString> Attributes;
		Attributes.Add(TEXT("durationSeconds"), LexToString(FMath::RoundToInt64(FPlatformTime::Seconds() - SessionStartSeconds)));
		RecordEvent(TEXT("session_end"), Attributes);
	}

	const int32 AwaitingCount = AwaitingConsent.Num();
	if (AwaitingCount > 0)
	{
		UE_LOG(LogExtendedFoundation, Log, TEXT("Analytics: %d event(s) recorded without a consent decision were not sent."), AwaitingCount);
		AwaitingConsent.Reset();
	}
	DeliverBatch(/*bShuttingDown*/ true);
}

// Sinks

void UEFAnalyticsSubsystem::CreateBuiltInSinks()
{
	const UEFAnalyticsSettings* Settings = UEFAnalyticsSettings::Get();

	if (Settings->bLogSink)
	{
		AddSink(MakeShared<FEFAnalyticsLogSink>());
	}

	if (Settings->bFileSink)
	{
		const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), Settings->FileSinkFolder.IsEmpty() ? FString(TEXT("Analytics")) : Settings->FileSinkFolder);
		const FString FileName = FString::Printf(TEXT("%s_%s.jsonl"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")), *SessionId.Left(8));
		AddSink(MakeShared<FEFAnalyticsFileSink>(FPaths::Combine(Folder, FileName)));
	}

	if (!Settings->HttpEndpoint.IsEmpty())
	{
		FEFAnalyticsHttpSink::FConfig Config;
		Config.Endpoint = Settings->HttpEndpoint;
		Config.Headers = Settings->HttpHeaders;
		Config.TimeoutSeconds = Settings->HttpTimeoutSeconds;
		Config.MaxRetries = Settings->HttpMaxRetries;
		Config.bPersistUnsent = Settings->bPersistUnsentHttpBatches;
		Config.MaxPersistedBatches = Settings->MaxPersistedHttpBatches;
		Config.PendingFolder = FEFAnalyticsHttpSink::GetDefaultPendingFolder();

		const TSharedRef<FEFAnalyticsHttpSink> HttpSink = MakeShared<FEFAnalyticsHttpSink>(Config);
		AddSink(HttpSink);
		if (IsCollecting())
		{
			HttpSink->ResendPersisted();
		}
	}
}

bool UEFAnalyticsSubsystem::IsSinkDisabled(FName SinkName) const
{
	return UEFAnalyticsSettings::Get()->DisabledSinks.Contains(SinkName);
}

void UEFAnalyticsSubsystem::AddSink(TSharedRef<IEFAnalyticsSink> Sink)
{
	const FName SinkName = Sink->GetSinkName();
	if (IsSinkDisabled(SinkName))
	{
		UE_LOG(LogExtendedFoundation, Log, TEXT("Analytics: sink %s is listed in DisabledSinks; it is not used."), *SinkName.ToString());
		return;
	}

	RemoveSink(SinkName);
	FSinkEntry& Entry = Sinks.AddDefaulted_GetRef();
	Entry.Sink = Sink;
	UE_LOG(LogExtendedFoundation, Log, TEXT("Analytics: sink %s added."), *SinkName.ToString());
}

void UEFAnalyticsSubsystem::RemoveSink(FName SinkName)
{
	Sinks.RemoveAll([SinkName](const FSinkEntry& Entry) { return !Entry.Sink.IsValid() || Entry.Sink->GetSinkName() == SinkName; });
}

TArray<TSharedRef<IEFAnalyticsSink>> UEFAnalyticsSubsystem::GetSinks() const
{
	TArray<TSharedRef<IEFAnalyticsSink>> Result;
	for (const FSinkEntry& Entry : Sinks)
	{
		if (Entry.Sink.IsValid())
		{
			Result.Add(Entry.Sink.ToSharedRef());
		}
	}
	return Result;
}

int32 UEFAnalyticsSubsystem::GetPendingEventCount() const
{
	int32 Count = Batch.Num() + AwaitingConsent.Num();
	for (const FSinkEntry& Entry : Sinks)
	{
		Count += Entry.Waiting.Num();
	}
	return Count;
}

// Recording

void UEFAnalyticsSubsystem::RecordEvent(const FString& Name, const TMap<FString, FString>& Attributes)
{
	FString TrimmedName = Name.TrimStartAndEnd();
	if (TrimmedName.IsEmpty())
	{
		UE_LOG(LogExtendedFoundation, Warning, TEXT("Analytics: an event without a name was ignored."));
		return;
	}
	if (TrimmedName.Len() > EFAnalyticsPrivate::MaxEventNameLength)
	{
		TrimmedName.LeftInline(EFAnalyticsPrivate::MaxEventNameLength);
	}

	FEFAnalyticsEvent Event;
	Event.Name = MoveTemp(TrimmedName);
	Event.Attributes = GlobalAttributes;
	Event.Attributes.Append(Attributes);
	Event.TimestampUtc = FDateTime::UtcNow();
	Event.SessionId = SessionId;
	Event.UserId = UserId;
	Event.Sequence = NextSequence++;
	Enqueue(MoveTemp(Event));
}

void UEFAnalyticsSubsystem::RecordStructEvent(const FString& Name, const UScriptStruct* Struct, const void* StructData)
{
	TMap<FString, FString> Attributes;
	if (Struct && StructData)
	{
		const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		if (FJsonObjectConverter::UStructToJsonObject(Struct, StructData, Json))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Json->Values)
			{
				Attributes.Add(Pair.Key, EFAnalyticsPrivate::JsonValueToAttribute(Pair.Value));
			}
		}
	}
	RecordEvent(Name, Attributes);
}

void UEFAnalyticsSubsystem::Enqueue(FEFAnalyticsEvent&& Event)
{
	RecentEvents.Add(Event);
	EFAnalyticsPrivate::TrimOldest(RecentEvents, EFAnalyticsPrivate::MaxRecentEvents);
	if (EFAnalyticsPrivate::LogEvents != 0)
	{
		UE_LOG(LogExtendedFoundation, Display, TEXT("Analytics #%lld %s (%d attribute(s))"), Event.Sequence, *Event.Name, Event.Attributes.Num());
	}
	OnEventRecorded.Broadcast(Event);

	const UEFAnalyticsSettings* Settings = UEFAnalyticsSettings::Get();
	if (!Settings->bEnabled || Consent == EEFAnalyticsConsent::Denied)
	{
		return;
	}
	if (Settings->bRequireConsent && Consent == EEFAnalyticsConsent::Unknown)
	{
		AwaitingConsent.Add(MoveTemp(Event));
		EFAnalyticsPrivate::TrimOldest(AwaitingConsent, Settings->MaxEventsAwaitingConsent);
		return;
	}

	Batch.Add(MoveTemp(Event));
	if (Batch.Num() >= FMath::Max(Settings->MaxBatchSize, 1))
	{
		DeliverBatch(false);
	}
}

void UEFAnalyticsSubsystem::Flush()
{
	DeliverBatch(false);
	for (FSinkEntry& Entry : Sinks)
	{
		if (Entry.Sink.IsValid() && Entry.Sink->IsReady())
		{
			Entry.Sink->Flush(false);
		}
	}
}

bool UEFAnalyticsSubsystem::TickFlush(float DeltaTime)
{
	DeliverBatch(false);
	return true;
}

void UEFAnalyticsSubsystem::DeliverBatch(bool bShuttingDown)
{
	TArray<FEFAnalyticsEvent> ToSend = MoveTemp(Batch);
	Batch.Reset();

	// A sink may record events or add sinks while it sends; iterate over a stable count.
	const int32 SinkCount = Sinks.Num();
	for (int32 Index = 0; Index < SinkCount && Index < Sinks.Num(); ++Index)
	{
		DeliverToSink(Sinks[Index], ToSend, bShuttingDown);
	}
}

void UEFAnalyticsSubsystem::DeliverToSink(FSinkEntry& Entry, const TArray<FEFAnalyticsEvent>& NewEvents, bool bShuttingDown)
{
	const TSharedPtr<IEFAnalyticsSink> Sink = Entry.Sink;
	if (!Sink.IsValid())
	{
		return;
	}

	const UEFAnalyticsSettings* Settings = UEFAnalyticsSettings::Get();
	if (!Sink->IsReady())
	{
		Entry.Waiting.Append(NewEvents);
		const int32 Overflow = Entry.Waiting.Num() - FMath::Max(Settings->MaxEventsWaitingPerSink, 0);
		if (Overflow > 0)
		{
			UE_LOG(LogExtendedFoundation, Warning, TEXT("Analytics: sink %s is not ready; dropped its %d oldest waiting event(s)."), *Sink->GetSinkName().ToString(), Overflow);
			EFAnalyticsPrivate::TrimOldest(Entry.Waiting, FMath::Max(Settings->MaxEventsWaitingPerSink, 0));
		}
		if (bShuttingDown && Entry.Waiting.Num() > 0)
		{
			UE_LOG(LogExtendedFoundation, Log, TEXT("Analytics: sink %s never became ready; %d event(s) were not sent."), *Sink->GetSinkName().ToString(), Entry.Waiting.Num());
		}
		return;
	}

	TArray<FEFAnalyticsEvent> All = MoveTemp(Entry.Waiting);
	Entry.Waiting.Reset();
	All.Append(NewEvents);

	const int32 BatchSize = FMath::Max(Settings->MaxBatchSize, 1);
	for (int32 Start = 0; Start < All.Num(); Start += BatchSize)
	{
		const TArray<FEFAnalyticsEvent> Chunk(All.GetData() + Start, FMath::Min(BatchSize, All.Num() - Start));
		Sink->SendEvents(Chunk);
	}

	if (bShuttingDown)
	{
		Sink->Flush(true);
	}
}

// Consent

bool UEFAnalyticsSubsystem::IsCollecting() const
{
	const UEFAnalyticsSettings* Settings = UEFAnalyticsSettings::Get();
	return Settings->bEnabled && Consent != EEFAnalyticsConsent::Denied && (!Settings->bRequireConsent || Consent == EEFAnalyticsConsent::Granted);
}

void UEFAnalyticsSubsystem::SetConsent(EEFAnalyticsConsent NewConsent)
{
	if (NewConsent == Consent)
	{
		return;
	}
	Consent = NewConsent;
	UE_LOG(LogExtendedFoundation, Log, TEXT("Analytics: consent is now %s."), EFAnalyticsPrivate::ConsentToString(Consent));

	if (Consent == EEFAnalyticsConsent::Denied)
	{
		AwaitingConsent.Reset();
		Batch.Reset();
		for (FSinkEntry& Entry : Sinks)
		{
			Entry.Waiting.Reset();
			if (Entry.Sink.IsValid())
			{
				Entry.Sink->DiscardPending();
			}
		}
	}
	else if (Consent == EEFAnalyticsConsent::Granted && AwaitingConsent.Num() > 0)
	{
		Batch.Append(MoveTemp(AwaitingConsent));
		AwaitingConsent.Reset();
		DeliverBatch(false);
	}

	SaveConsent();
	OnConsentChanged.Broadcast(Consent);
}

void UEFAnalyticsSubsystem::LoadConsent()
{
	if (!UEFAnalyticsSettings::Get()->bRememberConsent)
	{
		return;
	}
	FString Saved;
	if (FFileHelper::LoadFileToString(Saved, *GetConsentFilePath()))
	{
		Saved.TrimStartAndEndInline();
		if (Saved.Equals(TEXT("Granted"), ESearchCase::IgnoreCase))
		{
			Consent = EEFAnalyticsConsent::Granted;
		}
		else if (Saved.Equals(TEXT("Denied"), ESearchCase::IgnoreCase))
		{
			Consent = EEFAnalyticsConsent::Denied;
		}
	}
}

void UEFAnalyticsSubsystem::SaveConsent() const
{
	if (!UEFAnalyticsSettings::Get()->bRememberConsent)
	{
		return;
	}
	if (Consent == EEFAnalyticsConsent::Unknown)
	{
		IFileManager::Get().Delete(*GetConsentFilePath(), false, true, true);
		return;
	}
	FFileHelper::SaveStringToFile(FString(EFAnalyticsPrivate::ConsentToString(Consent)), *GetConsentFilePath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

// Attributes

void UEFAnalyticsSubsystem::SetUserId(const FString& InUserId)
{
	UserId = InUserId;
}

void UEFAnalyticsSubsystem::SetGlobalAttribute(const FString& Key, const FString& Value)
{
	if (Key.IsEmpty())
	{
		return;
	}
	if (Value.IsEmpty())
	{
		GlobalAttributes.Remove(Key);
	}
	else
	{
		GlobalAttributes.Add(Key, Value);
	}
}

// Message forwarding

void UEFAnalyticsSubsystem::BindForwardedMessages()
{
	UEFMessageSubsystem* Messages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEFMessageSubsystem>() : nullptr;
	if (!Messages)
	{
		return;
	}

	TWeakObjectPtr<UEFAnalyticsSubsystem> WeakThis(this);
	for (const FGameplayTag& Channel : UEFAnalyticsSettings::Get()->ForwardedMessageChannels)
	{
		if (!Channel.IsValid())
		{
			continue;
		}
		ForwardedMessageHandles.Add(Messages->RegisterListener(Channel, [WeakThis](FGameplayTag ActualChannel, const FInstancedStruct& Payload)
		{
			if (UEFAnalyticsSubsystem* This = WeakThis.Get())
			{
				This->HandleForwardedMessage(ActualChannel, Payload);
			}
		}, EEFMessageMatch::PartialMatch, nullptr, this));
	}
}

void UEFAnalyticsSubsystem::HandleForwardedMessage(FGameplayTag Channel, const FInstancedStruct& Payload)
{
	RecordStructEvent(Channel.ToString(), Payload.GetScriptStruct(), Payload.GetMemory());
}
