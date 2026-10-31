// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncTitleNews.h"

UEPFAsyncGetTitleNews* UEPFAsyncGetTitleNews::GetTitleNews(UObject* WorldContext, int32 Count)
{
	auto* Action = NewObject<UEPFAsyncGetTitleNews>();
	Action->NewsCount = FMath::Clamp(Count, 1, 100);
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncGetTitleNews::ExecuteAction()
{
	auto* Sub = RequireGameInstanceSubsystem<UEPFTitleNewsSubsystem>();
	if (!Sub) return;
	Sub->OnTitleNewsReceived.AddDynamic(this, &UEPFAsyncGetTitleNews::HandleComplete);
	CallWithContext([&] { Sub->GetTitleNews(NewsCount); });
}

void UEPFAsyncGetTitleNews::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFTitleNewsSubsystem>())
		Sub->OnTitleNewsReceived.RemoveDynamic(this, &UEPFAsyncGetTitleNews::HandleComplete);
}

void UEPFAsyncGetTitleNews::HandleComplete(const FEPFResult& Result, const TArray<FEPFTitleNewsItem>& NewsItems)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(NewsItems) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get title news")));
}
