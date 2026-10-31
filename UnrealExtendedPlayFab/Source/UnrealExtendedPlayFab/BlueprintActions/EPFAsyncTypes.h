// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Shared/EPFTypes.h"
#include "Subsystems/EFSubsystems.h"
#include "EPFAsyncTypes.generated.h"

/** Fired when any PlayFab async action fails. Provides both the full error struct and a plain message string for easy Blueprint wiring. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFAsyncFailed, const FEPFError&, Error, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncSimpleSuccess);

/**
 * Shared async types marker struct (forces UHT to process this file first).
 */
USTRUCT()
struct UNREALEXTENDEDPLAYFAB_API FEPFAsyncTypesDummy
{
	GENERATED_BODY()
};

inline FEPFError MakeEPFAsyncError(const FEPFResult& Result, const FString& FallbackMessage = TEXT("Request failed"))
{
	if (Result.bSuccess)
	{
		return FEPFError::None();
	}

	FEPFError Error = FEPFError::Failure(
		Result.ErrorMessage.IsEmpty() ? FallbackMessage : Result.ErrorMessage,
		Result.ErrorCode,
		Result.HttpStatusCode
	);
	Error.ErrorDetails = Result.ErrorDetails;
	if (!Error.ErrorCode.IsEmpty())
	{
		Error.ErrorName = Error.ErrorCode;
	}
	return Error;
}

/** Build an FEPFError from a foundation result: a node's own cancellation, timeout or missing subsystem. */
inline FEPFError MakeEPFAsyncError(const FEFResult& Result)
{
	FEPFError Error = FEPFError::Failure(Result.Message.IsEmpty() ? Result.ToString() : Result.Message, Result.Code, Result.HttpStatus);
	Error.ErrorName = Result.Code;
	return Error;
}

/** Convenience overload: build an FEPFError from just a message string. */
inline FEPFError MakeEPFAsyncError(const FString& Message)
{
	return MakeEPFAsyncError(FEPFResult::Failure(Message), Message);
}

/** Convenience overload: build an FEPFError from a TCHAR* literal (avoids TEXT("...") ambiguity). */
inline FEPFError MakeEPFAsyncError(const TCHAR* Message)
{
	return MakeEPFAsyncError(FString(Message));
}

/**
 * Helper: broadcast to FOnEPFAsyncFailed (which now carries both FEPFError and ErrorMessage).
 * Use this instead of calling OnFailure.Broadcast(...) directly so the message pin is always populated.
 *
 * Usage from a subsystem-unavailable guard:
 *   BroadcastEPFFailure(OnFailure, TEXT("Subsystem not found"));
 * Usage from a HandleComplete result:
 *   BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to do X")));
 */
inline void BroadcastEPFFailure(FOnEPFAsyncFailed& Delegate, const FEPFError& Error)
{
	Delegate.Broadcast(Error, Error.ErrorMessage);
}

inline void BroadcastEPFFailure(FOnEPFAsyncFailed& Delegate, const TCHAR* Message)
{
	const FEPFError Error = MakeEPFAsyncError(Message);
	Delegate.Broadcast(Error, Error.ErrorMessage);
}

inline void BroadcastEPFFailure(FOnEPFAsyncFailed& Delegate, const FString& Message)
{
	const FEPFError Error = MakeEPFAsyncError(Message);
	Delegate.Broadcast(Error, Error.ErrorMessage);
}


/**
 * Internal helper: resolve a GameInstance subsystem from a WorldContext.
 * Not exposed to Blueprints — used by all async action .cpp files.
 */
template<typename T>
T* GetEPFSubsystemFromContext(UObject* WorldContext)
{
	return EF::GetGameInstanceSubsystem<T>(WorldContext);
}

/**
 * Base for PlayFab async nodes: UEFAsyncActionBase plus the PlayFab failure pin.
 *
 * Nodes start their call inside CallWithContext() and return early from their delegate handler
 * unless IsResultForThisCall(). UEPFSubsystem carries the call context to the response, so two
 * nodes making the same call at once each finish on their own result. Cancellation (the node's
 * Async Task pin), timeouts and a missing subsystem arrive on OnFailure as an FEPFError.
 */
UCLASS(Abstract)
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncActionBase : public UEFAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnEPFAsyncFailed OnFailure;

protected:
	virtual FName GetResultSource() const override { return TEXT("PlayFab"); }

	virtual void BroadcastFailure(const FEFResult& Result) override
	{
		BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result));
	}

	/** Finishes the node and fires OnFailure with Error. */
	void FailWithError(const FEPFError& Error)
	{
		if (FinishAction())
		{
			BroadcastEPFFailure(OnFailure, Error);
		}
	}
};
