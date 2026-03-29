// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncTitleNews.h"
#include "Engine/GameInstance.h"

UEPFAsyncGetTitleNews* UEPFAsyncGetTitleNews::GetTitleNews(UObject* WorldContext, int32 Count)
{
	auto* Action = NewObject<UEPFAsyncGetTitleNews>();
	Action->Count = FMath::Clamp(Count, 1, 100);
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncGetTitleNews::Activate()
{
	auto* Sub = GetEPFSubsystemFromContext<UEPFTitleNewsSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("TitleNews subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnTitleNewsReceived.AddDynamic(this, &UEPFAsyncGetTitleNews::HandleComplete);
	Sub->GetTitleNews(Count);
}

void UEPFAsyncGetTitleNews::HandleComplete(const FEPFResult& Result, const TArray<FEPFTitleNewsItem>& NewsItems)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFTitleNewsSubsystem>(WorldContext.Get()))
		Sub->OnTitleNewsReceived.RemoveDynamic(this, &UEPFAsyncGetTitleNews::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(NewsItems) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get title news")));
	SetReadyToDestroy();
}
