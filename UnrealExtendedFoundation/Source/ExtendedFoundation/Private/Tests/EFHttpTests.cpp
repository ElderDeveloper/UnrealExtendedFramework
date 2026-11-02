// Copyright Moon Punch Games. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Http/EFHttpClient.h"
#include "Http/EFHttpLibrary.h"
#include "Http/EFHttpMock.h"
#include "Http/EFHttpSubsystem.h"
#include "Json/EFJsonLibrary.h"
#include "Testing/EFTestHelpers.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFHttpRequestBuildersTest, "ExtendedFoundation.Http.RequestBuilders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFHttpRequestBuildersTest::RunTest(const FString& Parameters)
{
	// Custom verbs: sent upper-cased, validated, never retried by default.
	const FEFHttpRequest Purge = UEFHttpLibrary::MakeCustomVerbHttpRequest(TEXT("https://a.test/cache"), TEXT(" purge "));
	TestTrue(TEXT("custom request uses the Custom verb"), Purge.Verb == EEFHttpVerb::Custom);
	TestEqual(TEXT("custom verb is sent upper-cased"), EFHttp::GetVerbString(Purge), FString(TEXT("PURGE")));
	TestEqual(TEXT("built-in verbs keep their name"), EFHttp::GetVerbString(UEFHttpLibrary::MakeHttpRequest(TEXT("x"), EEFHttpVerb::Patch)), FString(TEXT("PATCH")));
	TestTrue(TEXT("token characters are a valid verb"), EFHttp::IsValidCustomVerb(TEXT("VERSION-CONTROL")));
	TestFalse(TEXT("an empty verb is invalid"), EFHttp::IsValidCustomVerb(FString()));
	TestFalse(TEXT("a verb with a space is invalid"), EFHttp::IsValidCustomVerb(TEXT("GET X")));
	TestFalse(TEXT("custom verbs are not idempotent"), EFHttp::IsIdempotent(EEFHttpVerb::Custom));

	// Form bodies.
	TMap<FString, FString> Fields;
	Fields.Add(TEXT("user"), TEXT("Ada Lovelace"));
	Fields.Add(TEXT("note"), TEXT("a&b=c"));
	TestEqual(TEXT("form body encodes names and values"), EFHttp::BuildFormBody(Fields), FString(TEXT("user=Ada%20Lovelace&note=a%26b%3Dc")));
	TestTrue(TEXT("an empty form has an empty body"), EFHttp::BuildFormBody({}).IsEmpty());
	const FEFHttpRequest Form = UEFHttpLibrary::MakeFormHttpRequest(TEXT("https://a.test/login"), EEFHttpVerb::Post, Fields);
	TestEqual(TEXT("form content type"), Form.ContentType, FString(TEXT("application/x-www-form-urlencoded")));
	TestEqual(TEXT("form body"), Form.Body, EFHttp::BuildFormBody(Fields));

	// Tags.
	FEFHttpRequest Tagged = UEFHttpLibrary::AddRequestTag(Form, TEXT("Login"));
	Tagged = UEFHttpLibrary::AddRequestTag(Tagged, TEXT("Login"));
	Tagged = UEFHttpLibrary::AddRequestTag(Tagged, NAME_None);
	TestEqual(TEXT("a tag is added once and None is ignored"), Tagged.Tags.Num(), 1);
	FEFHttpResponse TaggedResponse;
	TaggedResponse.Tags = Tagged.Tags;
	TestTrue(TEXT("the response has the tag"), UEFHttpLibrary::ResponseHasTag(TaggedResponse, TEXT("Login")));
	TestFalse(TEXT("the response lacks other tags"), UEFHttpLibrary::ResponseHasTag(TaggedResponse, TEXT("Logout")));

	// Response body modes.
	TestTrue(TEXT("JSON is text"), EFHttp::IsTextContentType(TEXT("application/json")));
	TestTrue(TEXT("parameters and case are ignored"), EFHttp::IsTextContentType(TEXT(" Text/HTML ; charset=UTF-8")));
	TestTrue(TEXT("+json types are text"), EFHttp::IsTextContentType(TEXT("application/problem+json")));
	TestTrue(TEXT("a charset makes any type text"), EFHttp::IsTextContentType(TEXT("application/octet-stream; charset=utf-8")));
	TestTrue(TEXT("no content type counts as text"), EFHttp::IsTextContentType(TEXT("")));
	TestFalse(TEXT("images are not text"), EFHttp::IsTextContentType(TEXT("image/png")));
	TestFalse(TEXT("octet-stream is not text"), EFHttp::IsTextContentType(TEXT("application/octet-stream")));
	TestTrue(TEXT("Auto keeps text for JSON"),
		EFHttp::ResolveResponseBodyMode(EEFHttpResponseBodyMode::Auto, TEXT("application/json")) == EEFHttpResponseBodyMode::Text);
	TestTrue(TEXT("Auto keeps bytes for images"),
		EFHttp::ResolveResponseBodyMode(EEFHttpResponseBodyMode::Auto, TEXT("image/png")) == EEFHttpResponseBodyMode::Bytes);
	TestTrue(TEXT("an explicit mode is kept"),
		EFHttp::ResolveResponseBodyMode(EEFHttpResponseBodyMode::TextAndBytes, TEXT("image/png")) == EEFHttpResponseBodyMode::TextAndBytes);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFHttpMockedResponsesTest, "ExtendedFoundation.Http.BinaryBodiesVerbsAndTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFHttpMockedResponsesTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		TSharedPtr<FEFScopedHttpMock> Mock = MakeShared<FEFScopedHttpMock>();
		FEFHttpResponse Binary;
		FEFHttpResponse Text;
		FEFHttpResponse MislabelledAuto;
		FEFHttpResponse MislabelledText;
		FEFHttpResponse Both;
		FEFHttpResponse BadVerb;
		TArray<EEFHttpResult> TaggedResults;
		FEFHttpResponse Untagged;
		int32 Done = 0;
	};
	const TSharedRef<FState> State = MakeShared<FState>();

	// Bytes that are not valid UTF-8 must come back untouched.
	const TArray<uint8> Png = { 0x89, 'P', 'N', 'G', 0x00, 0xFF, 0x10 };
	State->Mock->Get().AddBinaryResponse(EEFHttpVerb::Get, TEXT("/image.png"), 200, Png, TEXT("image/png"));
	State->Mock->Get().AddResponse(EEFHttpVerb::Custom, TEXT("/cache"), 200, TEXT("purged"));
	State->Mock->Get().AddResponse(EEFHttpVerb::Get, TEXT("/slow"), 200, TEXT("{}"));
	State->Mock->Get().AddResponse(EEFHttpVerb::Get, TEXT("/both"), 200, TEXT("hello!"));
	// JSON a server labels as binary, as CDNs often do.
	const FString Json = TEXT("{\"level\":3}");
	const FTCHARToUTF8 JsonUtf8(*Json);
	const TArray<uint8> JsonBytes(reinterpret_cast<const uint8*>(JsonUtf8.Get()), JsonUtf8.Length());
	State->Mock->Get().AddBinaryResponse(EEFHttpVerb::Get, TEXT("/data.json"), 200, JsonBytes);

	FEFHttpOperation::Start(UEFHttpLibrary::MakeHttpRequest(TEXT("https://a.test/image.png")), [State](const FEFHttpResponse& Response)
	{
		State->Binary = Response;
		++State->Done;
	});

	FEFHttpOperation::Start(UEFHttpLibrary::MakeHttpRequest(TEXT("https://a.test/data.json")), [State](const FEFHttpResponse& Response)
	{
		State->MislabelledAuto = Response;
		++State->Done;
	});

	FEFHttpRequest AsText = UEFHttpLibrary::MakeHttpRequest(TEXT("https://a.test/data.json"));
	AsText.ResponseBodyMode = EEFHttpResponseBodyMode::Text;
	FEFHttpOperation::Start(AsText, [State](const FEFHttpResponse& Response)
	{
		State->MislabelledText = Response;
		++State->Done;
	});

	FEFHttpRequest AsBoth = UEFHttpLibrary::MakeHttpRequest(TEXT("https://a.test/both"));
	AsBoth.ResponseBodyMode = EEFHttpResponseBodyMode::TextAndBytes;
	FEFHttpOperation::Start(AsBoth, [State](const FEFHttpResponse& Response)
	{
		State->Both = Response;
		++State->Done;
	});

	FEFHttpRequest Purge = UEFHttpLibrary::MakeCustomVerbHttpRequest(TEXT("https://a.test/cache"), TEXT("purge"));
	Purge = UEFHttpLibrary::AddRequestTag(Purge, TEXT("Cache"));
	FEFHttpOperation::Start(Purge, [State](const FEFHttpResponse& Response)
	{
		State->Text = Response;
		++State->Done;
	});

	FEFHttpRequest BadVerb = UEFHttpLibrary::MakeCustomVerbHttpRequest(TEXT("https://a.test/cache"), TEXT("NOT A VERB"));
	BadVerb.bLogFailuresAsWarnings = false;
	FEFHttpOperation::Start(BadVerb, [State](const FEFHttpResponse& Response)
	{
		State->BadVerb = Response;
		++State->Done;
	});

	// Cancelling by tag stops only the tagged requests.
	UEFHttpSubsystem* Http = NewObject<UEFHttpSubsystem>();
	const FEFHttpRequest Slow = UEFHttpLibrary::AddRequestTag(UEFHttpLibrary::MakeHttpRequest(TEXT("https://a.test/slow")), TEXT("Level"));
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Http->Send(Slow, [State](const FEFHttpResponse& Response) { State->TaggedResults.Add(Response.Result); });
	}
	Http->Send(UEFHttpLibrary::MakeHttpRequest(TEXT("https://a.test/slow")), [State](const FEFHttpResponse& Response)
	{
		State->Untagged = Response;
		++State->Done;
	});
	TestEqual(TEXT("two running requests carry the tag"), Http->GetActiveRequestCountWithTag(TEXT("Level")), 2);
	TestEqual(TEXT("cancelling by tag reports two"), Http->CancelRequestsWithTag(TEXT("Level")), 2);
	TestEqual(TEXT("both tagged callbacks ran"), State->TaggedResults.Num(), 2);
	TestTrue(TEXT("the tagged requests were cancelled"), State->TaggedResults.Num() == 2
		&& State->TaggedResults[0] == EEFHttpResult::Cancelled && State->TaggedResults[1] == EEFHttpResult::Cancelled);
	TestEqual(TEXT("the untagged request still runs"), Http->GetActiveRequestCount(), 1);

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("mocked responses"), [State] { return State->Done == 7; }));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State, Png, Json]()
	{
		TestTrue(TEXT("binary request succeeds"), State->Binary.IsSuccess());
		TestTrue(TEXT("binary body is byte-exact"), State->Binary.BodyBytes == Png);
		TestTrue(TEXT("an image keeps no text copy"), State->Binary.Body.IsEmpty());
		TestEqual(TEXT("binary content type"), State->Binary.ContentType, FString(TEXT("image/png")));

		TestTrue(TEXT("custom verb request succeeds"), State->Text.IsSuccess());
		TestEqual(TEXT("text body"), State->Text.Body, FString(TEXT("purged")));
		TestEqual(TEXT("text keeps no byte copy"), State->Text.BodyBytes.Num(), 0);

		TestTrue(TEXT("Auto keeps bytes for octet-stream"), State->MislabelledAuto.Body.IsEmpty() && State->MislabelledAuto.BodyBytes.Num() > 0);
		TestEqual(TEXT("Get Response Text decodes the bytes"), UEFHttpLibrary::GetResponseText(State->MislabelledAuto), Json);
		FEFJsonObject Parsed;
		TestTrue(TEXT("Get Response Json reads a bytes-only body"), UEFHttpLibrary::GetResponseJson(State->MislabelledAuto, Parsed));
		TestEqual(TEXT("Text mode decodes the body"), State->MislabelledText.Body, Json);
		TestEqual(TEXT("Text mode keeps no bytes"), State->MislabelledText.BodyBytes.Num(), 0);
		TestEqual(TEXT("TextAndBytes keeps the text"), State->Both.Body, FString(TEXT("hello!")));
		TestEqual(TEXT("TextAndBytes keeps the bytes"), State->Both.BodyBytes.Num(), 6);
		TestTrue(TEXT("the response carries the request's tag"), UEFHttpLibrary::ResponseHasTag(State->Text, TEXT("Cache")));
		const TArray<FEFHttpMockRecordedRequest>& Requests = State->Mock->Get().GetRequests();
		TestTrue(TEXT("the mock saw PURGE"), Requests.ContainsByPredicate([](const FEFHttpMockRecordedRequest& Request) { return Request.VerbString == TEXT("PURGE"); }));

		TestTrue(TEXT("an invalid custom verb is rejected"), State->BadVerb.Result == EEFHttpResult::InvalidRequest);
		TestEqual(TEXT("an invalid custom verb is never sent"), State->BadVerb.Attempts, 0);

		TestTrue(TEXT("the untagged request completes"), State->Untagged.IsSuccess());
		State->Mock.Reset();
		return true;
	}));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
