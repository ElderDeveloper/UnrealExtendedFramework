// Copyright Moon Punch Games. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Credentials/EFCredentialStore.h"
#include "HAL/FileManager.h"
#include "Http/EFHttpMock.h"
#include "Http/EFHttpSubsystem.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Status/EFServiceStatus.h"
#include "Testing/EFTestHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFCredentialStoreTest, "ExtendedFoundation.Credentials.FileStore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFCredentialStoreTest::RunTest(const FString& Parameters)
{
	// Exercise the file store in a throwaway folder so no real secret is touched.
	const FString Root = FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("EFCredentials"), FGuid::NewGuid().ToString());
	FEFCredentialStore::SetFileStoreRootForTests(Root);
	ON_SCOPE_EXIT
	{
		FEFCredentialStore::SetFileStoreRootForTests(FString());
		IFileManager::Get().DeleteDirectory(*Root, /*RequireExists*/ false, /*Tree*/ true);
	};

	const FString Service = TEXT("ExtendedFoundation/Test Service");
	const FString Account = TEXT("dev@example.com");
	const FString Secret = TEXT("t\u00F6k\u20ACn with spaces\nand a newline");

	TestFalse(TEXT("nothing stored yet"), FEFCredentialStore::Has(Service, Account));
	TestTrue(TEXT("saves"), FEFCredentialStore::Save(Service, Account, Secret));
	TestTrue(TEXT("is found"), FEFCredentialStore::Has(Service, Account));

	FString Loaded;
	TestTrue(TEXT("loads"), FEFCredentialStore::Load(Service, Account, Loaded));
	TestEqual(TEXT("round trips exactly, including non-ASCII text and newlines"), Loaded, Secret);

	TestFalse(TEXT("another account is separate"), FEFCredentialStore::Has(Service, TEXT("other@example.com")));

	TestTrue(TEXT("overwrites"), FEFCredentialStore::Save(Service, Account, TEXT("second")));
	FEFCredentialStore::Load(Service, Account, Loaded);
	TestEqual(TEXT("the newest value wins"), Loaded, FString(TEXT("second")));

	TestTrue(TEXT("deletes"), FEFCredentialStore::Delete(Service, Account));
	TestFalse(TEXT("gone after delete"), FEFCredentialStore::Has(Service, Account));
	TestTrue(TEXT("deleting again is fine"), FEFCredentialStore::Delete(Service, Account));

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*"), /*Files*/ true, /*Directories*/ false);
	for (const FString& File : Files)
	{
		TestFalse(TEXT("file names do not reveal the account"), File.Contains(TEXT("example.com")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFHttpMockRetryTest, "ExtendedFoundation.Http.MockTransportRetries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFHttpMockRetryTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		TSharedPtr<FEFScopedHttpMock> Mock;
		TOptional<FEFHttpResponse> Flaky;
		TOptional<FEFHttpResponse> Post;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	State->Mock = MakeShared<FEFScopedHttpMock>();

	(*State->Mock)->AddResponse(EEFHttpVerb::Get, TEXT("/flaky"), 503, TEXT("busy"), {}, /*bOnce*/ true);
	(*State->Mock)->AddResponse(EEFHttpVerb::Get, TEXT("/flaky"), 200, TEXT("{\"ok\":true}"));
	(*State->Mock)->AddResponse(EEFHttpVerb::Post, TEXT("/order"), 503, TEXT("busy"));

	FEFHttpRequest Get;
	Get.Url = TEXT("https://api.test/flaky");
	Get.MaxRetries = 2;
	Get.RetryDelaySeconds = 0.f;
	FEFHttpOperation::Start(Get, [State](const FEFHttpResponse& Response) { State->Flaky = Response; });

	// POST is not idempotent, so a 503 is not retried even with retries allowed.
	FEFHttpRequest Post = Get;
	Post.Url = TEXT("https://api.test/order");
	Post.Verb = EEFHttpVerb::Post;
	Post.Body = TEXT("{}");
	FEFHttpOperation::Start(Post, [State](const FEFHttpResponse& Response) { State->Post = Response; });

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("both mocked requests"), [State]()
	{
		return State->Flaky.IsSet() && State->Post.IsSet();
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		if (State->Flaky.IsSet())
		{
			TestTrue(TEXT("the GET succeeds after a retry"), State->Flaky->IsSuccess());
			TestEqual(TEXT("the GET took two attempts"), State->Flaky->Attempts, 2);
			TestEqual(TEXT("the GET body is the second answer"), State->Flaky->Body, FString(TEXT("{\"ok\":true}")));
		}
		if (State->Post.IsSet())
		{
			TestTrue(TEXT("the POST reports the 503"), State->Post->Result == EEFHttpResult::HttpError && State->Post->StatusCode == 503);
			TestEqual(TEXT("the POST was sent once"), State->Post->Attempts, 1);
		}
		TestEqual(TEXT("the mock saw two GETs"), (*State->Mock)->CountRequests(TEXT("/flaky")), 2);
		TestEqual(TEXT("the mock saw one POST"), (*State->Mock)->CountRequests(TEXT("/order")), 1);
		if (TestEqual(TEXT("POST body recorded"), (*State->Mock)->GetRequests().Num(), 3))
		{
			TestEqual(TEXT("recorded body"), (*State->Mock)->GetRequests()[1].Body, FString(TEXT("{}")));
		}
		State->Mock.Reset();
	}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFHttpSubsystemMockTest, "ExtendedFoundation.Http.SubsystemWithMock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFHttpSubsystemMockTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		TSharedPtr<FEFScopedHttpMock> Mock;
		TSharedPtr<EFTest::FScopedGameInstance> Game;
		TOptional<FEFHttpResponse> Response;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	State->Mock = MakeShared<FEFScopedHttpMock>();
	State->Game = MakeShared<EFTest::FScopedGameInstance>();

	UEFHttpSubsystem* Http = State->Game->GetSubsystem<UEFHttpSubsystem>();
	if (!TestNotNull(TEXT("the test game instance has the HTTP subsystem"), Http))
	{
		return false;
	}

	(*State->Mock)->AddResponse(EEFHttpVerb::Get, TEXT("api.test/v1/me"), 200, TEXT("{\"name\":\"Ada\"}"));

	Http->SetBaseUrl(TEXT("https://api.test/v1"));
	Http->SetDefaultHeader(TEXT("Authorization"), TEXT("Bearer t"));

	FEFHttpRequest Request;
	Request.Url = TEXT("me");
	Http->Send(Request, [State](const FEFHttpResponse& Response) { State->Response = Response; });
	TestEqual(TEXT("one request in flight"), Http->GetActiveRequestCount(), 1);

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the subsystem response"), [State]() { return State->Response.IsSet(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		TestTrue(TEXT("the request succeeds"), State->Response.IsSet() && State->Response->IsSuccess());
		if (TestEqual(TEXT("one request recorded"), (*State->Mock)->GetRequests().Num(), 1))
		{
			const FEFHttpMockRecordedRequest& Recorded = (*State->Mock)->GetRequests()[0];
			TestEqual(TEXT("the base URL was applied"), Recorded.Url, FString(TEXT("https://api.test/v1/me")));
			TestEqual(TEXT("the default header was sent"), Recorded.Headers.FindRef(TEXT("Authorization")), FString(TEXT("Bearer t")));
		}
		if (UEFHttpSubsystem* Subsystem = State->Game->GetSubsystem<UEFHttpSubsystem>())
		{
			TestEqual(TEXT("nothing left in flight"), Subsystem->GetActiveRequestCount(), 0);
		}
		State->Game.Reset();
		State->Mock.Reset();
	}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFServiceStatusTest, "ExtendedFoundation.Status.Registry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFServiceStatusTest::RunTest(const FString& Parameters)
{
	const int32 Handle = FEFServiceStatusRegistry::Register(TEXT("ZZTestService"), []
	{
		FEFServiceStatus Status;
		Status.State = EEFServiceState::Degraded;
		Status.Summary = TEXT("retrying");
		Status.Details.Add(TEXT("Attempts"), TEXT("3"));
		return Status;
	});

	const TArray<FEFServiceStatus> Statuses = FEFServiceStatusRegistry::Collect();
	const FEFServiceStatus* Found = Statuses.FindByPredicate([](const FEFServiceStatus& Status) { return Status.Service == FName(TEXT("ZZTestService")); });
	if (TestNotNull(TEXT("the registered service is reported"), Found))
	{
		TestTrue(TEXT("its state is reported"), Found->State == EEFServiceState::Degraded);
	}
	TestTrue(TEXT("the foundation reports Http itself"), Statuses.ContainsByPredicate([](const FEFServiceStatus& Status) { return Status.Service == FName(TEXT("Http")); }));

	const FString Report = FEFServiceStatusRegistry::FormatReport(true);
	TestTrue(TEXT("the report names the service and its summary"), Report.Contains(TEXT("ZZTestService")) && Report.Contains(TEXT("retrying")));
	TestTrue(TEXT("the full report includes details"), Report.Contains(TEXT("Attempts: 3")));
	TestFalse(TEXT("the brief report leaves details out"), FEFServiceStatusRegistry::FormatReport(false).Contains(TEXT("Attempts: 3")));

	FEFServiceStatusRegistry::Unregister(Handle);
	TestFalse(TEXT("an unregistered service is gone"), FEFServiceStatusRegistry::FormatReport(false).Contains(TEXT("ZZTestService")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
