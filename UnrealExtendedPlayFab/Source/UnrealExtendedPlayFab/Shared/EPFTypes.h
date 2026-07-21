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
	TArray<FString> Scores;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Stats")
	FString Metadata;

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
	FString EntityId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Leaderboards")
	FString EntityType;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Leaderboards")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Leaderboards")
	int32 StatValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Leaderboards")
	TArray<FString> Scores;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Leaderboards")
	FString Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Leaderboards")
	FDateTime LastUpdated;
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


// ── Auto Analytics Config ────────────────────────────────────────────────────

/**
 * Fine-grained toggles for which lifecycle events are automatically tracked.
 * Only active when UEPFSettings::bAutoAnalyticsEnabled is true.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFAutoAnalyticsConfig
{
	GENERATED_BODY()

	/** Fire "session_start" when the Analytics subsystem initialises */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Auto Analytics")
	bool bTrackSessionStart = true;

	/** Fire "session_end" when the Analytics subsystem shuts down */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Auto Analytics")
	bool bTrackSessionEnd = true;

	/** Fire "player_login" after a successful PlayFab login */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Auto Analytics")
	bool bTrackLogin = true;

	/** Fire "player_logout" when the player logs out */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Auto Analytics")
	bool bTrackLogout = true;

	/** Fire "level_change_started" / "level_loaded" on map transitions */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Auto Analytics")
	bool bTrackLevelChange = true;

	/** Call ReportDeviceInfo() automatically after login */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Auto Analytics")
	bool bTrackDeviceInfo = true;

	/** Fire "app_backgrounded" when the application loses focus */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Auto Analytics")
	bool bTrackAppBackground = true;

	/** Fire "app_foregrounded" when the application regains focus */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Auto Analytics")
	bool bTrackAppForeground = true;


	// ── Network & Stability ──────────────────────────────────────────────────

	/** Fire "network_failure" when the engine reports any network driver error (connection lost, timeout, etc.) */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Auto Analytics")
	bool bTrackNetworkFailure = true;

	/**
	 * Fire "crash_detected" on an engine-level error.
	 * The event is written straight to the offline queue on disk — no network call is attempted.
	 * It will be sent to PlayFab on the player's next successful login.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Auto Analytics")
	bool bTrackCrash = true;

	/** Fire "low_memory_warning" when the OS requests resource unloading (primarily iOS / Android) */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Auto Analytics")
	bool bTrackLowMemory = true;


	// ── Multiplayer ───────────────────────────────────────────────────────────

	/**
	 * Fire "player_joined" / "player_left" when players connect or disconnect through the GameMode.
	 * Only fires on the server / listen-server host — silent on pure clients.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Auto Analytics")
	bool bTrackPlayerConnections = true;


	// ── Performance ───────────────────────────────────────────────────────────

	/** Periodically sample and log the average FPS as "fps_sample". Off by default to avoid event spam. */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Auto Analytics")
	bool bTrackFrameRate = false;

	/**
	 * How often (in seconds) to capture an FPS sample.
	 * Minimum 30s recommended. Has no effect when bTrackFrameRate is false.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Auto Analytics",
		meta = (EditCondition = "bTrackFrameRate", ClampMin = "30.0", ClampMax = "600.0"))
	float FrameRateSampleIntervalSeconds = 120.0f;


	// ── Input ─────────────────────────────────────────────────────────────────

	/** Fire "input_device_changed" when a gamepad is connected or disconnected */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Auto Analytics")
	bool bTrackInputDeviceChanged = true;
};


// ── Offline Queue Entry ──────────────────────────────────────────────────────

/**
 * A single analytics event that was captured while the player was offline.
 * Persisted to disk and flushed to PlayFab once authenticated.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFQueuedEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Analytics")
	FString EventName;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Analytics")
	TMap<FString, FString> Body;

	/** UTC timestamp when the event was captured, ISO-8601 string for JSON serialisation */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Analytics")
	FString Timestamp;
};


// ── Common Delegates ─────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFOperationComplete, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFErrorReceived, const FEPFError&, Error);
