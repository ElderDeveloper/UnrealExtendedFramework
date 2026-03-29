// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EPFTypes.generated.h"


// ── Normalized Error ───────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFError
{
	GENERATED_BODY()

	FEPFError() = default;

	FEPFError(const FString& InMessage)
	{
		if (!InMessage.IsEmpty())
		{
			bHasError = true;
			ErrorMessage = InMessage;
		}
	}

	FEPFError(const TCHAR* InMessage)
		: FEPFError(FString(InMessage))
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Error")
	bool bHasError = false;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Error")
	int32 HttpStatusCode = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Error")
	FString ErrorCode;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Error")
	FString ErrorName;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Error")
	FString ErrorMessage;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Error")
	FString ErrorDetails;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Error")
	FString RawResponse;

	static FEPFError None()
	{
		return FEPFError();
	}

	static FEPFError Failure(const FString& InMessage, const FString& InCode = TEXT(""), int32 InHttpStatusCode = 0)
	{
		FEPFError Error;
		Error.bHasError = true;
		Error.HttpStatusCode = InHttpStatusCode;
		Error.ErrorCode = InCode;
		Error.ErrorName = InCode;
		Error.ErrorMessage = InMessage;
		return Error;
	}
};


// ── Generic Result ───────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab")
	FString ErrorMessage;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab")
	FString ErrorCode;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab")
	int32 HttpStatusCode = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab")
	FString ErrorDetails;

	static FEPFResult Success()
	{
		FEPFResult Result;
		Result.bSuccess = true;
		return Result;
	}

	static FEPFResult Failure(const FString& InErrorMessage, const FString& InErrorCode = TEXT(""))
	{
		FEPFResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = InErrorMessage;
		Result.ErrorCode = InErrorCode;
		return Result;
	}

	/** Explicit TCHAR* overload to disambiguate TEXT("…") from FEPFError. */
	static FEPFResult Failure(const TCHAR* InErrorMessage)
	{
		return Failure(FString(InErrorMessage));
	}

	static FEPFResult Failure(const FEPFError& InError)
	{
		FEPFResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = InError.ErrorMessage;
		Result.ErrorCode = InError.ErrorCode;
		Result.HttpStatusCode = InError.HttpStatusCode;
		Result.ErrorDetails = InError.ErrorDetails;
		return Result;
	}
};


// ── Auth Context ───────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFAuthContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Auth")
	FString SessionTicket;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Auth")
	FString EntityToken;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Auth")
	FString PlayFabId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Auth")
	FString EntityId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Auth")
	FString EntityType;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Auth")
	FString DeveloperSecretKey;

	bool HasSessionTicket() const
	{
		return !SessionTicket.IsEmpty();
	}

	bool HasEntityToken() const
	{
		return !EntityToken.IsEmpty();
	}

	void Reset()
	{
		SessionTicket.Empty();
		EntityToken.Empty();
		PlayFabId.Empty();
		EntityId.Empty();
		EntityType.Empty();
		DeveloperSecretKey.Empty();
	}
};


// ── Player Data Entry ────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFPlayerData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|PlayerData")
	FString Key;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|PlayerData")
	FString Value;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|PlayerData")
	FDateTime LastUpdated;
};


// ── Statistic ────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFStatistic
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Stats")
	FString StatName;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Stats")
	int32 Value = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Stats")
	int32 Version = 0;
};


// ── Leaderboard Entry ────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFLeaderboardEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Leaderboards")
	int32 Position = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Leaderboards")
	FString PlayFabId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Leaderboards")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Leaderboards")
	int32 StatValue = 0;
};


// ── CloudScript Result ───────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFCloudScriptResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|CloudScript")
	FString FunctionName;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|CloudScript")
	FString ResultJson;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|CloudScript")
	bool bError = false;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|CloudScript")
	FString ErrorMessage;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|CloudScript")
	int32 ExecutionTimeMs = 0;
};


// ── Analytics Event ──────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFAnalyticsEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayFab|Analytics")
	FString EventName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayFab|Analytics")
	TMap<FString, FString> Body;
};


// ── Common Delegates ─────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFOperationComplete, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFErrorReceived, const FEPFError&, Error);
