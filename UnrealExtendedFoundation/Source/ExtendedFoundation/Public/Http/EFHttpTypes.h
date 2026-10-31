// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EFHttpTypes.generated.h"

EXTENDEDFOUNDATION_API DECLARE_LOG_CATEGORY_EXTERN(LogEFHttp, Log, All);

UENUM(BlueprintType)
enum class EEFHttpVerb : uint8
{
	Get,
	Post,
	Put,
	Patch,
	Delete,
	Head,
	Options
};

/** How a request ended. */
UENUM(BlueprintType)
enum class EEFHttpResult : uint8
{
	/** The server answered with a 2xx status. */
	Success,
	/** The server answered with a status outside 2xx. StatusCode and Body describe the error. */
	HttpError,
	/** No response: DNS, TLS or connection failure, or the request could not be started. */
	ConnectionError,
	/** No response within the request's timeout. */
	TimedOut,
	/** The request was cancelled before it finished. */
	Cancelled,
	/** The request was rejected before sending, for example because the URL is empty or not http(s). */
	InvalidRequest
};

/** Everything needed to send one HTTP request. */
USTRUCT(BlueprintType)
struct EXTENDEDFOUNDATION_API FEFHttpRequest
{
	GENERATED_BODY()

	/** Absolute http:// or https:// URL, or a path relative to the HTTP subsystem's base URL. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Http")
	FString Url;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Http")
	EEFHttpVerb Verb = EEFHttpVerb::Get;

	/** Request headers. Names are matched without regard to case. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Http")
	TMap<FString, FString> Headers;

	/** Appended to the URL as ?name=value pairs, URL-encoded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Http")
	TMap<FString, FString> QueryParameters;

	/** Request body, sent as UTF-8. Leave empty for no body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Http", meta = (MultiLine = true))
	FString Body;

	/** Raw request body, e.g. multipart form data or a file upload. When not empty it is sent instead of Body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Http")
	TArray<uint8> BodyBytes;

	/** Content-Type sent with a non-empty Body or BodyBytes, unless Headers already sets one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Http")
	FString ContentType = TEXT("application/json");

	/** Seconds to wait for the whole request. 0 uses the engine's default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Http", meta = (ClampMin = "0", Units = "Seconds"))
	float TimeoutSeconds = 30.f;

	/** Extra attempts after the first one fails. 0 sends the request once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Http|Retry", meta = (ClampMin = "0", ClampMax = "10"))
	int32 MaxRetries = 0;

	/** Delay before the first retry. It doubles for each following retry, up to 30 seconds. A Retry-After header from the server takes precedence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Http|Retry", meta = (ClampMin = "0", Units = "Seconds"))
	float RetryDelaySeconds = 1.f;

	/** Also retry on 408, 429 and 5xx responses. Connection failures and timeouts are always retried while retries remain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Http|Retry")
	bool bRetryOnServerErrors = true;

	/**
	 * Allow retries for POST and PATCH. These are not idempotent: a request whose response was lost
	 * may already have been applied, so retrying it can apply it twice.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Http|Retry")
	bool bAllowRetryForNonIdempotentVerbs = false;

	/**
	 * Log a failed request as a warning. Turn off for clients that retry and report failures
	 * themselves; failures are then logged at Verbose.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Http", AdvancedDisplay)
	bool bLogFailuresAsWarnings = true;
};

/** The outcome of a request: the final attempt's response, or why there was none. */
USTRUCT(BlueprintType)
struct EXTENDEDFOUNDATION_API FEFHttpResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Http")
	EEFHttpResult Result = EEFHttpResult::ConnectionError;

	/** HTTP status code, or 0 when no response was received. */
	UPROPERTY(BlueprintReadOnly, Category = "Http")
	int32 StatusCode = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Http")
	FString Body;

	/** Response headers. Names are matched without regard to case; repeated headers are joined with ", ". */
	UPROPERTY(BlueprintReadOnly, Category = "Http")
	TMap<FString, FString> Headers;

	UPROPERTY(BlueprintReadOnly, Category = "Http")
	FString ContentType;

	/** The URL that was requested, including query parameters. */
	UPROPERTY(BlueprintReadOnly, Category = "Http")
	FString Url;

	/** A readable reason when Result is not Success. */
	UPROPERTY(BlueprintReadOnly, Category = "Http")
	FString ErrorMessage;

	/** How many times the request was sent. 0 when it was rejected or cancelled before sending. */
	UPROPERTY(BlueprintReadOnly, Category = "Http")
	int32 Attempts = 0;

	/** Seconds from the call to completion, including retry delays. */
	UPROPERTY(BlueprintReadOnly, Category = "Http", meta = (Units = "Seconds"))
	float ElapsedSeconds = 0.f;

	bool IsSuccess() const { return Result == EEFHttpResult::Success; }
};

/** Identifies a request sent through UEFHttpSubsystem so it can be cancelled. */
USTRUCT(BlueprintType)
struct EXTENDEDFOUNDATION_API FEFHttpRequestHandle
{
	GENERATED_BODY()

	UPROPERTY()
	int64 Id = 0;

	bool IsValid() const { return Id != 0; }
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FEFHttpResponseDelegate, const FEFHttpResponse&, Response);
