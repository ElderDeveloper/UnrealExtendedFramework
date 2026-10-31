// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EFResult.generated.h"

struct FEFHttpResponse;

/** Codes FEFResult uses for outcomes every service shares. Service-specific codes are free text. */
namespace EFResultCodes
{
	/** The call was cancelled before it finished. */
	inline constexpr const TCHAR* Cancelled = TEXT("Cancelled");
	/** The call did not finish within its timeout. */
	inline constexpr const TCHAR* TimedOut = TEXT("TimedOut");
	/** A required subsystem, service or connection is not available. */
	inline constexpr const TCHAR* NotAvailable = TEXT("NotAvailable");
	/** An argument was missing or invalid; the call was not attempted. */
	inline constexpr const TCHAR* InvalidArgument = TEXT("InvalidArgument");
	/** The server or SDK reported a failure without a more specific code. */
	inline constexpr const TCHAR* Failed = TEXT("Failed");
}

/**
 * The outcome of an operation, in the same shape for every Extended plugin.
 *
 * Plugins keep their own detailed result types; each converts to FEFResult so UI, retry logic and
 * analytics can treat a PlayFab, EOS, Steam or HTTP failure the same way.
 */
USTRUCT(BlueprintType)
struct EXTENDEDFOUNDATION_API FEFResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	bool bSuccess = false;

	/** The system that produced the result, e.g. Http, PlayFab, EOS, Steam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	FName Source;

	/** Machine-readable code from that system. Empty on success. See EFResultCodes for the shared ones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	FString Code;

	/** Readable description for logs and developer UI. Not localized; do not show it to players as is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	FString Message;

	/** HTTP status when the result came from an HTTP call, otherwise 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	int32 HttpStatus = 0;

	/** Whether repeating the same call later can succeed: timeouts, rate limits, server and connection errors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	bool bRetryable = false;

	bool IsSuccess() const { return bSuccess; }
	bool HasCode(const TCHAR* InCode) const { return Code.Equals(InCode, ESearchCase::IgnoreCase); }
	bool IsCancelled() const { return HasCode(EFResultCodes::Cancelled); }
	bool IsTimedOut() const { return HasCode(EFResultCodes::TimedOut); }

	/** "PlayFab InvalidParams (HTTP 400): Display name is too long", or "Success" / "PlayFab: Success". */
	FString ToString() const;

	static FEFResult Success(FName InSource = NAME_None);
	static FEFResult Failure(FName InSource, const FString& InCode, const FString& InMessage, bool bInRetryable = false, int32 InHttpStatus = 0);
	static FEFResult Cancelled(FName InSource);
	static FEFResult TimedOut(FName InSource, float TimeoutSeconds);
	static FEFResult NotAvailable(FName InSource, const FString& WhatIsMissing);
	static FEFResult InvalidArgument(FName InSource, const FString& Problem);

	/** Maps an HTTP response: 2xx succeeds; retryable for timeouts, connection errors, 408, 429 and 5xx. */
	static FEFResult FromHttp(const FEFHttpResponse& Response, FName InSource = TEXT("Http"));
};
