// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EFServiceStatus.generated.h"

UENUM(BlueprintType)
enum class EEFServiceState : uint8
{
	/** The service is compiled out, disabled, or its SDK is missing on this platform. */
	Unavailable,
	/** Enabled but missing configuration (ids, keys, URLs). */
	NotConfigured,
	/** Initializing, logging in or connecting. */
	Starting,
	/** Working. */
	Ready,
	/** Working with problems: retries, rate limits, partial features. */
	Degraded,
	/** Failed and not recovering on its own. */
	Error
};

/** One service's state at a moment, as shown by the Extended.Status console command. */
USTRUCT(BlueprintType)
struct EXTENDEDFOUNDATION_API FEFServiceStatus
{
	GENERATED_BODY()

	/** e.g. "PlayFab", "EOS", "Steam", "Http". */
	UPROPERTY(BlueprintReadOnly, Category = "Status")
	FName Service;

	UPROPERTY(BlueprintReadOnly, Category = "Status")
	EEFServiceState State = EEFServiceState::Unavailable;

	/** One line: "Logged in as Ada (PlayFabId 1234)", "2 requests in flight". No secrets. */
	UPROPERTY(BlueprintReadOnly, Category = "Status")
	FString Summary;

	/** Extra key/value lines for the full report. No secrets. */
	UPROPERTY(BlueprintReadOnly, Category = "Status")
	TMap<FString, FString> Details;
};

/**
 * Plugins register a function that reports their service's state; Extended.Status prints them all
 * and debug UI can read them with GetServiceStatuses.
 *
 *     // Module startup:
 *     StatusHandle = FEFServiceStatusRegistry::Register(TEXT("PlayFab"), []
 *     {
 *         FEFServiceStatus Status;
 *         Status.State = ...;
 *         Status.Summary = ...;
 *         return Status;
 *     });
 *     // Module shutdown:
 *     FEFServiceStatusRegistry::Unregister(StatusHandle);
 *
 * Reporters run on the game thread, must be cheap, and must never include tokens, keys or
 * passwords: the output goes to logs and screenshots.
 */
class EXTENDEDFOUNDATION_API FEFServiceStatusRegistry
{
public:
	using FReporter = TFunction<FEFServiceStatus()>;

	/** Registers a reporter for Service and returns a handle for Unregister. */
	static int32 Register(FName Service, FReporter Reporter);

	static void Unregister(int32 Handle);

	/** Every registered service's current status, sorted by service name. */
	static TArray<FEFServiceStatus> Collect();

	/** Collect() formatted as text, one service per block. */
	static FString FormatReport(bool bIncludeDetails = true);

	static const TCHAR* StateToString(EEFServiceState State);
};

UCLASS(meta = (ScriptName = "EFServiceStatusLibrary"))
class EXTENDEDFOUNDATION_API UEFServiceStatusLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The current status of every service that registered one (EOS, PlayFab, Steam, Http, ...). */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Status")
	static TArray<FEFServiceStatus> GetServiceStatuses() { return FEFServiceStatusRegistry::Collect(); }

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Status")
	static FString GetServiceStatusReport(bool bIncludeDetails = true) { return FEFServiceStatusRegistry::FormatReport(bIncludeDetails); }

	/** Shows or hides the on-screen status overlay (the Extended.Status.Overlay console variable). Does nothing in Shipping. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Status")
	static void SetServiceStatusOverlayVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Status")
	static bool IsServiceStatusOverlayVisible();
};
