// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFCloudScriptSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFCloudScriptExecuted, const FEPFResult&, Result, const FString&, ResultJson);

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
