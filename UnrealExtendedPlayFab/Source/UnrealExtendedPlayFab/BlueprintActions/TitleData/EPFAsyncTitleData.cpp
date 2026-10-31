// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncTitleData.h"
#include "TitleData/EPFTitleDataSubsystem.h"

UEPFAsyncGetTitleData* UEPFAsyncGetTitleData::GetTitleData(UObject* WorldContext, const TArray<FString>& Keys)
{
	auto* Action = NewObject<UEPFAsyncGetTitleData>();
	Action->RequestedKeys = Keys;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncGetTitleData::ExecuteAction()
{
	auto* Sub = RequireGameInstanceSubsystem<UEPFTitleDataSubsystem>();
	if (!Sub) return;
	Sub->OnTitleDataReceived.AddDynamic(this, &UEPFAsyncGetTitleData::HandleComplete);
	CallWithContext([&]
	{
		if (RequestedKeys.Num() > 0) { Sub->GetTitleData(RequestedKeys); }
		else { Sub->GetAllTitleData(); }
	});
}

void UEPFAsyncGetTitleData::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFTitleDataSubsystem>())
		Sub->OnTitleDataReceived.RemoveDynamic(this, &UEPFAsyncGetTitleData::HandleComplete);
}

void UEPFAsyncGetTitleData::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get title data")));
}
