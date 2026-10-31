// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Http/EFHttpMock.h"
#include "Platform/EFPlatformSubsystem.h"
#include "Platform/EPFPlatformProvider.h"
#include "Shared/EPFTypes.h"
#include "Testing/EFTestHelpers.h"
#include "Tests/EPFSharedAuthTestScope.h"

namespace EPFPlatformProviderTest
{
	FEFStatUpdate MakeStatUpdate(const TCHAR* ProviderId, int64 Value, EEFStatUpdateMode Mode)
	{
		FEFStatUpdate Update;
		Update.Id = FName(ProviderId);
		Update.ProviderId = ProviderId;
		Update.Value = Value;
		Update.Mode = Mode;
		return Update;
	}

	FEPFLeaderboardEntry MakePlayFabEntry(int32 Position, const TCHAR* EntityId, const TCHAR* Score)
	{
		FEPFLeaderboardEntry Entry;
		Entry.Position = Position;
		Entry.EntityId = EntityId;
		Entry.PlayFabId = EntityId;
		Entry.DisplayName = FString::Printf(TEXT("Player %d"), Position);
		Entry.Scores.Add(Score);
		Entry.StatValue = FCString::Atoi(Score);
		return Entry;
	}

	/** One provider callback: how often it ran and what it got last. */
	struct FAnswer
	{
		int32 Calls = 0;
		FEFResult Result;
		TArray<FEFStatValue> Stats;
		TArray<FEFLeaderboardEntry> Entries;
	};
}


// ═══════════════════════════════════════════════════════════════════════════════
// Conversions
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFPlatformStatUpdatesTest,
	"UnrealExtendedPlayFab.Platform.Conversions.StatUpdates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFPlatformStatUpdatesTest::RunTest(const FString& Parameters)
{
	using namespace EPFPlatformProviderTest;

	TestEqual(TEXT("values above int32 clamp to its maximum"), UEPFPlatformProvider::ClampToInt32(static_cast<int64>(5000000000LL)), MAX_int32);
	TestEqual(TEXT("values below int32 clamp to its minimum"), UEPFPlatformProvider::ClampToInt32(static_cast<int64>(-5000000000LL)), MIN_int32);
	TestEqual(TEXT("values in range are unchanged"), UEPFPlatformProvider::ClampToInt32(-42), -42);

	TArray<FEFStatUpdate> Updates;
	Updates.Add(MakeStatUpdate(TEXT("Kills"), 5, EEFStatUpdateMode::Set));
	Updates.Add(MakeStatUpdate(TEXT("Kills"), 3, EEFStatUpdateMode::Increment));
	Updates.Add(MakeStatUpdate(TEXT("Coins"), 10, EEFStatUpdateMode::Increment));
	Updates.Add(MakeStatUpdate(TEXT("Deaths"), 7, EEFStatUpdateMode::Increment));
	Updates.Add(MakeStatUpdate(TEXT("Deaths"), 2, EEFStatUpdateMode::Set));
	Updates.Add(MakeStatUpdate(TEXT("Distance"), 5000000000LL, EEFStatUpdateMode::Set));

	TMap<FString, int32> Values;
	FString Problem;
	if (!TestTrue(TEXT("a batch with provider ids converts"), UEPFPlatformProvider::MakeStatisticUpdates(Updates, Values, Problem)))
	{
		return false;
	}
	TestEqual(TEXT("one value per statistic"), Values.Num(), 4);
	TestEqual(TEXT("an Increment adds to the Set before it"), Values.FindRef(TEXT("Kills")), 8);
	TestEqual(TEXT("a lone Increment sends its value, for a Sum statistic to add"), Values.FindRef(TEXT("Coins")), 10);
	TestEqual(TEXT("a Set replaces the Increment before it"), Values.FindRef(TEXT("Deaths")), 2);
	TestEqual(TEXT("a 64-bit value is clamped to 32 bits"), Values.FindRef(TEXT("Distance")), MAX_int32);

	TArray<FEFStatUpdate> Unmapped;
	Unmapped.Add(MakeStatUpdate(TEXT(""), 1, EEFStatUpdateMode::Set));
	TestFalse(TEXT("an update without a provider id is rejected"), UEPFPlatformProvider::MakeStatisticUpdates(Unmapped, Values, Problem));
	TestFalse(TEXT("the rejection says why"), Problem.IsEmpty());

	TArray<FEPFStatistic> PlayFabStats;
	FEPFStatistic& Kills = PlayFabStats.AddDefaulted_GetRef();
	Kills.StatName = TEXT("Kills");
	Kills.Value = 12;
	Kills.Scores = { TEXT("12") };
	FEPFStatistic& Other = PlayFabStats.AddDefaulted_GetRef();
	Other.StatName = TEXT("Other");
	Other.Value = 1;

	const TArray<FEFStatValue> Requested = UEPFPlatformProvider::ToPlatformStats(PlayFabStats, { TEXT("Kills"), TEXT("NeverReported") });
	if (TestEqual(TEXT("only requested statistics PlayFab reported come back"), Requested.Num(), 1))
	{
		TestEqual(TEXT("the provider id is PlayFab's statistic name"), Requested[0].ProviderId, FString(TEXT("Kills")));
		TestEqual(TEXT("the value is the first score"), Requested[0].Value, static_cast<int64>(12));
	}
	TestEqual(TEXT("no ids asks for every statistic"), UEPFPlatformProvider::ToPlatformStats(PlayFabStats, TArray<FString>()).Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFPlatformLeaderboardEntriesTest,
	"UnrealExtendedPlayFab.Platform.Conversions.LeaderboardEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFPlatformLeaderboardEntriesTest::RunTest(const FString& Parameters)
{
	using namespace EPFPlatformProviderTest;

	// Out of order on purpose: the conversion puts the best first.
	TArray<FEPFLeaderboardEntry> PlayFabEntries;
	PlayFabEntries.Add(MakePlayFabEntry(2, TEXT("E2"), TEXT("800")));
	PlayFabEntries.Add(MakePlayFabEntry(1, TEXT("E1"), TEXT("9000000000")));
	PlayFabEntries.Add(MakePlayFabEntry(3, TEXT("E3"), TEXT("700")));
	PlayFabEntries.Add(MakePlayFabEntry(4, TEXT("E4"), TEXT("600")));
	PlayFabEntries.Add(MakePlayFabEntry(5, TEXT("E5"), TEXT("500")));

	const TArray<FEFLeaderboardEntry> Top = UEPFPlatformProvider::ToPlatformEntries(PlayFabEntries, TEXT("E4"), 2, false);
	if (TestEqual(TEXT("Top keeps Count entries"), Top.Num(), 2))
	{
		TestEqual(TEXT("best first"), Top[0].Rank, 1);
		TestEqual(TEXT("then the next rank"), Top[1].Rank, 2);
		TestEqual(TEXT("scores keep 64 bits"), Top[0].Score, static_cast<int64>(9000000000LL));
		TestEqual(TEXT("UserId is the entity id"), Top[0].UserId, FString(TEXT("E1")));
		TestFalse(TEXT("other players are not the local user"), Top[0].bIsLocalUser);
	}

	const TArray<FEFLeaderboardEntry> Around = UEPFPlatformProvider::ToPlatformEntries(PlayFabEntries, TEXT("E4"), 3, true);
	if (TestEqual(TEXT("AroundUser keeps Count entries"), Around.Num(), 3))
	{
		TestEqual(TEXT("the window ends at the last entry when the user is near the bottom"), Around[0].Rank, 3);
		TestTrue(TEXT("the local user is in the window and marked"), Around[1].bIsLocalUser && Around[1].Rank == 4);
		TestEqual(TEXT("the window is contiguous"), Around[2].Rank, 5);
	}

	const TArray<FEFLeaderboardEntry> All = UEPFPlatformProvider::ToPlatformEntries(PlayFabEntries, TEXT(""), 10, false);
	TestEqual(TEXT("fewer entries than Count are all kept"), All.Num(), 5);
	TestFalse(TEXT("no entity id marks nobody as the local user"), All.ContainsByPredicate([](const FEFLeaderboardEntry& Entry) { return Entry.bIsLocalUser; }));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFPlatformResultsTest,
	"UnrealExtendedPlayFab.Platform.Conversions.Results",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFPlatformResultsTest::RunTest(const FString& Parameters)
{
	const FName PlayFab(TEXT("PlayFab"));

	const FEFResult Success = UEPFPlatformProvider::ToPlatformResult(FEPFResult::Success(), PlayFab);
	TestTrue(TEXT("success stays success"), Success.bSuccess);
	TestEqual(TEXT("results come from the PlayFab provider"), Success.Source, PlayFab);

	const FEFResult SignedOut = UEPFPlatformProvider::ToPlatformResult(FEPFResult::Failure(TEXT("Missing entity token"), TEXT("MissingAuthentication")), PlayFab);
	TestEqual(TEXT("a missing session is NotSignedIn"), SignedOut.Code, FString(EFPlatformResultCodes::NotSignedIn));

	const FEFResult Missing = UEPFPlatformProvider::ToPlatformResult(FEPFResult::Failure(TEXT("No such leaderboard"), TEXT("LeaderboardNotFound")), PlayFab);
	TestEqual(TEXT("PlayFab's *NotFound errors are NotFound"), Missing.Code, FString(EFPlatformResultCodes::NotFound));
	TestTrue(TEXT("the PlayFab code stays in the message"), Missing.Message.StartsWith(TEXT("LeaderboardNotFound")));
	TestEqual(TEXT("failures come from the PlayFab provider"), Missing.Source, PlayFab);

	const FEFResult Unconfigured = UEPFPlatformProvider::ToPlatformResult(FEPFResult::Failure(TEXT("PlayFab TitleId is not configured"), TEXT("NotConfigured")), PlayFab);
	TestEqual(TEXT("no title id is NotAvailable"), Unconfigured.Code, FString(EFResultCodes::NotAvailable));

	const FEFResult NoCode = UEPFPlatformProvider::ToPlatformResult(FEPFResult::Failure(TEXT("EntityId cannot be empty")), PlayFab);
	TestEqual(TEXT("a failure without a code is Failed"), NoCode.Code, FString(EFResultCodes::Failed));

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════════
// Provider in a game instance
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFPlatformProviderFeaturesTest,
	"UnrealExtendedPlayFab.Platform.ProviderFeatures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFPlatformProviderFeaturesTest::RunTest(const FString& Parameters)
{
	// Declared first so it is destroyed last, after the game instance (see FEPFSharedAuthTestScope).
	FEPFSharedAuthTestScope PlayFab;
	PlayFab.ConfigureTitle(TEXT("EPFTEST"), TEXT("https://epf.test"));
	FEFScopedHttpMock Mock;
	EFTest::FScopedGameInstance Game;

	UEPFPlatformProvider* Provider = Game.GetSubsystem<UEPFPlatformProvider>();
	if (!TestNotNull(TEXT("the game instance has the PlayFab platform provider"), Provider))
	{
		return false;
	}

	TestEqual(TEXT("the provider is named PlayFab"), Provider->GetProviderName(), FName(TEXT("PlayFab")));
	if (const UEFPlatformSubsystem* Platform = Game.GetSubsystem<UEFPlatformSubsystem>())
	{
		TestTrue(TEXT("the provider registered with the platform subsystem"), Platform->FindProvider(TEXT("PlayFab")) == Provider);
	}

	TestFalse(TEXT("PlayFab has no achievements"), Provider->SupportsFeature(EEFPlatformFeature::Achievements));
	TestFalse(TEXT("PlayFab has no presence"), Provider->SupportsFeature(EEFPlatformFeature::Presence));
	TestFalse(TEXT("cloud storage is not served"), Provider->SupportsFeature(EEFPlatformFeature::CloudStorage));
	TestTrue(TEXT("stats are served"), Provider->SupportsFeature(EEFPlatformFeature::Stats));
	TestTrue(TEXT("leaderboards are served"), Provider->SupportsFeature(EEFPlatformFeature::Leaderboards));

	PlayFab.SignOut();
	TestFalse(TEXT("nothing is available before a login"), Provider->IsAnyFeatureAvailable());
	const FEFPlatformUser SignedOutUser = Provider->GetLocalUser();
	TestFalse(TEXT("no user before a login"), SignedOutUser.bSignedIn);
	TestEqual(TEXT("the user is reported by PlayFab"), SignedOutUser.Provider, FName(TEXT("PlayFab")));

	int32 Answers = 0;
	FEFResult Achievement;
	Provider->UnlockAchievement(TEXT("FirstBlood"), [&Answers, &Achievement](const FEFResult& Result) { ++Answers; Achievement = Result; });
	TestEqual(TEXT("UnlockAchievement answers once, at once"), Answers, 1);
	TestFalse(TEXT("UnlockAchievement fails"), Achievement.bSuccess);
	TestEqual(TEXT("UnlockAchievement answers NotAvailable"), Achievement.Code, FString(EFResultCodes::NotAvailable));
	TestEqual(TEXT("UnlockAchievement's answer comes from PlayFab"), Achievement.Source, FName(TEXT("PlayFab")));

	FEFResult Presence;
	Provider->SetPresence(TEXT("In a match"), TMap<FString, FString>(), [&Presence](const FEFResult& Result) { Presence = Result; });
	TestEqual(TEXT("SetPresence answers NotAvailable"), Presence.Code, FString(EFResultCodes::NotAvailable));

	FEFResult Cloud;
	Provider->WriteCloudFile(TEXT("save.sav"), TArray<uint8>(), [&Cloud](const FEFResult& Result) { Cloud = Result; });
	TestEqual(TEXT("WriteCloudFile answers NotAvailable"), Cloud.Code, FString(EFResultCodes::NotAvailable));

	PlayFab.SignIn(TEXT("test-session-ticket"), TEXT("TESTPLAYER"), TEXT("test-entity-token"), TEXT("ENTITY-LOCAL"));
	TestTrue(TEXT("stats are available once logged in with an entity"), Provider->IsFeatureAvailable(EEFPlatformFeature::Stats));
	TestTrue(TEXT("leaderboards are available once logged in with an entity"), Provider->IsFeatureAvailable(EEFPlatformFeature::Leaderboards));
	TestFalse(TEXT("achievements stay unavailable"), Provider->IsFeatureAvailable(EEFPlatformFeature::Achievements));

	const FEFPlatformUser SignedInUser = Provider->GetLocalUser();
	TestTrue(TEXT("the logged-in user is reported"), SignedInUser.bSignedIn);
	TestEqual(TEXT("the user id is the PlayFabId"), SignedInUser.UserId, FString(TEXT("TESTPLAYER")));

	PlayFab.SignIn(TEXT("test-session-ticket"), TEXT("TESTPLAYER"));
	TestFalse(TEXT("a session without an entity token cannot use Progression statistics"), Provider->IsFeatureAvailable(EEFPlatformFeature::Stats));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFPlatformProviderRoundTripTest,
	"UnrealExtendedPlayFab.Platform.StatsAndLeaderboardsOverHttp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFPlatformProviderRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace EPFPlatformProviderTest;

	struct FState
	{
		// Declared first so it is destroyed last, after the game instance (see FEPFSharedAuthTestScope).
		FEPFSharedAuthTestScope PlayFab;
		TSharedPtr<FEFScopedHttpMock> Mock;
		TSharedPtr<EFTest::FScopedGameInstance> Game;
		FAnswer Write;
		FAnswer Stats;
		FAnswer Top;
		FAnswer Friends;
	};
	const TSharedRef<FState> State = MakeShared<FState>();

	State->PlayFab.ConfigureTitle(TEXT("EPFTEST"), TEXT("https://epf.test"));
	State->Mock = MakeShared<FEFScopedHttpMock>();
	State->Game = MakeShared<EFTest::FScopedGameInstance>();

	UEPFPlatformProvider* Provider = State->Game->GetSubsystem<UEPFPlatformProvider>();
	if (!TestNotNull(TEXT("the game instance has the PlayFab platform provider"), Provider))
	{
		return false;
	}
	State->PlayFab.SignIn(TEXT("test-session-ticket"), TEXT("TESTPLAYER"), TEXT("test-entity-token"), TEXT("ENTITY-LOCAL"));

	FEFHttpMockTransport& HttpMock = State->Mock->Get();
	HttpMock.AddResponse(EEFHttpVerb::Post, TEXT("/Statistic/UpdateStatistics"), 200,
		TEXT("{\"code\":200,\"status\":\"OK\",\"data\":{}}"));
	HttpMock.AddResponse(EEFHttpVerb::Post, TEXT("/Statistic/GetStatistics"), 200,
		TEXT("{\"code\":200,\"status\":\"OK\",\"data\":{\"Statistics\":{\"Kills\":{\"Name\":\"Kills\",\"Scores\":[\"42\"],\"Version\":1}}}}"));
	HttpMock.AddResponse(EEFHttpVerb::Post, TEXT("/Leaderboard/GetLeaderboard"), 200,
		TEXT("{\"code\":200,\"status\":\"OK\",\"data\":{\"Rankings\":["
			"{\"Rank\":1,\"DisplayName\":\"Ace\",\"Entity\":{\"Id\":\"ENTITY-ACE\",\"Type\":\"title_player_account\"},\"Scores\":[\"900\"]},"
			"{\"Rank\":2,\"DisplayName\":\"Me\",\"Entity\":{\"Id\":\"ENTITY-LOCAL\",\"Type\":\"title_player_account\"},\"Scores\":[\"500\"]}]}}"));
	HttpMock.AddResponse(EEFHttpVerb::Post, TEXT("/Leaderboard/GetFriendLeaderboardForEntity"), 200,
		TEXT("{\"code\":400,\"status\":\"BadRequest\",\"error\":\"LeaderboardNotFound\",\"errorCode\":1,\"errorMessage\":\"No such leaderboard\"}"));

	// All four at once: two share the stats subsystem's delegates and two the leaderboard
	// subsystem's, so each answer has to find its own call.
	TArray<FEFStatUpdate> Updates;
	Updates.Add(MakeStatUpdate(TEXT("Kills"), 5000000000LL, EEFStatUpdateMode::Set));
	Provider->UpdateStats(Updates, [State](const FEFResult& Result) { ++State->Write.Calls; State->Write.Result = Result; });
	Provider->QueryStats({ TEXT("Kills") }, [State](const FEFResult& Result, const TArray<FEFStatValue>& Values)
	{
		++State->Stats.Calls;
		State->Stats.Result = Result;
		State->Stats.Stats = Values;
	});
	Provider->QueryLeaderboard(TEXT("HighScore"), EEFLeaderboardRange::Top, 10, [State](const FEFResult& Result, const TArray<FEFLeaderboardEntry>& Entries)
	{
		++State->Top.Calls;
		State->Top.Result = Result;
		State->Top.Entries = Entries;
	});
	Provider->QueryLeaderboard(TEXT("Missing"), EEFLeaderboardRange::Friends, 5, [State](const FEFResult& Result, const TArray<FEFLeaderboardEntry>& Entries)
	{
		++State->Friends.Calls;
		State->Friends.Result = Result;
		State->Friends.Entries = Entries;
	});
	TestEqual(TEXT("four calls wait for PlayFab"), Provider->GetPendingCallCount(), 4);

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("all four platform calls to answer"), [State]()
	{
		return State->Write.Calls > 0 && State->Stats.Calls > 0 && State->Top.Calls > 0 && State->Friends.Calls > 0;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		TestEqual(TEXT("UpdateStats answered once"), State->Write.Calls, 1);
		TestTrue(TEXT("UpdateStats succeeded"), State->Write.Result.bSuccess);
		TestEqual(TEXT("UpdateStats' answer comes from PlayFab"), State->Write.Result.Source, FName(TEXT("PlayFab")));

		TestEqual(TEXT("QueryStats answered once"), State->Stats.Calls, 1);
		TestTrue(TEXT("QueryStats succeeded"), State->Stats.Result.bSuccess);
		if (TestEqual(TEXT("QueryStats returned the requested stat"), State->Stats.Stats.Num(), 1))
		{
			TestEqual(TEXT("with its PlayFab name"), State->Stats.Stats[0].ProviderId, FString(TEXT("Kills")));
			TestEqual(TEXT("and PlayFab's value"), State->Stats.Stats[0].Value, static_cast<int64>(42));
		}

		TestEqual(TEXT("the Top query answered once"), State->Top.Calls, 1);
		TestTrue(TEXT("the Top query succeeded"), State->Top.Result.bSuccess);
		if (TestEqual(TEXT("the Top query returned both entries"), State->Top.Entries.Num(), 2))
		{
			TestEqual(TEXT("best first"), State->Top.Entries[0].Rank, 1);
			TestEqual(TEXT("with its score"), State->Top.Entries[0].Score, static_cast<int64>(900));
			TestEqual(TEXT("and display name"), State->Top.Entries[0].DisplayName, FString(TEXT("Ace")));
			TestTrue(TEXT("the local player's entry is marked"), State->Top.Entries[1].bIsLocalUser);
		}

		TestEqual(TEXT("the Friends query answered once"), State->Friends.Calls, 1);
		TestFalse(TEXT("the Friends query failed"), State->Friends.Result.bSuccess);
		TestEqual(TEXT("an unknown leaderboard is NotFound"), State->Friends.Result.Code, FString(EFPlatformResultCodes::NotFound));

		const TArray<FEFHttpMockRecordedRequest>& Requests = State->Mock->Get().GetRequests();
		const FEFHttpMockRecordedRequest* Update = Requests.FindByPredicate([](const FEFHttpMockRecordedRequest& Request)
		{
			return Request.Url.Contains(TEXT("/Statistic/UpdateStatistics"));
		});
		if (TestNotNull(TEXT("UpdateStats sent UpdateStatistics"), Update))
		{
			TestTrue(TEXT("the 64-bit value was clamped to int32"), Update->Body.Contains(TEXT("2147483647")));
			TestEqual(TEXT("statistics calls carry the entity token"), Update->Headers.FindRef(TEXT("X-EntityToken")), FString(TEXT("test-entity-token")));
		}
		TestTrue(TEXT("the Top query asked for the HighScore leaderboard"), Requests.ContainsByPredicate([](const FEFHttpMockRecordedRequest& Request)
		{
			return Request.Url.Contains(TEXT("/Leaderboard/GetLeaderboard")) && Request.Body.Contains(TEXT("\"HighScore\""));
		}));

		if (const UEPFPlatformProvider* ProviderAfter = State->Game->GetSubsystem<UEPFPlatformProvider>())
		{
			TestEqual(TEXT("no call is left waiting"), ProviderAfter->GetPendingCallCount(), 0);
		}

		State->Game.Reset();
		State->Mock.Reset();
		State->PlayFab.Restore();
	}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFPlatformProviderCancelTest,
	"UnrealExtendedPlayFab.Platform.PendingCallsCancelledAtShutdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFPlatformProviderCancelTest::RunTest(const FString& Parameters)
{
	using namespace EPFPlatformProviderTest;

	struct FState
	{
		FEPFSharedAuthTestScope PlayFab;
		TSharedPtr<FEFScopedHttpMock> Mock;
		TSharedPtr<EFTest::FScopedGameInstance> Game;
		FAnswer Stats;
		int32 FramesWaited = 0;
	};
	const TSharedRef<FState> State = MakeShared<FState>();

	State->PlayFab.ConfigureTitle(TEXT("EPFTEST"), TEXT("https://epf.test"));
	State->Mock = MakeShared<FEFScopedHttpMock>();
	State->Game = MakeShared<EFTest::FScopedGameInstance>();

	UEPFPlatformProvider* Provider = State->Game->GetSubsystem<UEPFPlatformProvider>();
	if (!TestNotNull(TEXT("the game instance has the PlayFab platform provider"), Provider))
	{
		return false;
	}
	State->PlayFab.SignIn(TEXT("test-session-ticket"), TEXT("TESTPLAYER"), TEXT("test-entity-token"), TEXT("ENTITY-LOCAL"));
	State->Mock->Get().AddResponse(EEFHttpVerb::Post, TEXT("/Statistic/GetStatistics"), 200,
		TEXT("{\"code\":200,\"status\":\"OK\",\"data\":{\"Statistics\":{}}}"));

	Provider->QueryStats({ TEXT("Kills") }, [State](const FEFResult& Result, const TArray<FEFStatValue>& Values)
	{
		++State->Stats.Calls;
		State->Stats.Result = Result;
	});
	TestEqual(TEXT("the query waits for PlayFab"), State->Stats.Calls, 0);

	// The game instance ends before the answer arrives (the mock answers on the next tick).
	State->Game.Reset();
	TestEqual(TEXT("the pending query was answered at shutdown"), State->Stats.Calls, 1);
	TestTrue(TEXT("with Cancelled"), State->Stats.Result.IsCancelled());
	TestEqual(TEXT("from PlayFab"), State->Stats.Result.Source, FName(TEXT("PlayFab")));

	// Let the late HTTP answer arrive; it must not answer the call a second time.
	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("a few frames to pass"), [State]()
	{
		return ++State->FramesWaited >= 5;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		TestEqual(TEXT("the late answer did not reach the cancelled call"), State->Stats.Calls, 1);
		State->Mock.Reset();
		State->PlayFab.Restore();
	}));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
