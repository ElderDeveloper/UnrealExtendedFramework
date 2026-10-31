// Copyright Moon Punch Games. All Rights Reserved.

#include "Result/EFResult.h"

#include "Http/EFHttpClient.h"
#include "Http/EFHttpTypes.h"

FString FEFResult::ToString() const
{
	const FString Prefix = Source.IsNone() ? FString() : Source.ToString();

	if (bSuccess)
	{
		return Prefix.IsEmpty() ? FString(TEXT("Success")) : Prefix + TEXT(": Success");
	}

	FString Out = Prefix;
	if (!Code.IsEmpty())
	{
		Out += Out.IsEmpty() ? Code : TEXT(" ") + Code;
	}
	if (HttpStatus > 0)
	{
		Out += FString::Printf(TEXT("%s(HTTP %d)"), Out.IsEmpty() ? TEXT("") : TEXT(" "), HttpStatus);
	}
	if (!Message.IsEmpty())
	{
		Out += Out.IsEmpty() ? Message : TEXT(": ") + Message;
	}
	return Out.IsEmpty() ? FString(TEXT("Failed")) : Out;
}

FEFResult FEFResult::Success(FName InSource)
{
	FEFResult Result;
	Result.bSuccess = true;
	Result.Source = InSource;
	return Result;
}

FEFResult FEFResult::Failure(FName InSource, const FString& InCode, const FString& InMessage, bool bInRetryable, int32 InHttpStatus)
{
	FEFResult Result;
	Result.bSuccess = false;
	Result.Source = InSource;
	Result.Code = InCode.IsEmpty() ? FString(EFResultCodes::Failed) : InCode;
	Result.Message = InMessage;
	Result.bRetryable = bInRetryable;
	Result.HttpStatus = InHttpStatus;
	return Result;
}

FEFResult FEFResult::Cancelled(FName InSource)
{
	return Failure(InSource, EFResultCodes::Cancelled, TEXT("The operation was cancelled."));
}

FEFResult FEFResult::TimedOut(FName InSource, float TimeoutSeconds)
{
	return Failure(InSource, EFResultCodes::TimedOut, FString::Printf(TEXT("No result within %.1f seconds."), TimeoutSeconds), /*bRetryable*/ true);
}

FEFResult FEFResult::NotAvailable(FName InSource, const FString& WhatIsMissing)
{
	return Failure(InSource, EFResultCodes::NotAvailable, FString::Printf(TEXT("%s is not available."), *WhatIsMissing));
}

FEFResult FEFResult::InvalidArgument(FName InSource, const FString& Problem)
{
	return Failure(InSource, EFResultCodes::InvalidArgument, Problem);
}

FEFResult FEFResult::FromHttp(const FEFHttpResponse& Response, FName InSource)
{
	switch (Response.Result)
	{
	case EEFHttpResult::Success:
	{
		FEFResult Result = Success(InSource);
		Result.HttpStatus = Response.StatusCode;
		return Result;
	}
	case EEFHttpResult::HttpError:
		return Failure(InSource, FString::Printf(TEXT("Http%d"), Response.StatusCode), Response.ErrorMessage,
			EFHttp::IsRetryableStatus(Response.StatusCode), Response.StatusCode);
	case EEFHttpResult::TimedOut:
		return Failure(InSource, EFResultCodes::TimedOut, Response.ErrorMessage, /*bRetryable*/ true);
	case EEFHttpResult::Cancelled:
		return Failure(InSource, EFResultCodes::Cancelled, Response.ErrorMessage);
	case EEFHttpResult::InvalidRequest:
		return Failure(InSource, EFResultCodes::InvalidArgument, Response.ErrorMessage);
	case EEFHttpResult::ConnectionError:
	default:
		return Failure(InSource, TEXT("ConnectionError"), Response.ErrorMessage, /*bRetryable*/ true);
	}
}
