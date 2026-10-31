// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Backend/Http/EFHttpTypes.h"
#include "Containers/Ticker.h"
#include "Interfaces/IHttpRequest.h"

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
 * - The completion callback runs exactly once, on the game thread, and never from inside Start().
 * - The operation keeps itself alive until it finishes; holding the returned pointer is only
 *   needed to cancel it.
 * - A callback that touches a UObject must check that the object is still valid.
 */
class UNREALEXTENDEDFRAMEWORK_API FEFHttpOperation : public TSharedFromThis<FEFHttpOperation>
{
public:
	using FOnComplete = TFunction<void(const FEFHttpResponse&)>;

	/** Validates and sends the request. */
	static TSharedRef<FEFHttpOperation> Start(const FEFHttpRequest& Request, FOnComplete OnComplete);

	/**
	 * Stops the request and any pending retry. With bNotify the callback runs now with a Cancelled
	 * result; without it the callback is dropped. Does nothing once the operation has finished.
	 */
	void Cancel(bool bNotify = true);

	bool IsFinished() const { return bFinished; }

	const FEFHttpRequest& GetRequest() const { return Request; }

	/** Use Start(). Public only so MakeShared can construct the operation. */
	FEFHttpOperation(const FEFHttpRequest& InRequest, FOnComplete InOnComplete);
	~FEFHttpOperation();

private:
	void BeginAttempt();
	void HandleAttemptComplete(int32 AttemptNumber, FHttpRequestPtr CompletedRequest, FHttpResponsePtr HttpResponse, bool bConnectedSuccessfully);
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
	UNREALEXTENDEDFRAMEWORK_API const TCHAR* VerbToString(EEFHttpVerb Verb);

	/** POST and PATCH are the verbs whose repetition can change the result. */
	UNREALEXTENDEDFRAMEWORK_API bool IsIdempotent(EEFHttpVerb Verb);

	UNREALEXTENDEDFRAMEWORK_API bool IsSuccessStatus(int32 StatusCode);

	/** 408 Request Timeout, 429 Too Many Requests and every 5xx. */
	UNREALEXTENDEDFRAMEWORK_API bool IsRetryableStatus(int32 StatusCode);

	/** True for http:// and https:// URLs with something after the scheme. */
	UNREALEXTENDEDFRAMEWORK_API bool HasHttpScheme(const FString& Url);

	/** Percent-encodes a query component. */
	UNREALEXTENDEDFRAMEWORK_API FString UrlEncode(const FString& Value);

	/** Appends URL-encoded query parameters, keeping any existing query and #fragment. */
	UNREALEXTENDEDFRAMEWORK_API FString BuildUrl(const FString& BaseUrl, const TMap<FString, FString>& QueryParameters);

	/** Joins a base URL and a relative path with exactly one '/'. An absolute Url is returned unchanged. */
	UNREALEXTENDEDFRAMEWORK_API FString ResolveUrl(const FString& BaseUrl, const FString& Url);

	/** scheme://host/path with user info, query and fragment removed, so tokens in a URL never reach the log. */
	UNREALEXTENDEDFRAMEWORK_API FString RedactUrlForLog(const FString& Url);

	/**
	 * Seconds to wait before retry number RetryNumber (1 for the first retry): BaseDelay doubled for
	 * each previous retry, capped at 30 seconds. A positive RetryAfterSeconds from the server is used
	 * instead, capped at 60 seconds.
	 */
	UNREALEXTENDEDFRAMEWORK_API float ComputeRetryDelay(int32 RetryNumber, float BaseDelaySeconds, float RetryAfterSeconds = 0.f);

	/** Parses "Name: Value" lines into a map, joining repeated names with ", ". */
	UNREALEXTENDEDFRAMEWORK_API TMap<FString, FString> ParseHeaderLines(const TArray<FString>& HeaderLines);

	/** Delay-seconds form of a Retry-After header. Returns 0 for HTTP dates and anything unparsable. */
	UNREALEXTENDEDFRAMEWORK_API float ParseRetryAfterSeconds(const FString& HeaderValue);
}
