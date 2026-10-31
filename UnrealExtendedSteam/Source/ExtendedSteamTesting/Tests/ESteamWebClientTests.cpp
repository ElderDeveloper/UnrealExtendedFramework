// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Core/ESteamWebClient.h"
#include "Core/ESteamWebSettings.h"
#include "Core/ESteamWebTypes.h"
#include "Http/EFHttpClient.h"
#include "Http/EFHttpMock.h"
#include "Testing/EFTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

// FESteamWebClient::Send end to end, offline: FEFScopedHttpMock answers every request that
// FEFHttpOperation makes, so nothing reaches the network.

namespace ESteamWebClientTestHelpers
{
	/** One FESteamWebClient::Send completion, as the callback saw it. */
	struct FSteamWebTestReply
	{
		FString Json;
		bool bSucceeded = false;
		int32 Code = -1;
	};

	/** Shared with the latent commands, which run on later frames. */
	struct FSteamWebMockedSendState
	{
		TSharedPtr<FEFScopedHttpMock> Mock;
		TOptional<FSteamWebTestReply> Get;
		TOptional<FSteamWebTestReply> Post;
		TOptional<FSteamWebTestReply> ServerError;
		TOptional<FSteamWebTestReply> NoConnection;
		int32 GetCallbackCount = 0;
		bool bDevModeBefore = false;
	};

	const FEFHttpMockRecordedRequest* FindRecorded(const FEFHttpMockTransport& Transport, const FString& UrlContains)
	{
		for (const FEFHttpMockRecordedRequest& Recorded : Transport.GetRequests())
		{
			if (Recorded.Url.Contains(UrlContains))
			{
				return &Recorded;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamWebClientMockedSendTest,
	"UnrealExtendedSteam.Web.Client.MockedSend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamWebClientMockedSendTest::RunTest(const FString& Parameters)
{
	using namespace ESteamWebClientTestHelpers;

	const TSharedRef<FSteamWebMockedSendState> State = MakeShared<FSteamWebMockedSendState>();
	State->Mock = MakeShared<FEFScopedHttpMock>();

	UESteamWebSettings* WebSettings = GetMutableDefault<UESteamWebSettings>();
	State->bDevModeBefore = WebSettings->bUseDevMode;

	FEFHttpMockTransport& Transport = State->Mock->Get();
	Transport.AddResponse(EEFHttpVerb::Get, TEXT("/ISteamNews/GetNewsForApp/v2/"), 200, TEXT("{\"appnews\":{\"appid\":480}}"));
	Transport.AddResponse(EEFHttpVerb::Post, TEXT("/IGameNotificationsService/UserCreateSession/v1/"), 200, TEXT("{\"response\":{}}"));
	Transport.AddResponse(EEFHttpVerb::Get, TEXT("/ISteamUser/GetPlayerSummaries/v2/"), 503, TEXT("busy"));
	Transport.AddFailure(EEFHttpVerb::Get, TEXT("/ISteamUser/ResolveVanityURL/v1/"));

	// Dev mode answers at once, inside Send(), and sends nothing.
	{
		WebSettings->bUseDevMode = true;

		FESteamWebRequest Request;
		Request.Interface = TEXT("ISteamNews");
		Request.Method = TEXT("GetNewsForApp");
		Request.Version = 2;
		Request.bRequiresApiKey = false;

		TOptional<FSteamWebTestReply> DevReply;
		FESteamWebClient::Send(Request, FOnSteamWebResponseNative::CreateLambda([&DevReply](const FString& InJson, bool bInSuccess, int32 InCode)
		{
			DevReply = FSteamWebTestReply{ InJson, bInSuccess, InCode };
		}));

		if (TestTrue(TEXT("dev mode replies immediately"), DevReply.IsSet()))
		{
			TestFalse(TEXT("dev mode reports failure"), DevReply->bSucceeded);
			TestEqual(TEXT("dev mode code is 0"), DevReply->Code, 0);
			TestEqual(TEXT("dev mode body"), DevReply->Json, FString(TEXT("{\"devmode\":true}")));
		}
		TestEqual(TEXT("dev mode sends nothing"), Transport.GetRequests().Num(), 0);
	}

	// Everything below goes through FEFHttpOperation, answered by the mock.
	WebSettings->bUseDevMode = false;

	// The two failing calls are logged as warnings, by the client and by FEFHttpOperation.
	AddExpectedMessage(TEXT("failed"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	// GET: parameters keep their insertion order, an explicit key included.
	{
		FESteamWebRequest Request;
		Request.Interface = TEXT("ISteamNews");
		Request.Method = TEXT("GetNewsForApp");
		Request.Version = 2;
		Request.bRequiresApiKey = false;
		Request.AddParam(TEXT("key"), TEXT("SECRETKEY123"));
		Request.AddParam(TEXT("appid"), 480);
		FESteamWebClient::Send(Request, FOnSteamWebResponseNative::CreateLambda([State](const FString& InJson, bool bInSuccess, int32 InCode)
		{
			++State->GetCallbackCount;
			State->Get = FSteamWebTestReply{ InJson, bInSuccess, InCode };
		}));
	}

	// POST: parameters go in a form-encoded body, not the URL.
	{
		FESteamWebRequest Request;
		Request.Interface = TEXT("IGameNotificationsService");
		Request.Method = TEXT("UserCreateSession");
		Request.Version = 1;
		Request.Verb = EESteamWebVerb::Post;
		Request.bRequiresApiKey = false;
		Request.AddParam(TEXT("appid"), 480);
		Request.AddParam(TEXT("steamid"), TEXT("76561197960287930"));
		FESteamWebClient::Send(Request, FOnSteamWebResponseNative::CreateLambda([State](const FString& InJson, bool bInSuccess, int32 InCode)
		{
			State->Post = FSteamWebTestReply{ InJson, bInSuccess, InCode };
		}));
	}

	// HTTP error: reported with its status and body, and not retried.
	{
		FESteamWebRequest Request;
		Request.Interface = TEXT("ISteamUser");
		Request.Method = TEXT("GetPlayerSummaries");
		Request.Version = 2;
		Request.bRequiresApiKey = false;
		Request.AddParam(TEXT("steamids"), TEXT("76561197960287930"));
		FESteamWebClient::Send(Request, FOnSteamWebResponseNative::CreateLambda([State](const FString& InJson, bool bInSuccess, int32 InCode)
		{
			State->ServerError = FSteamWebTestReply{ InJson, bInSuccess, InCode };
		}));
	}

	// No connection: failure with code 0 and an empty body.
	{
		FESteamWebRequest Request;
		Request.Interface = TEXT("ISteamUser");
		Request.Method = TEXT("ResolveVanityURL");
		Request.Version = 1;
		Request.bRequiresApiKey = false;
		Request.AddParam(TEXT("vanityurl"), TEXT("someone"));
		FESteamWebClient::Send(Request, FOnSteamWebResponseNative::CreateLambda([State](const FString& InJson, bool bInSuccess, int32 InCode)
		{
			State->NoConnection = FSteamWebTestReply{ InJson, bInSuccess, InCode };
		}));
	}

	TestFalse(TEXT("the callback never runs inside Send()"), State->Get.IsSet());

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("four mocked Steam Web replies"), [State]()
	{
		return State->Get.IsSet() && State->Post.IsSet() && State->ServerError.IsSet() && State->NoConnection.IsSet();
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		const FEFHttpMockTransport& Recorder = State->Mock->Get();

		if (State->Get.IsSet())
		{
			TestTrue(TEXT("GET succeeds"), State->Get->bSucceeded);
			TestEqual(TEXT("GET code"), State->Get->Code, 200);
			TestEqual(TEXT("GET body"), State->Get->Json, FString(TEXT("{\"appnews\":{\"appid\":480}}")));
			TestEqual(TEXT("GET calls back once"), State->GetCallbackCount, 1);
		}
		if (const FEFHttpMockRecordedRequest* Recorded = FindRecorded(Recorder, TEXT("GetNewsForApp")))
		{
			TestTrue(TEXT("GET verb"), Recorded->Verb == EEFHttpVerb::Get);
			TestEqual(TEXT("GET url keeps the parameter order"), Recorded->Url,
				FString(TEXT("https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/?key=SECRETKEY123&appid=480")));
			TestTrue(TEXT("GET has no body"), Recorded->Body.IsEmpty());
			// FEFHttpOperation logs every URL through RedactUrlForLog, so the key never reaches the log.
			TestFalse(TEXT("the logged form of the URL has no key"), EFHttp::RedactUrlForLog(Recorded->Url).Contains(TEXT("SECRETKEY123")));
		}
		else
		{
			AddError(TEXT("the GET request was not sent"));
		}

		if (State->Post.IsSet())
		{
			TestTrue(TEXT("POST succeeds"), State->Post->bSucceeded);
			TestEqual(TEXT("POST code"), State->Post->Code, 200);
		}
		if (const FEFHttpMockRecordedRequest* Recorded = FindRecorded(Recorder, TEXT("UserCreateSession")))
		{
			TestTrue(TEXT("POST verb"), Recorded->Verb == EEFHttpVerb::Post);
			TestEqual(TEXT("POST url has no query"), Recorded->Url,
				FString(TEXT("https://api.steampowered.com/IGameNotificationsService/UserCreateSession/v1/")));
			TestEqual(TEXT("POST body is form-encoded"), Recorded->Body, FString(TEXT("appid=480&steamid=76561197960287930")));
			TestEqual(TEXT("POST content type"), Recorded->Headers.FindRef(TEXT("Content-Type")), FString(TEXT("application/x-www-form-urlencoded")));
		}
		else
		{
			AddError(TEXT("the POST request was not sent"));
		}

		if (State->ServerError.IsSet())
		{
			TestFalse(TEXT("HTTP 503 fails"), State->ServerError->bSucceeded);
			TestEqual(TEXT("HTTP 503 code"), State->ServerError->Code, 503);
			TestEqual(TEXT("HTTP 503 body is passed on"), State->ServerError->Json, FString(TEXT("busy")));
		}
		TestEqual(TEXT("the 503 is not retried"), Recorder.CountRequests(TEXT("GetPlayerSummaries")), 1);

		if (State->NoConnection.IsSet())
		{
			TestFalse(TEXT("no connection fails"), State->NoConnection->bSucceeded);
			TestEqual(TEXT("no connection code is 0"), State->NoConnection->Code, 0);
			TestTrue(TEXT("no connection has no body"), State->NoConnection->Json.IsEmpty());
		}

		GetMutableDefault<UESteamWebSettings>()->bUseDevMode = State->bDevModeBefore;
		State->Mock.Reset();
	}));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
