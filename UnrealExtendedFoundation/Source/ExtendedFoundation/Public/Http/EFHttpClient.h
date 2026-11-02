// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Http/EFHttpTypes.h"
#include "Containers/Ticker.h"
#include "Interfaces/IHttpRequest.h"

/**
 * Sends one attempt of a request instead of the engine's HTTP module.
 *
 * Install one with EFHttp::SetTransportOverride (or FEFScopedHttpMock in tests) to run every
 * FEFHttpOperation offline. Retries, timeouts of the retry schedule, cancellation and logging still
 * go through FEFHttpOperation; the transport only answers single attempts.
 */
class IEFHttpTransport
{
public:
	virtual ~IEFHttpTransport() = default;

	/**
	 * Answers one attempt. Call OnResponse exactly once, on the game thread, now or later. Set
	 * StatusCode, Body or BodyBytes, Headers and ContentType for an answer, or set Result to
	 * ConnectionError or TimedOut for no answer. Result is then derived from the status, and the body
	 * is converted to the forms the request's ResponseBodyMode keeps, as for a real response.
	 */
	virtual void Send(const FEFHttpRequest& Request, const FString& FinalUrl, TFunction<void(const FEFHttpResponse&)> OnResponse) = 0;
};

/**
 * One HTTP request, including its retries, sent through the engine's HTTP module.
 *
 * This is the C++ layer under UEFHttpSubsystem and the Http Request Blueprint node. It needs no
 * UObject or game instance, so editor tools and other modules can use it directly:
 *
 *     FEFHttpRequest Request;
 *     Request.Url = TEXT("https://api.example.com/status");
 *     FEFHttpOperation::Start(Request, [](const FEFHttpResponse& Response) { ... });
 *
 * - The completion callback runs exactly once, on the game thread, and never from inside Start()
 *   or from inside package loading (MoviePlayer ticks HTTP during a blocking load); it then runs on
 *   the first tick after.
 * - The operation keeps itself alive until it finishes; holding the returned pointer is only
 *   needed to cancel it.
 * - A callback that touches a UObject must check that the object is still valid.
 */
class EXTENDEDFOUNDATION_API FEFHttpOperation : public TSharedFromThis<FEFHttpOperation>
{
public:
	using FOnComplete = TFunction<void(const FEFHttpResponse&)>;

	/** Validates and sends the request. */
	static TSharedRef<FEFHttpOperation> Start(const FEFHttpRequest& Request, FOnComplete OnComplete);

	/**
	 * Stops the request and any pending retry. With bNotify the callback runs now with a Cancelled
	 * result (inside package loading: on the first tick after); without it the callback is dropped.
	 * Does nothing once the operation has finished.
	 */
	void Cancel(bool bNotify = true);

	bool IsFinished() const { return bFinished; }

	/** Operations started and not yet finished, across the process. */
	static int32 GetActiveCount();

	const FEFHttpRequest& GetRequest() const { return Request; }

	/** Use Start(). Public only so MakeShared can construct the operation. */
	FEFHttpOperation(const FEFHttpRequest& InRequest, FOnComplete InOnComplete);
	~FEFHttpOperation();

private:
	void BeginAttempt();
	void HandleAttemptComplete(int32 AttemptNumber, FHttpRequestPtr CompletedRequest, FHttpResponsePtr HttpResponse, bool bConnectedSuccessfully);
	void HandleTransportResponse(int32 AttemptNumber, FEFHttpResponse Response);
	void ProcessAttemptResponse(FEFHttpResponse Response);
	void ScheduleRetry(const FEFHttpResponse& FailedResponse);
	void FinishOnNextTick(FEFHttpResponse Response);
	void Finish(FEFHttpResponse Response);
	void ReleaseHttpRequest();

	FEFHttpRequest Request;
	FString FinalUrl;
	FOnComplete OnComplete;
	FHttpRequestPtr HttpRequest;
	FTSTicker::FDelegateHandle PendingTickerHandle;
	double StartTimeSeconds = 0.0;
	int32 AttemptCount = 0;
	bool bFinished = false;
	/** True while Start() runs; completions in that window are deferred to the next tick. */
	bool bStarting = false;
};

/** Pure helpers behind FEFHttpOperation, exposed for reuse and tests. */
namespace EFHttp
{
	/**
	 * Routes every FEFHttpOperation attempt through Transport instead of the network, until reset
	 * with nullptr. Process-wide and game-thread only; restore it when done (FEFScopedHttpMock does).
	 */
	EXTENDEDFOUNDATION_API void SetTransportOverride(TSharedPtr<IEFHttpTransport> Transport);
	EXTENDEDFOUNDATION_API TSharedPtr<IEFHttpTransport> GetTransportOverride();

	/** "GET", "POST" and so on. Custom has no fixed name and returns "CUSTOM"; use GetVerbString for a request. */
	EXTENDEDFOUNDATION_API const TCHAR* VerbToString(EEFHttpVerb Verb);

	/** The verb the request sends: its CustomVerb upper-cased when Verb is Custom. */
	EXTENDEDFOUNDATION_API FString GetVerbString(const FEFHttpRequest& Request);

	/** True for a CustomVerb that can be sent: letters, digits, '-' and '_', at least one character. */
	EXTENDEDFOUNDATION_API bool IsValidCustomVerb(const FString& CustomVerb);

	/** POST, PATCH and custom verbs are the ones whose repetition can change the result. */
	EXTENDEDFOUNDATION_API bool IsIdempotent(EEFHttpVerb Verb);

	EXTENDEDFOUNDATION_API bool IsSuccessStatus(int32 StatusCode);

	/** 408 Request Timeout, 429 Too Many Requests and every 5xx. */
	EXTENDEDFOUNDATION_API bool IsRetryableStatus(int32 StatusCode);

	/** True for http:// and https:// URLs with something after the scheme. */
	EXTENDEDFOUNDATION_API bool HasHttpScheme(const FString& Url);

	/** Percent-encodes a query component. */
	EXTENDEDFOUNDATION_API FString UrlEncode(const FString& Value);

	/** An application/x-www-form-urlencoded body: name=value pairs, URL-encoded, joined with '&'. */
	EXTENDEDFOUNDATION_API FString BuildFormBody(const TMap<FString, FString>& Fields);

	/** Appends URL-encoded query parameters, keeping any existing query and #fragment. */
	EXTENDEDFOUNDATION_API FString BuildUrl(const FString& BaseUrl, const TMap<FString, FString>& QueryParameters);

	/** Joins a base URL and a relative path with exactly one '/'. An absolute Url is returned unchanged. */
	EXTENDEDFOUNDATION_API FString ResolveUrl(const FString& BaseUrl, const FString& Url);

	/** scheme://host/path with user info, query and fragment removed, so tokens in a URL never reach the log. */
	EXTENDEDFOUNDATION_API FString RedactUrlForLog(const FString& Url);

	/**
	 * Seconds to wait before retry number RetryNumber (1 for the first retry): BaseDelay doubled for
	 * each previous retry, capped at 30 seconds. A positive RetryAfterSeconds from the server is used
	 * instead, capped at 60 seconds.
	 */
	EXTENDEDFOUNDATION_API float ComputeRetryDelay(int32 RetryNumber, float BaseDelaySeconds, float RetryAfterSeconds = 0.f);

	/**
	 * True for content that is text: text/*, JSON, XML, YAML, JavaScript, form data and the +json,
	 * +xml and +yaml types, any type with a charset parameter, and an empty Content-Type.
	 */
	EXTENDEDFOUNDATION_API bool IsTextContentType(const FString& ContentType);

	/** The body forms a response with ContentType keeps: Auto becomes Text or Bytes, other modes stay as they are. */
	EXTENDEDFOUNDATION_API EEFHttpResponseBodyMode ResolveResponseBodyMode(EEFHttpResponseBodyMode Mode, const FString& ContentType);

	/** The response body as text: Body, or BodyBytes decoded as UTF-8 when only the bytes were kept. */
	EXTENDEDFOUNDATION_API FString GetResponseText(const FEFHttpResponse& Response);

	/** Parses "Name: Value" lines into a map, joining repeated names with ", ". */
	EXTENDEDFOUNDATION_API TMap<FString, FString> ParseHeaderLines(const TArray<FString>& HeaderLines);

	/** Delay-seconds form of a Retry-After header. Returns 0 for HTTP dates and anything unparsable. */
	EXTENDEDFOUNDATION_API float ParseRetryAfterSeconds(const FString& HeaderValue);
}
