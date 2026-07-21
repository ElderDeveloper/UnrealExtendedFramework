// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFCloudScriptSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFCloudScriptExecuted, const FEPFResult&, Result, const FString&, ResultJson);

/** Native, per-call completion delegate for CloudScript executions that want a dedicated callback
 *  (instead of the shared OnCloudScriptExecuted multicast). */
DECLARE_DELEGATE_TwoParams(FOnEPFCloudScriptComplete, const FEPFResult& /*Result*/, const FString& /*ResultJson*/);

/**
 * Executes PlayFab CloudScript functions — server-side logic for
 * XP rewards, achievement validation, anti-cheat, etc.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFCloudScriptSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Execute a CloudScript function by name with optional JSON parameters */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|CloudScript")
	void ExecuteFunction(const FString& FunctionName, const FString& FunctionParameterJson = TEXT("{}"));

	/** Execute a CloudScript function with a key-value parameter map */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|CloudScript")
	void ExecuteFunctionWithParams(const FString& FunctionName, const TMap<FString, FString>& Params);

	/**
	 * Execute a "CloudScript using Azure Functions" function via the entity endpoint
	 * /CloudScript/ExecuteFunction (authenticated with the player EntityToken).
	 *
	 * Use this for functions registered under Automation → Cloud Script → Functions
	 * (as opposed to legacy revision handlers, which use ExecuteFunction above).
	 * Broadcasts OnCloudScriptExecuted, mirroring ExecuteFunction.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|CloudScript")
	void ExecuteAzureFunction(const FString& FunctionName, const FString& FunctionParameterJson = TEXT("{}"));

	/**
	 * Same as ExecuteAzureFunction but reports completion through a dedicated per-call
	 * delegate instead of the shared OnCloudScriptExecuted multicast — so concurrent
	 * callers (e.g. a match submit and a currency op) don't cross-fire each other.
	 */
	void ExecuteAzureFunctionCallback(const FString& FunctionName, const FString& FunctionParameterJson, FOnEPFCloudScriptComplete OnComplete);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the result from the last CloudScript execution */
	UFUNCTION(BlueprintPure, Category = "PlayFab|CloudScript")
	FEPFCloudScriptResult GetLastResult() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|CloudScript")
	FOnEPFCloudScriptExecuted OnCloudScriptExecuted;

private:

	FEPFCloudScriptResult LastResult;
};
