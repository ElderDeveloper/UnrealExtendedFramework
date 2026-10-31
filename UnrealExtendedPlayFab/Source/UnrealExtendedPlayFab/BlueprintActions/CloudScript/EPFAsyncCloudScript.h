// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "EPFAsyncCloudScript.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncCloudScriptSuccess, const FString&, ResultJson);

UCLASS(meta = (DisplayName = "PlayFab: Execute CloudScript"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncExecuteCloudScript : public UEPFAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Execute CloudScript"), Category = "PlayFab|Async|CloudScript")
	static UEPFAsyncExecuteCloudScript* ExecuteCloudScript(UObject* WorldContext, const FString& FunctionName, const TMap<FString, FString>& Params);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncCloudScriptSuccess OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString CloudFunctionName;
	TMap<FString, FString> FunctionParams;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& ResultJson);
};
