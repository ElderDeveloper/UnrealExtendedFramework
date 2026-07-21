// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncCloudScript.h"
#include "CloudScript/EPFCloudScriptSubsystem.h"
#include "Engine/GameInstance.h"

UEPFAsyncExecuteCloudScript* UEPFAsyncExecuteCloudScript::ExecuteCloudScript(UObject* WorldContext, const FString& FunctionName, const TMap<FString, FString>& Params)
{
	auto* Action = NewObject<UEPFAsyncExecuteCloudScript>();
	Action->FunctionName = FunctionName;
	Action->Params = Params;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncExecuteCloudScript::Activate()
{
	if (FunctionName.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Function name cannot be empty"))); SetReadyToDestroy(); return; }
	auto* Sub = GetEPFSubsystemFromContext<UEPFCloudScriptSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("CloudScript subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnCloudScriptExecuted.AddDynamic(this, &UEPFAsyncExecuteCloudScript::HandleComplete);
	Sub->ExecuteFunctionWithParams(FunctionName, Params);
}

void UEPFAsyncExecuteCloudScript::HandleComplete(const FEPFResult& Result, const FString& ResultJson)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFCloudScriptSubsystem>(WorldContext.Get()))
		Sub->OnCloudScriptExecuted.RemoveDynamic(this, &UEPFAsyncExecuteCloudScript::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(ResultJson) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("CloudScript execution failed")));
	SetReadyToDestroy();
}
