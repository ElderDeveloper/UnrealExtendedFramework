// Copyright Moon Punch Games. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../Analytics/EFAnalyticsSinks.h"
#include "Analytics/EFAnalyticsSettings.h"
#include "Analytics/EFAnalyticsSubsystem.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Http/EFHttpMock.h"
#include "Json/EFJsonLibrary.h"
#include "Messages/EFMessageSubsystem.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "NativeGameplayTags.h"
#include "Testing/EFTestHelpers.h"
#include "EFTestMessageTypes.h"

namespace EFAnalyticsTestPrivate
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Forwarded, "Extended.Test.Analytics");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(ForwardedKill, "Extended.Test.Analytics.Kill");

	/** Collects what it receives; can pretend not to be ready. */
	class FTestSink final : public IEFAnalyticsSink
	{
	public:
		virtual FName GetSinkName() const override { return TEXT("Test"); }
		virtual bool IsReady() const override { return bReady; }
		virtual void SendEvents(const TArray<FEFAnalyticsEvent>& Events) override
		{
			Received.Append(Events);
			++Batches;
		}
		virtual void DiscardPending() override { ++Discards; }

		const FEFAnalyticsEvent* Find(const FString& EventName) const
		{
			return Received.FindByPredicate([&EventName](const FEFAnalyticsEvent& Event) { return Event.Name == EventName; });
		}

		bool bReady = true;
		TArray<FEFAnalyticsEvent> Received;
		int32 Batches = 0;
		int32 Discards = 0;
	};

	/** Analytics settings for a test, restored afterwards. Real back-end sinks are kept out. */
	struct FScopedAnalyticsTestSettings
	{
		FScopedAnalyticsTestSettings(bool bRequireConsent, const FString& HttpEndpoint)
		{
			UEFAnalyticsSettings* Settings = GetMutableDefault<UEFAnalyticsSettings>();
			Saved.bEnabled = Settings->bEnabled;
			Saved.bRequireConsent = Settings->bRequireConsent;
			Saved.bRememberConsent = Settings->bRememberConsent;
			Saved.FlushIntervalSeconds = Settings->FlushIntervalSeconds;
			Saved.MaxBatchSize = Settings->MaxBatchSize;
			Saved.bRecordSessionEvents = Settings->bRecordSessionEvents;
			Saved.bIncludeBuildAttributes = Settings->bIncludeBuildAttributes;
			Saved.ForwardedMessageChannels = Settings->ForwardedMessageChannels;
			Saved.DisabledSinks = Settings->DisabledSinks;
			Saved.bLogSink = Settings->bLogSink;
			Saved.bFileSink = Settings->bFileSink;
			Saved.HttpEndpoint = Settings->HttpEndpoint;
			Saved.HttpHeaders = Settings->HttpHeaders;
			Saved.HttpMaxRetries = Settings->HttpMaxRetries;
			Saved.bPersistUnsentHttpBatches = Settings->bPersistUnsentHttpBatches;

			Settings->bEnabled = true;
			Settings->bRequireConsent = bRequireConsent;
			Settings->bRememberConsent = false;
			Settings->FlushIntervalSeconds = 3600.f;
			Settings->MaxBatchSize = 50;
			Settings->bRecordSessionEvents = true;
			Settings->bIncludeBuildAttributes = true;
			Settings->ForwardedMessageChannels = { Forwarded.GetTag() };
			Settings->DisabledSinks = { FName(TEXT("PlayFab")) };
			Settings->bLogSink = false;
			Settings->bFileSink = false;
			Settings->HttpEndpoint = HttpEndpoint;
			Settings->HttpHeaders.Reset();
			Settings->HttpMaxRetries = 0;
			Settings->bPersistUnsentHttpBatches = false;
		}

		~FScopedAnalyticsTestSettings()
		{
			UEFAnalyticsSettings* Settings = GetMutableDefault<UEFAnalyticsSettings>();
			Settings->bEnabled = Saved.bEnabled;
			Settings->bRequireConsent = Saved.bRequireConsent;
			Settings->bRememberConsent = Saved.bRememberConsent;
			Settings->FlushIntervalSeconds = Saved.FlushIntervalSeconds;
			Settings->MaxBatchSize = Saved.MaxBatchSize;
			Settings->bRecordSessionEvents = Saved.bRecordSessionEvents;
			Settings->bIncludeBuildAttributes = Saved.bIncludeBuildAttributes;
			Settings->ForwardedMessageChannels = Saved.ForwardedMessageChannels;
			Settings->DisabledSinks = Saved.DisabledSinks;
			Settings->bLogSink = Saved.bLogSink;
			Settings->bFileSink = Saved.bFileSink;
			Settings->HttpEndpoint = Saved.HttpEndpoint;
			Settings->HttpHeaders = Saved.HttpHeaders;
			Settings->HttpMaxRetries = Saved.HttpMaxRetries;
			Settings->bPersistUnsentHttpBatches = Saved.bPersistUnsentHttpBatches;
		}

		struct FSaved
		{
			bool bEnabled = true;
			bool bRequireConsent = false;
			bool bRememberConsent = true;
			float FlushIntervalSeconds = 15.f;
			int32 MaxBatchSize = 50;
			bool bRecordSessionEvents = true;
			bool bIncludeBuildAttributes = true;
			TArray<FGameplayTag> ForwardedMessageChannels;
			TArray<FName> DisabledSinks;
			bool bLogSink = false;
			bool bFileSink = false;
			FString HttpEndpoint;
			TMap<FString, FString> HttpHeaders;
			int32 HttpMaxRetries = 2;
			bool bPersistUnsentHttpBatches = true;
		} Saved;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFAnalyticsConsentTest, "ExtendedFoundation.Analytics.ConsentAndBatching",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFAnalyticsConsentTest::RunTest(const FString& Parameters)
{
	using namespace EFAnalyticsTestPrivate;

	FScopedAnalyticsTestSettings Settings(/*bRequireConsent*/ true, FString());
	EFTest::FScopedGameInstance Game;
	UEFAnalyticsSubsystem* Analytics = Game.GetSubsystem<UEFAnalyticsSubsystem>();
	UEFMessageSubsystem* Messages = Game.GetSubsystem<UEFMessageSubsystem>();
	if (!TestNotNull(TEXT("the analytics subsystem exists"), Analytics) || !TestNotNull(TEXT("the message subsystem exists"), Messages))
	{
		return false;
	}

	const TSharedRef<FTestSink> Sink = MakeShared<FTestSink>();
	Analytics->AddSink(Sink);
	TestTrue(TEXT("consent starts unknown when it is required"), Analytics->GetConsent() == EEFAnalyticsConsent::Unknown);
	TestFalse(TEXT("nothing is collected before consent"), Analytics->IsCollecting());

	Analytics->SetGlobalAttribute(TEXT("mode"), TEXT("test"));
	TMap<FString, FString> LevelAttributes;
	LevelAttributes.Add(TEXT("level"), TEXT("Intro"));
	Analytics->RecordEvent(TEXT("  level_started "), LevelAttributes);
	Analytics->Flush();
	TestEqual(TEXT("events wait for consent"), Sink->Received.Num(), 0);
	TestTrue(TEXT("but are shown to debug UI"), Analytics->GetRecentEvents().ContainsByPredicate([](const FEFAnalyticsEvent& Event) { return Event.Name == TEXT("level_started"); }));

	Analytics->SetConsent(EEFAnalyticsConsent::Granted);
	const FEFAnalyticsEvent* SessionStart = Sink->Find(TEXT("session_start"));
	const FEFAnalyticsEvent* Level = Sink->Find(TEXT("level_started"));
	if (TestNotNull(TEXT("granting consent delivers what waited, session_start included"), SessionStart) && TestNotNull(TEXT("and the trimmed level_started"), Level))
	{
		TestEqual(TEXT("event attributes are kept"), Level->Attributes.FindRef(TEXT("level")), FString(TEXT("Intro")));
		TestEqual(TEXT("global attributes are added"), Level->Attributes.FindRef(TEXT("mode")), FString(TEXT("test")));
		TestTrue(TEXT("build attributes are added"), Level->Attributes.Contains(TEXT("platform")) && Level->Attributes.Contains(TEXT("config")));
		TestEqual(TEXT("events carry the session id"), Level->SessionId, Analytics->GetSessionId());
		TestTrue(TEXT("sequence numbers count up"), SessionStart->Sequence < Level->Sequence);
	}

	// A message on a forwarded channel becomes an event named after the channel.
	FEFTestMessage Kill;
	Kill.Value = 3;
	Kill.Label = TEXT("orc");
	Messages->BroadcastMessage(ForwardedKill.GetTag(), Kill);
	Analytics->Flush();
	if (const FEFAnalyticsEvent* ForwardedEvent = Sink->Find(TEXT("Extended.Test.Analytics.Kill")))
	{
		TestEqual(TEXT("whole numbers are written without a fraction"), ForwardedEvent->Attributes.FindRef(TEXT("value")), FString(TEXT("3")));
		TestEqual(TEXT("text fields are kept"), ForwardedEvent->Attributes.FindRef(TEXT("label")), FString(TEXT("orc")));
	}
	else
	{
		AddError(TEXT("a message on a forwarded channel was not recorded"));
	}

	// A sink that is not ready keeps its events until it is.
	Sink->bReady = false;
	Analytics->RecordEvent(TEXT("waiting"), TMap<FString, FString>());
	Analytics->Flush();
	TestNull(TEXT("a sink that is not ready receives nothing"), Sink->Find(TEXT("waiting")));
	TestEqual(TEXT("its event waits"), Analytics->GetPendingEventCount(), 1);
	Sink->bReady = true;
	Analytics->Flush();
	TestNotNull(TEXT("it is delivered once the sink is ready"), Sink->Find(TEXT("waiting")));

	// Denying consent discards and stops collection.
	Analytics->SetConsent(EEFAnalyticsConsent::Denied);
	TestEqual(TEXT("sinks are told to discard what they hold"), Sink->Discards, 1);
	Analytics->RecordEvent(TEXT("after_denied"), TMap<FString, FString>());
	Analytics->Flush();
	TestNull(TEXT("nothing is sent after consent is denied"), Sink->Find(TEXT("after_denied")));

	AddExpectedMessage(TEXT("without a name"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, false);
	Analytics->RecordEvent(TEXT("   "), TMap<FString, FString>());

	Analytics->RemoveSink(TEXT("Test"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFAnalyticsHttpSinkTest, "ExtendedFoundation.Analytics.HttpSink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFAnalyticsHttpSinkTest::RunTest(const FString& Parameters)
{
	using namespace EFAnalyticsTestPrivate;

	struct FState
	{
		TSharedPtr<FScopedAnalyticsTestSettings> Settings;
		TSharedPtr<FEFScopedHttpMock> Mock;
		TSharedPtr<EFTest::FScopedGameInstance> Game;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	State->Settings = MakeShared<FScopedAnalyticsTestSettings>(/*bRequireConsent*/ false, TEXT("https://analytics.test/ingest"));
	State->Mock = MakeShared<FEFScopedHttpMock>();
	(*State->Mock)->AddResponse(EEFHttpVerb::Post, TEXT("/ingest"), 200, TEXT("{}"));
	State->Game = MakeShared<EFTest::FScopedGameInstance>();

	UEFAnalyticsSubsystem* Analytics = State->Game->GetSubsystem<UEFAnalyticsSubsystem>();
	if (!TestNotNull(TEXT("the analytics subsystem exists"), Analytics))
	{
		return false;
	}
	TestTrue(TEXT("the Http sink is created from the endpoint setting"), Analytics->GetSinks().ContainsByPredicate([](const TSharedRef<IEFAnalyticsSink>& Sink) { return Sink->GetSinkName() == TEXT("Http"); }));

	TMap<FString, FString> Attributes;
	Attributes.Add(TEXT("sku"), TEXT("gold_pack"));
	Analytics->RecordEvent(TEXT("purchase"), Attributes);
	Analytics->Flush();

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the analytics POST"), [State]() { return (*State->Mock)->CountRequests(TEXT("/ingest")) >= 1; }));
	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		const TArray<FEFHttpMockRecordedRequest>& Requests = (*State->Mock)->GetRequests();
		if (TestTrue(TEXT("one batch was posted"), Requests.Num() >= 1))
		{
			TestTrue(TEXT("as a POST"), Requests[0].Verb == EEFHttpVerb::Post);
			FEFJsonObject Body;
			TArray<FEFJsonObject> Events;
			const bool bParsed = UEFJsonLibrary::ParseJsonObject(Requests[0].Body, Body) && UEFJsonLibrary::TryGetObjectArrayField(Body, TEXT("events"), Events);
			if (TestTrue(TEXT("the body is {\"events\": [...]}"), bParsed))
			{
				const FEFJsonObject* Purchase = Events.FindByPredicate([](const FEFJsonObject& Event)
				{
					FString EventName;
					return UEFJsonLibrary::TryGetStringField(Event, TEXT("name"), EventName) && EventName == TEXT("purchase");
				});
				FEFJsonValue Sku;
				TestTrue(TEXT("the purchase event and its attribute are in the batch"),
					Purchase && UEFJsonLibrary::TryGetFieldByPath(*Purchase, TEXT("attributes.sku"), Sku) && Sku.Value.IsValid() && Sku.Value->AsString() == TEXT("gold_pack"));
			}
		}
		State->Game.Reset();
		State->Mock.Reset();
		State->Settings.Reset();
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFAnalyticsHttpPersistenceTest, "ExtendedFoundation.Analytics.HttpSinkPersistsUnsentBatches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFAnalyticsHttpPersistenceTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		TSharedPtr<FEFScopedHttpMock> Mock;
		TSharedPtr<FEFAnalyticsHttpSink> First;
		TSharedPtr<FEFAnalyticsHttpSink> Second;
		FString Folder;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	State->Folder = FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("EFAnalyticsPending"), FGuid::NewGuid().ToString());
	State->Mock = MakeShared<FEFScopedHttpMock>();
	(*State->Mock)->AddResponse(EEFHttpVerb::Post, TEXT("/ingest"), 503, TEXT("busy"), TMap<FString, FString>(), /*bOnce*/ true);
	(*State->Mock)->AddResponse(EEFHttpVerb::Post, TEXT("/ingest"), 200, TEXT("{}"));

	FEFAnalyticsHttpSink::FConfig Config;
	Config.Endpoint = TEXT("https://analytics.test/ingest");
	Config.MaxRetries = 0;
	Config.bPersistUnsent = true;
	Config.MaxPersistedBatches = 10;
	Config.PendingFolder = State->Folder;
	State->First = MakeShared<FEFAnalyticsHttpSink>(Config);

	FEFAnalyticsEvent Event;
	Event.Name = TEXT("offline_event");
	Event.Sequence = 1;
	State->First->SendEvents({ Event });

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the failed POST"), [State]() { return State->First->GetInFlightCount() == 0; }));
	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State, Config]()
	{
		TArray<FString> Saved;
		IFileManager::Get().FindFiles(Saved, *FPaths::Combine(State->Folder, TEXT("*.json")), true, false);
		TestEqual(TEXT("a batch that got a 503 is saved for later"), Saved.Num(), 1);

		// The next run sends what the last one saved.
		State->Second = MakeShared<FEFAnalyticsHttpSink>(Config);
		State->Second->ResendPersisted();
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the resent POST"), [State]() { return State->Second.IsValid() && State->Second->GetInFlightCount() == 0; }));
	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		TArray<FString> Saved;
		IFileManager::Get().FindFiles(Saved, *FPaths::Combine(State->Folder, TEXT("*.json")), true, false);
		TestEqual(TEXT("the saved batch is deleted once delivered"), Saved.Num(), 0);
		TestEqual(TEXT("it was posted twice in all"), (*State->Mock)->CountRequests(TEXT("/ingest")), 2);
		State->First.Reset();
		State->Second.Reset();
		State->Mock.Reset();
		IFileManager::Get().DeleteDirectory(*State->Folder, false, true);
	}));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
