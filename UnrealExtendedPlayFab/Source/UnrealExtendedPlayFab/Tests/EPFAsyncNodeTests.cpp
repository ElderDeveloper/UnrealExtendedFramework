// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "BlueprintActions/SharedData/EPFAsyncSharedData.h"
#include "Http/EFHttpMock.h"
#include "Shared/EPFSettings.h"
#include "Shared/EPFSubsystem.h"
#include "SharedData/EPFSharedDataSubsystem.h"
#include "Testing/EFTestHelpers.h"
#include "Tests/EPFTestAsyncListener.h"
#include "UObject/StrongObjectPtr.h"

/**
 * Gives a test a configured PlayFab title and a signed-in session without a real login, and puts
 * back the project's settings and the previous session when restored or destroyed.
 *
 * The session lives in UEPFSubsystem's protected statics, so this is a friend of that class. Restore
 * only after the test game instance has shut down: its auth subsystem clears the session then.
 */
struct FEPFSharedAuthTestScope
{
	FEPFSharedAuthTestScope()
	{
		const UEPFSettings* Settings = GetDefault<UEPFSettings>();
		SavedTitleId = Settings->TitleId;
		SavedApiBaseUrlOverride = Settings->ApiBaseUrlOverride;
		bSavedAutoLoginOnStart = Settings->bAutoLoginOnStart;
		bSavedAutoAnalyticsEnabled = Settings->bAutoAnalyticsEnabled;

		SavedAuthContext = UEPFSubsystem::SharedAuthContext;
		SavedSessionTicket = UEPFSubsystem::SharedSessionTicket;
		SavedPlayFabId = UEPFSubsystem::SharedPlayFabId;
	}

	~FEPFSharedAuthTestScope()
	{
		Restore();
	}

	FEPFSharedAuthTestScope(const FEPFSharedAuthTestScope&) = delete;
	FEPFSharedAuthTestScope& operator=(const FEPFSharedAuthTestScope&) = delete;

	/** Call before creating the game instance, so its subsystems start with these settings. */
	void ConfigureTitle(const FString& InTitleId, const FString& InApiBaseUrl)
	{
		UEPFSettings* Settings = GetMutableDefault<UEPFSettings>();
		Settings->TitleId = InTitleId;
		Settings->ApiBaseUrlOverride = InApiBaseUrl;
		// Keep the test game instance quiet: no sign-in attempt and no telemetry of its own.
		Settings->bAutoLoginOnStart = false;
		Settings->bAutoAnalyticsEnabled = false;
	}

	/** What a successful login would leave behind, as far as session-ticket calls are concerned. */
	void SignIn(const FString& InSessionTicket, const FString& InPlayFabId)
	{
		FEPFAuthContext Context;
		Context.SessionTicket = InSessionTicket;
		Context.PlayFabId = InPlayFabId;
		UEPFSubsystem::SharedAuthContext = Context;
		UEPFSubsystem::SharedSessionTicket = InSessionTicket;
		UEPFSubsystem::SharedPlayFabId = InPlayFabId;
	}

	void Restore()
	{
		if (bRestored)
		{
			return;
		}
		bRestored = true;

		UEPFSettings* Settings = GetMutableDefault<UEPFSettings>();
		Settings->TitleId = SavedTitleId;
		Settings->ApiBaseUrlOverride = SavedApiBaseUrlOverride;
		Settings->bAutoLoginOnStart = bSavedAutoLoginOnStart;
		Settings->bAutoAnalyticsEnabled = bSavedAutoAnalyticsEnabled;

		UEPFSubsystem::SharedAuthContext = SavedAuthContext;
		UEPFSubsystem::SharedSessionTicket = SavedSessionTicket;
		UEPFSubsystem::SharedPlayFabId = SavedPlayFabId;
	}

private:
	FString SavedTitleId;
	FString SavedApiBaseUrlOverride;
	bool bSavedAutoLoginOnStart = false;
	bool bSavedAutoAnalyticsEnabled = false;
	FEPFAuthContext SavedAuthContext;
	FString SavedSessionTicket;
	FString SavedPlayFabId;
	bool bRestored = false;
};


// ═══════════════════════════════════════════════════════════════════════════════
// Async nodes — two identical nodes in flight each finish on their own result
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFAsyncConcurrentNodesTest,
	"UnrealExtendedPlayFab.Async.ConcurrentNodesGetTheirOwnResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFAsyncConcurrentNodesTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		// Declared first so it is destroyed last, after the game instance (see FEPFSharedAuthTestScope).
		FEPFSharedAuthTestScope PlayFab;
		TSharedPtr<FEFScopedHttpMock> Mock;
		TSharedPtr<EFTest::FScopedGameInstance> Game;
		TStrongObjectPtr<UEPFTestAsyncListener> PinsA;
		TStrongObjectPtr<UEPFTestAsyncListener> PinsB;
	};
	const TSharedRef<FState> State = MakeShared<FState>();

	State->PlayFab.ConfigureTitle(TEXT("EPFTEST"), TEXT("https://epf.test"));
	State->Mock = MakeShared<FEFScopedHttpMock>();
	State->Game = MakeShared<EFTest::FScopedGameInstance>();

	UGameInstance* GameInstance = State->Game->Get();
	if (!TestNotNull(TEXT("the test game instance exists"), GameInstance)
		|| !TestNotNull(TEXT("the test game instance has the SharedData subsystem"), State->Game->GetSubsystem<UEPFSharedDataSubsystem>()))
	{
		return false;
	}
	State->PlayFab.SignIn(TEXT("test-session-ticket"), TEXT("TESTPLAYER"));

	// Both nodes call the same endpoint, so the mock tells them apart by order: the first request
	// (GroupA's) gets data, the second (GroupB's) is rejected by PlayFab. The recorded bodies below
	// confirm which group each answer went to. Before call contexts, both nodes finished on the first
	// OnDataReceived broadcast, so GroupB's node reported GroupA's success.
	(*State->Mock)->AddResponse(EEFHttpVerb::Post, TEXT("/Client/GetSharedGroupData"), 200,
		TEXT("{\"code\":200,\"status\":\"OK\",\"data\":{\"Data\":{\"Owner\":{\"Value\":\"group-a\"}}}}"),
		{}, /*bOnce*/ true);
	(*State->Mock)->AddResponse(EEFHttpVerb::Post, TEXT("/Client/GetSharedGroupData"), 200,
		TEXT("{\"code\":400,\"status\":\"BadRequest\",\"error\":\"SharedGroupNotFound\",\"errorCode\":1088,\"errorMessage\":\"GroupB does not exist\"}"),
		{}, /*bOnce*/ true);

	UEPFAsyncGetSharedData* NodeA = UEPFAsyncGetSharedData::GetSharedGroupData(GameInstance, TEXT("GroupA"), TArray<FString>());
	UEPFAsyncGetSharedData* NodeB = UEPFAsyncGetSharedData::GetSharedGroupData(GameInstance, TEXT("GroupB"), TArray<FString>());
	State->PinsA.Reset(NewObject<UEPFTestAsyncListener>());
	State->PinsB.Reset(NewObject<UEPFTestAsyncListener>());

	NodeA->OnSuccess.AddDynamic(State->PinsA.Get(), &UEPFTestAsyncListener::HandleSuccess);
	NodeA->OnFailure.AddDynamic(State->PinsA.Get(), &UEPFTestAsyncListener::HandleFailure);
	NodeB->OnSuccess.AddDynamic(State->PinsB.Get(), &UEPFTestAsyncListener::HandleSuccess);
	NodeB->OnFailure.AddDynamic(State->PinsB.Get(), &UEPFTestAsyncListener::HandleFailure);

	// Back to back in one frame, as two nodes on one Blueprint execution path would be.
	NodeA->Activate();
	NodeB->Activate();

	TestFalse(TEXT("GroupA's node waits for its response"), State->PinsA->HasFinished());
	TestFalse(TEXT("GroupB's node waits for its response"), State->PinsB->HasFinished());

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("both Get Shared Group Data nodes to finish"), [State]()
	{
		return State->PinsA->HasFinished() && State->PinsB->HasFinished();
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		const UEPFTestAsyncListener* ResultA = State->PinsA.Get();
		const UEPFTestAsyncListener* ResultB = State->PinsB.Get();

		TestEqual(TEXT("GroupA's node fired OnSuccess once"), ResultA->SuccessCount, 1);
		TestEqual(TEXT("GroupA's node did not fire OnFailure"), ResultA->FailureCount, 0);
		TestEqual(TEXT("GroupB's node fired OnFailure once"), ResultB->FailureCount, 1);
		TestEqual(TEXT("GroupB's node did not take GroupA's success"), ResultB->SuccessCount, 0);
		TestEqual(TEXT("GroupB's failure is PlayFab's answer to GroupB"), ResultB->LastError.ErrorCode, FString(TEXT("SharedGroupNotFound")));
		TestEqual(TEXT("GroupB's failure message reaches the pin"), ResultB->LastErrorMessage, FString(TEXT("GroupB does not exist")));

		TArray<FEFHttpMockRecordedRequest> SharedDataRequests;
		for (const FEFHttpMockRecordedRequest& Recorded : (*State->Mock)->GetRequests())
		{
			if (Recorded.Url.Contains(TEXT("/Client/GetSharedGroupData")))
			{
				SharedDataRequests.Add(Recorded);
			}
		}
		if (TestEqual(TEXT("each node sent one request"), SharedDataRequests.Num(), 2))
		{
			TestTrue(TEXT("the first request, answered with data, was GroupA's"), SharedDataRequests[0].Body.Contains(TEXT("\"GroupA\"")));
			TestTrue(TEXT("the second request, answered with an error, was GroupB's"), SharedDataRequests[1].Body.Contains(TEXT("\"GroupB\"")));
			TestEqual(TEXT("requests carry the session ticket"), SharedDataRequests[0].Headers.FindRef(TEXT("X-Authorization")), FString(TEXT("test-session-ticket")));
		}

		if (const UEPFSharedDataSubsystem* SharedData = State->Game->GetSubsystem<UEPFSharedDataSubsystem>())
		{
			TestEqual(TEXT("GroupA's data was cached"), SharedData->GetCachedData(TEXT("GroupA")).FindRef(TEXT("Owner")), FString(TEXT("group-a")));
			TestEqual(TEXT("nothing was cached for GroupB"), SharedData->GetCachedData(TEXT("GroupB")).Num(), 0);
		}

		// Shut down in order: the game instance clears the shared session as it goes, then the
		// project's settings and the previous session are put back.
		State->PinsA.Reset();
		State->PinsB.Reset();
		State->Game.Reset();
		State->Mock.Reset();
		State->PlayFab.Restore();
	}));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
