// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Analytics/EFAnalyticsSettings.h"
#include "Analytics/EFAnalyticsSubsystem.h"
#include "Analytics/EPFAnalyticsSinkSubsystem.h"
#include "Analytics/EPFAnalyticsSubsystem.h"
#include "Dom/JsonObject.h"
#include "Http/EFHttpMock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/EPFSettings.h"
#include "Testing/EFTestHelpers.h"
#include "Tests/EPFSharedAuthTestScope.h"

namespace EPFAnalyticsSinkTest
{
	FEFAnalyticsEvent MakeEvent(const TCHAR* Name, int64 Sequence)
	{
		FEFAnalyticsEvent Event;
		Event.Name = Name;
		Event.Attributes.Add(TEXT("enemy"), TEXT("Orc"));
		Event.TimestampUtc = FDateTime(2026, 10, 31, 12, 0, 0);
		Event.SessionId = TEXT("session-1");
		Event.Sequence = Sequence;
		return Event;
	}

	/** The first event of a recorded WriteTelemetryEvents body, or null. */
	TSharedPtr<FJsonObject> FirstWrittenEvent(const FString& RequestBody)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestBody);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return nullptr;
		}
		const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
		const TSharedPtr<FJsonObject>* First = nullptr;
		if (!Root->TryGetArrayField(TEXT("Events"), Events) || !Events || Events->Num() == 0 || !(*Events)[0].IsValid() || !(*Events)[0]->TryGetObject(First) || !First)
		{
			return nullptr;
		}
		return *First;
	}
}


// ═══════════════════════════════════════════════════════════════════════════════
// Event names and payload
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFAnalyticsSinkNamesTest,
	"UnrealExtendedPlayFab.AnalyticsSink.SanitizesEventNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFAnalyticsSinkNamesTest::RunTest(const FString& Parameters)
{
	using namespace EPFAnalyticsSinkTest;

	TestEqual(TEXT("a valid name is unchanged"), FEPFAnalyticsSink::SanitizeEventName(TEXT("level_started")), FString(TEXT("level_started")));
	TestEqual(TEXT("a message channel's dots become underscores"), FEPFAnalyticsSink::SanitizeEventName(TEXT("Game.Combat.EnemyKilled")), FString(TEXT("Game_Combat_EnemyKilled")));
	TestEqual(TEXT("runs collapse and the ends are trimmed"), FEPFAnalyticsSink::SanitizeEventName(TEXT("  Player - Died!  ")), FString(TEXT("Player_Died")));
	TestEqual(TEXT("an existing underscore is not doubled"), FEPFAnalyticsSink::SanitizeEventName(TEXT("café_open")), FString(TEXT("caf_open")));
	TestEqual(TEXT("letters outside ASCII are not kept"), FEPFAnalyticsSink::SanitizeEventName(TEXT("été")), FString(TEXT("t")));
	TestEqual(TEXT("leading underscores the game chose are kept"), FEPFAnalyticsSink::SanitizeEventName(TEXT("_debug")), FString(TEXT("_debug")));
	TestTrue(TEXT("nothing usable gives no name"), FEPFAnalyticsSink::SanitizeEventName(TEXT("!!! ???")).IsEmpty());
	TestTrue(TEXT("only underscores gives no name"), FEPFAnalyticsSink::SanitizeEventName(TEXT("___")).IsEmpty());

	const FString Long = FString::ChrN(100, TEXT('a'));
	TestEqual(TEXT("names are cut to PlayFab's limit"), FEPFAnalyticsSink::SanitizeEventName(Long).Len(), FEPFAnalyticsSink::MaxEventNameLength);

	const FEPFTelemetryEventData Renamed = FEPFAnalyticsSink::ToTelemetryEvent(MakeEvent(TEXT("Game.Combat.EnemyKilled"), 7));
	TestEqual(TEXT("the event goes out under the sanitized name"), Renamed.EventName, FString(TEXT("Game_Combat_EnemyKilled")));
	TestEqual(TEXT("the original name rides along when it changed"), Renamed.PayloadFields.FindRef(TEXT("original_name")), FString(TEXT("Game.Combat.EnemyKilled")));
	TestEqual(TEXT("attributes become the properties"), Renamed.Properties.FindRef(TEXT("enemy")), FString(TEXT("Orc")));
	TestEqual(TEXT("the session id is sent"), Renamed.PayloadFields.FindRef(TEXT("session_id")), FString(TEXT("session-1")));
	TestEqual(TEXT("the sequence number is sent"), Renamed.PayloadFields.FindRef(TEXT("sequence")), FString(TEXT("7")));
	TestTrue(TEXT("the event's own time is kept"), Renamed.TimestampUtc == FDateTime(2026, 10, 31, 12, 0, 0));

	const FEPFTelemetryEventData Kept = FEPFAnalyticsSink::ToTelemetryEvent(MakeEvent(TEXT("level_started"), 1));
	TestFalse(TEXT("no original_name when the name was already valid"), Kept.PayloadFields.Contains(TEXT("original_name")));

	const FEPFTelemetryEventData Unnamed = FEPFAnalyticsSink::ToTelemetryEvent(MakeEvent(TEXT("???"), 2));
	TestEqual(TEXT("a name with nothing usable is sent as unnamed_event"), Unnamed.EventName, FString(TEXT("unnamed_event")));

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════════
// Registration and delivery
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFAnalyticsSinkRegistrationTest,
	"UnrealExtendedPlayFab.AnalyticsSink.RegisteredPerGameInstance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFAnalyticsSinkRegistrationTest::RunTest(const FString& Parameters)
{
	// Declared first so it is destroyed last, after the game instances (see FEPFSharedAuthTestScope).
	FEPFSharedAuthTestScope PlayFab;
	PlayFab.ConfigureTitle(TEXT("EPFTEST"), TEXT("https://epf.test"));
	FEFScopedHttpMock Mock;

	GetMutableDefault<UEPFSettings>()->bSendAnalyticsEvents = false;
	{
		EFTest::FScopedGameInstance Game;
		TestNull(TEXT("no sink subsystem with Send Analytics Events off"), Game.GetSubsystem<UEPFAnalyticsSinkSubsystem>());
	}

	GetMutableDefault<UEPFSettings>()->bSendAnalyticsEvents = true;
	EFTest::FScopedGameInstance Game;
	const UEPFAnalyticsSinkSubsystem* SinkSubsystem = Game.GetSubsystem<UEPFAnalyticsSinkSubsystem>();
	if (!TestNotNull(TEXT("the sink subsystem exists with Send Analytics Events on"), SinkSubsystem))
	{
		return false;
	}

	const TSharedPtr<FEPFAnalyticsSink> Sink = SinkSubsystem->GetSink();
	if (!TestTrue(TEXT("the sink subsystem made a sink"), Sink.IsValid()))
	{
		return false;
	}
	TestEqual(TEXT("the sink is named PlayFab"), Sink->GetSinkName(), FName(TEXT("PlayFab")));
	PlayFab.SignOut();
	TestFalse(TEXT("the sink is not ready before a login"), Sink->IsReady());

	const UEFAnalyticsSubsystem* Analytics = Game.GetSubsystem<UEFAnalyticsSubsystem>();
	if (TestNotNull(TEXT("the game instance has the analytics subsystem"), Analytics)
		&& !GetDefault<UEFAnalyticsSettings>()->DisabledSinks.Contains(FName(TEXT("PlayFab"))))
	{
		const TArray<TSharedRef<IEFAnalyticsSink>> Sinks = Analytics->GetSinks();
		TestTrue(TEXT("the sink was added to the analytics subsystem"), Sinks.ContainsByPredicate([&Sink](const TSharedRef<IEFAnalyticsSink>& Added)
		{
			return &Added.Get() == static_cast<IEFAnalyticsSink*>(Sink.Get());
		}));
	}

	PlayFab.SignIn(TEXT("test-session-ticket"), TEXT("TESTPLAYER"), TEXT("test-entity-token"), TEXT("ENTITY-LOCAL"));
	TestTrue(TEXT("the sink is ready once logged in with an entity"), Sink->IsReady());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFAnalyticsSinkDeliveryTest,
	"UnrealExtendedPlayFab.AnalyticsSink.SendsTelemetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFAnalyticsSinkDeliveryTest::RunTest(const FString& Parameters)
{
	using namespace EPFAnalyticsSinkTest;

	struct FState
	{
		// Declared first so it is destroyed last, after the game instance (see FEPFSharedAuthTestScope).
		FEPFSharedAuthTestScope PlayFab;
		TSharedPtr<FEFScopedHttpMock> Mock;
		TSharedPtr<EFTest::FScopedGameInstance> Game;
		TSharedPtr<FEPFAnalyticsSink> Sink;
	};
	const TSharedRef<FState> State = MakeShared<FState>();

	// The sink subsystem stays off (ConfigureTitle), so the only telemetry is this test's own sink.
	State->PlayFab.ConfigureTitle(TEXT("EPFTEST"), TEXT("https://epf.test"));
	State->Mock = MakeShared<FEFScopedHttpMock>();
	State->Game = MakeShared<EFTest::FScopedGameInstance>();

	UEPFAnalyticsSubsystem* PlayFabAnalytics = State->Game->GetSubsystem<UEPFAnalyticsSubsystem>();
	if (!TestNotNull(TEXT("the game instance has the PlayFab analytics subsystem"), PlayFabAnalytics))
	{
		return false;
	}
	State->Sink = MakeShared<FEPFAnalyticsSink>(PlayFabAnalytics);
	State->PlayFab.SignOut();
	TestFalse(TEXT("the sink waits for a login"), State->Sink->IsReady());

	State->PlayFab.SignIn(TEXT("test-session-ticket"), TEXT("TESTPLAYER"), TEXT("test-entity-token"), TEXT("ENTITY-LOCAL"));
	TestTrue(TEXT("the sink is ready once logged in"), State->Sink->IsReady());

	// The first batch is accepted, the second rejected for good.
	State->Mock->Get().AddResponse(EEFHttpVerb::Post, TEXT("/Event/WriteTelemetryEvents"), 200,
		TEXT("{\"code\":200,\"status\":\"OK\",\"data\":{\"AssignedEventIds\":[\"evt-1\"]}}"), {}, /*bOnce*/ true);
	State->Mock->Get().AddResponse(EEFHttpVerb::Post, TEXT("/Event/WriteTelemetryEvents"), 200,
		TEXT("{\"code\":400,\"status\":\"BadRequest\",\"error\":\"InvalidParams\",\"errorCode\":1000,\"errorMessage\":\"Invalid event\"}"));

	State->Sink->SendEvents({ MakeEvent(TEXT("Game.Combat.EnemyKilled"), 7) });

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the first batch to be accepted"), [State]()
	{
		return State->Sink->GetSentEventCount() > 0;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		TestEqual(TEXT("one event was sent"), State->Sink->GetSentEventCount(), static_cast<int64>(1));
		TestEqual(TEXT("nothing waits for a retry"), State->Sink->GetHeldEventCount(), 0);

		const TArray<FEFHttpMockRecordedRequest>& Requests = State->Mock->Get().GetRequests();
		const FEFHttpMockRecordedRequest* Write = Requests.FindByPredicate([](const FEFHttpMockRecordedRequest& Request)
		{
			return Request.Url.Contains(TEXT("/Event/WriteTelemetryEvents"));
		});
		if (!TestNotNull(TEXT("the sink called WriteTelemetryEvents"), Write))
		{
			return;
		}
		TestEqual(TEXT("telemetry carries the entity token"), Write->Headers.FindRef(TEXT("X-EntityToken")), FString(TEXT("test-entity-token")));

		const TSharedPtr<FJsonObject> Event = FirstWrittenEvent(Write->Body);
		if (!TestTrue(TEXT("the request body holds an event"), Event.IsValid()))
		{
			return;
		}
		TestEqual(TEXT("the event name is sanitized"), Event->GetStringField(TEXT("Name")), FString(TEXT("Game_Combat_EnemyKilled")));
		TestTrue(TEXT("the event keeps its own time"), Event->GetStringField(TEXT("OriginalTimestamp")).StartsWith(TEXT("2026-10-31T12:00:00")));

		const TSharedPtr<FJsonObject>* Payload = nullptr;
		if (TestTrue(TEXT("the event has a payload"), Event->TryGetObjectField(TEXT("Payload"), Payload) && Payload))
		{
			TestEqual(TEXT("event_source marks the Extended analytics layer"), (*Payload)->GetStringField(TEXT("event_source")), FString(TEXT("extended")));
			TestEqual(TEXT("the session id is in the payload"), (*Payload)->GetStringField(TEXT("session_id")), FString(TEXT("session-1")));
			TestEqual(TEXT("the original name is in the payload"), (*Payload)->GetStringField(TEXT("original_name")), FString(TEXT("Game.Combat.EnemyKilled")));
			const TSharedPtr<FJsonObject>* Properties = nullptr;
			if (TestTrue(TEXT("the attributes are the properties"), (*Payload)->TryGetObjectField(TEXT("properties"), Properties) && Properties))
			{
				TestEqual(TEXT("an attribute arrives as sent"), (*Properties)->GetStringField(TEXT("enemy")), FString(TEXT("Orc")));
			}
		}

		State->Sink->SendEvents({ MakeEvent(TEXT("level_started"), 8) });
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the rejected batch to be dropped"), [State]()
	{
		return State->Sink->GetDroppedEventCount() > 0;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		TestEqual(TEXT("a batch PlayFab rejects is dropped, not retried"), State->Sink->GetDroppedEventCount(), static_cast<int64>(1));
		TestEqual(TEXT("so nothing waits for a retry"), State->Sink->GetHeldEventCount(), 0);
		TestEqual(TEXT("the accepted count is unchanged"), State->Sink->GetSentEventCount(), static_cast<int64>(1));

		State->Sink->Shutdown();
		State->Sink.Reset();
		State->Game.Reset();
		State->Mock.Reset();
		State->PlayFab.Restore();
	}));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
