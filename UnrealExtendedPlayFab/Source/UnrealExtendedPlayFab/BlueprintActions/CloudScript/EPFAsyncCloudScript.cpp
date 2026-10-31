// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncCloudScript.h"
#include "CloudScript/EPFCloudScriptSubsystem.h"

UEPFAsyncExecuteCloudScript* UEPFAsyncExecuteCloudScript::ExecuteCloudScript(UObject* WorldContext, const FString& FunctionName, const TMap<FString, FString>& Params)
{
	auto* Action = NewObject<UEPFAsyncExecuteCloudScript>();
	Action->CloudFunctionName = FunctionName;
	Action->FunctionParams = Params;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncExecuteCloudScript::ExecuteAction()
{
	if (CloudFunctionName.IsEmpty()) { FailInvalidArgument(TEXT("Function name cannot be empty")); return; }
	auto* Sub = RequireGameInstanceSubsystem<UEPFCloudScriptSubsystem>();
	if (!Sub) return;
	Sub->OnCloudScriptExecuted.AddDynamic(this, &UEPFAsyncExecuteCloudScript::HandleComplete);
	CallWithContext([&] { Sub->ExecuteFunctionWithParams(CloudFunctionName, FunctionParams); });
}

void UEPFAsyncExecuteCloudScript::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFCloudScriptSubsystem>())
		Sub->OnCloudScriptExecuted.RemoveDynamic(this, &UEPFAsyncExecuteCloudScript::HandleComplete);
}

void UEPFAsyncExecuteCloudScript::HandleComplete(const FEPFResult& Result, const FString& ResultJson)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(ResultJson) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("CloudScript execution failed")));
}
