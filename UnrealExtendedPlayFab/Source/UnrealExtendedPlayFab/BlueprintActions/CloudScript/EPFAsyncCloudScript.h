// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "EPFAsyncCloudScript.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncCloudScriptSuccess, const FString&, ResultJson);

UCLASS(meta = (DisplayName = "PlayFab: Execute CloudScript"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncExecuteCloudScript : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Execute CloudScript"), Category = "PlayFab|Async|CloudScript")
	static UEPFAsyncExecuteCloudScript* ExecuteCloudScript(UObject* WorldContext, const FString& FunctionName, const TMap<FString, FString>& Params);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncCloudScriptSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;

private:
	FString FunctionName;
	TMap<FString, FString> Params;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& ResultJson);
};
