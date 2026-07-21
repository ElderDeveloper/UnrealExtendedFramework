// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncTitleData.h"
#include "TitleData/EPFTitleDataSubsystem.h"
#include "Engine/GameInstance.h"

UEPFAsyncGetTitleData* UEPFAsyncGetTitleData::GetTitleData(UObject* WorldContext, const TArray<FString>& Keys)
{
	auto* Action = NewObject<UEPFAsyncGetTitleData>();
	Action->Keys = Keys;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncGetTitleData::Activate()
{
	auto* Sub = GetEPFSubsystemFromContext<UEPFTitleDataSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("TitleData subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnTitleDataReceived.AddDynamic(this, &UEPFAsyncGetTitleData::HandleComplete);
	Keys.Num() > 0 ? Sub->GetTitleData(Keys) : Sub->GetAllTitleData();
}

void UEPFAsyncGetTitleData::HandleComplete(const FEPFResult& Result)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFTitleDataSubsystem>(WorldContext.Get()))
		Sub->OnTitleDataReceived.RemoveDynamic(this, &UEPFAsyncGetTitleData::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get title data")));
	SetReadyToDestroy();
}
