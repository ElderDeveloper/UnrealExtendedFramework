// Copyright Moon Punch Games. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Http/EFHttpClient.h"
#include "Http/EFHttpLibrary.h"
#include "Http/EFHttpSubsystem.h"
#include "Json/EFJsonLibrary.h"

// Every test here is offline: none of them opens a connection.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFHttpUrlTest, "ExtendedFoundation.Http.Urls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFHttpUrlTest::RunTest(const FString& Parameters)
{
	const TMap<FString, FString> NoQuery;
	TestEqual(TEXT("no parameters leaves the URL alone"), EFHttp::BuildUrl(TEXT("https://a.test/p"), NoQuery), FString(TEXT("https://a.test/p")));

	TMap<FString, FString> Query;
	Query.Add(TEXT("q"), TEXT("x"));
	TestEqual(TEXT("first parameter starts the query"), EFHttp::BuildUrl(TEXT("https://a.test/p"), Query), FString(TEXT("https://a.test/p?q=x")));
	TestEqual(TEXT("an existing query is extended"), EFHttp::BuildUrl(TEXT("https://a.test/p?a=1"), Query), FString(TEXT("https://a.test/p?a=1&q=x")));
	TestEqual(TEXT("a trailing ? is reused"), EFHttp::BuildUrl(TEXT("https://a.test/p?"), Query), FString(TEXT("https://a.test/p?q=x")));
	TestEqual(TEXT("the fragment stays last"), EFHttp::BuildUrl(TEXT("https://a.test/p#top"), Query), FString(TEXT("https://a.test/p?q=x#top")));

	TMap<FString, FString> Unsafe;
	Unsafe.Add(TEXT("name"), TEXT("a b&c=d"));
	const FString Encoded = EFHttp::BuildUrl(TEXT("https://a.test/p"), Unsafe);
	TestFalse(TEXT("spaces are encoded"), Encoded.Contains(TEXT(" ")));
	TestFalse(TEXT("& and = inside a value are encoded"), Encoded.Contains(TEXT("b&c")) || Encoded.Contains(TEXT("c=d")));
	TestEqual(TEXT("plain text is not encoded"), EFHttp::UrlEncode(TEXT("abc123")), FString(TEXT("abc123")));

	TestEqual(TEXT("base and path join with one slash"), EFHttp::ResolveUrl(TEXT("https://api.test/v1/"), TEXT("/users")), FString(TEXT("https://api.test/v1/users")));
	TestEqual(TEXT("base without slash"), EFHttp::ResolveUrl(TEXT("https://api.test/v1"), TEXT("users")), FString(TEXT("https://api.test/v1/users")));
	TestEqual(TEXT("an absolute URL ignores the base"), EFHttp::ResolveUrl(TEXT("https://api.test/v1"), TEXT("https://other.test/x")), FString(TEXT("https://other.test/x")));
	TestEqual(TEXT("no base leaves the URL alone"), EFHttp::ResolveUrl(TEXT(""), TEXT("users")), FString(TEXT("users")));

	TestTrue(TEXT("https is accepted"), EFHttp::HasHttpScheme(TEXT("https://a.test")));
	TestTrue(TEXT("scheme case is ignored"), EFHttp::HasHttpScheme(TEXT("HTTP://a.test")));
	TestFalse(TEXT("a bare scheme is rejected"), EFHttp::HasHttpScheme(TEXT("https://")));
	TestFalse(TEXT("other schemes are rejected"), EFHttp::HasHttpScheme(TEXT("ftp://a.test")));
	TestFalse(TEXT("relative paths are rejected"), EFHttp::HasHttpScheme(TEXT("users/1")));

	TestEqual(TEXT("user info, query and fragment are redacted"),
		EFHttp::RedactUrlForLog(TEXT("https://user:secret@a.test/path?token=abc#frag")), FString(TEXT("https://a.test/path")));
	TestEqual(TEXT("an @ in the path is not user info"),
		EFHttp::RedactUrlForLog(TEXT("https://a.test/users/@me")), FString(TEXT("https://a.test/users/@me")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFHttpPolicyTest, "ExtendedFoundation.Http.RetryPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFHttpPolicyTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("verb"), FString(EFHttp::VerbToString(EEFHttpVerb::Delete)), FString(TEXT("DELETE")));
	TestTrue(TEXT("GET is idempotent"), EFHttp::IsIdempotent(EEFHttpVerb::Get));
	TestTrue(TEXT("PUT is idempotent"), EFHttp::IsIdempotent(EEFHttpVerb::Put));
	TestFalse(TEXT("POST is not"), EFHttp::IsIdempotent(EEFHttpVerb::Post));
	TestFalse(TEXT("PATCH is not"), EFHttp::IsIdempotent(EEFHttpVerb::Patch));

	TestTrue(TEXT("204 is success"), EFHttp::IsSuccessStatus(204));
	TestFalse(TEXT("304 is not success"), EFHttp::IsSuccessStatus(304));
	TestTrue(TEXT("503 is retryable"), EFHttp::IsRetryableStatus(503));
	TestTrue(TEXT("429 is retryable"), EFHttp::IsRetryableStatus(429));
	TestFalse(TEXT("404 is not retryable"), EFHttp::IsRetryableStatus(404));

	TestEqual(TEXT("first retry waits the base delay"), EFHttp::ComputeRetryDelay(1, 1.f), 1.f);
	TestEqual(TEXT("the delay doubles"), EFHttp::ComputeRetryDelay(3, 1.f), 4.f);
	TestEqual(TEXT("the delay is capped"), EFHttp::ComputeRetryDelay(10, 1.f), 30.f);
	TestEqual(TEXT("Retry-After wins"), EFHttp::ComputeRetryDelay(1, 1.f, 5.f), 5.f);
	TestEqual(TEXT("Retry-After is capped"), EFHttp::ComputeRetryDelay(1, 1.f, 600.f), 60.f);
	TestEqual(TEXT("a zero base delay retries at once"), EFHttp::ComputeRetryDelay(4, 0.f), 0.f);

	TestEqual(TEXT("Retry-After seconds"), EFHttp::ParseRetryAfterSeconds(TEXT(" 120 ")), 120.f);
	TestEqual(TEXT("Retry-After dates are ignored"), EFHttp::ParseRetryAfterSeconds(TEXT("Wed, 21 Oct 2015 07:28:00 GMT")), 0.f);
	TestEqual(TEXT("negative Retry-After is ignored"), EFHttp::ParseRetryAfterSeconds(TEXT("-3")), 0.f);

	const TMap<FString, FString> Headers = EFHttp::ParseHeaderLines({
		TEXT("Content-Type: application/json"),
		TEXT("Set-Cookie: a=1"),
		TEXT("set-cookie: b=2"),
		TEXT("Location: https://a.test/x?y=1"),
		TEXT("not a header"),
		TEXT(": no name"),
	});
	TestEqual(TEXT("malformed lines are skipped"), Headers.Num(), 3);
	TestEqual(TEXT("lookup ignores case"), Headers.FindRef(TEXT("content-type")), FString(TEXT("application/json")));
	TestEqual(TEXT("repeated headers are joined"), Headers.FindRef(TEXT("Set-Cookie")), FString(TEXT("a=1, b=2")));
	TestEqual(TEXT("only the first colon splits"), Headers.FindRef(TEXT("Location")), FString(TEXT("https://a.test/x?y=1")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFHttpInvalidRequestTest, "ExtendedFoundation.Http.InvalidRequestCompletesLater",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFHttpInvalidRequestTest::RunTest(const FString& Parameters)
{
	// Shared with the latent check below, which runs on later frames.
	struct FState
	{
		int32 Calls = 0;
		FEFHttpResponse Received;
		TSharedPtr<FEFHttpOperation> Operation;
		int32 CancelCalls = 0;
		EEFHttpResult CancelResult = EEFHttpResult::Success;
		int32 SilentCalls = 0;
		int32 FramesWaited = 0;
	};
	const TSharedRef<FState> State = MakeShared<FState>();

	FEFHttpRequest Request;
	Request.Url = TEXT("ftp://a.test/file");
	State->Operation = FEFHttpOperation::Start(Request, [State](const FEFHttpResponse& Response)
	{
		++State->Calls;
		State->Received = Response;
	});

	TestEqual(TEXT("the callback never runs inside Start()"), State->Calls, 0);
	TestFalse(TEXT("the operation is still pending"), State->Operation->IsFinished());

	// Cancelling while the failure is still queued reports Cancelled instead, at once and only once.
	Request.Url.Reset();
	const TSharedRef<FEFHttpOperation> Cancelled = FEFHttpOperation::Start(Request, [State](const FEFHttpResponse& Response)
	{
		++State->CancelCalls;
		State->CancelResult = Response.Result;
	});
	Cancelled->Cancel();
	TestEqual(TEXT("cancel reports immediately"), State->CancelCalls, 1);
	TestTrue(TEXT("cancel reports Cancelled"), State->CancelResult == EEFHttpResult::Cancelled);

	// A silent cancel drops the callback.
	const TSharedRef<FEFHttpOperation> Silent = FEFHttpOperation::Start(Request, [State](const FEFHttpResponse&) { ++State->SilentCalls; });
	Silent->Cancel(/*bNotify*/ false);

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		// Queued completions run on the next core ticker tick; give it a few frames.
		if (++State->FramesWaited < 3)
		{
			return false;
		}

		TestEqual(TEXT("the invalid request reports exactly once"), State->Calls, 1);
		TestTrue(TEXT("the result is InvalidRequest"), State->Received.Result == EEFHttpResult::InvalidRequest);
		TestEqual(TEXT("nothing was sent"), State->Received.Attempts, 0);
		TestTrue(TEXT("the operation is finished"), State->Operation->IsFinished());
		TestEqual(TEXT("a cancelled request does not report again"), State->CancelCalls, 1);
		TestEqual(TEXT("a silent cancel never calls back"), State->SilentCalls, 0);
		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFHttpSubsystemPrepareTest, "ExtendedFoundation.Http.SubsystemPrepare",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFHttpSubsystemPrepareTest::RunTest(const FString& Parameters)
{
	UEFHttpSubsystem* Http = NewObject<UEFHttpSubsystem>();
	Http->SetBaseUrl(TEXT(" https://api.test/v1/ "));
	Http->SetDefaultHeader(TEXT("authorization"), TEXT("Bearer default"));
	Http->SetDefaultHeader(TEXT("X-Client"), TEXT("tests"));

	FEFHttpRequest Request = UEFHttpLibrary::MakeHttpRequest(TEXT("/users/1"));
	Request = UEFHttpLibrary::SetBearerToken(Request, TEXT("mine"));

	const FEFHttpRequest Prepared = Http->PrepareRequest(Request);
	TestEqual(TEXT("the base URL is applied"), Prepared.Url, FString(TEXT("https://api.test/v1/users/1")));
	TestEqual(TEXT("the request's header wins, ignoring case"), Prepared.Headers.FindRef(TEXT("Authorization")), FString(TEXT("Bearer mine")));
	TestEqual(TEXT("defaults fill the rest"), Prepared.Headers.FindRef(TEXT("X-Client")), FString(TEXT("tests")));
	TestEqual(TEXT("no duplicate Authorization header"), Prepared.Headers.Num(), 2);

	Http->ClearDefaultHeaders();
	TestEqual(TEXT("cleared defaults add nothing"), Http->PrepareRequest(UEFHttpLibrary::MakeHttpRequest(TEXT("x"))).Headers.Num(), 0);

	FEFJsonObject Body = UEFJsonLibrary::SetStringField(FEFJsonObject(), TEXT("name"), TEXT("Ada"));
	const FEFHttpRequest JsonRequest = UEFHttpLibrary::MakeJsonHttpRequest(TEXT("https://a.test"), EEFHttpVerb::Post, Body);
	TestEqual(TEXT("JSON body"), JsonRequest.Body, FString(TEXT("{\"name\":\"Ada\"}")));
	TestEqual(TEXT("JSON content type"), JsonRequest.ContentType, FString(TEXT("application/json")));

	FEFHttpResponse Response;
	Response.Body = TEXT("{\"ok\":true}");
	Response.Headers.Add(TEXT("Content-Type"), TEXT("application/json"));
	FEFJsonObject Parsed;
	TestTrue(TEXT("response JSON parses"), UEFHttpLibrary::GetResponseJson(Response, Parsed));
	FString ContentType;
	TestTrue(TEXT("response header lookup ignores case"), UEFHttpLibrary::GetResponseHeader(Response, TEXT("content-type"), ContentType));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
