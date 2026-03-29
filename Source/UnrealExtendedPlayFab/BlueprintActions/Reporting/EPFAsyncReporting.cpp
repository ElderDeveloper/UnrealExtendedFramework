// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncReporting.h"
#include "Reporting/EPFReportingSubsystem.h"
#include "Engine/GameInstance.h"

UEPFAsyncReportPlayer* UEPFAsyncReportPlayer::ReportPlayer(UObject* WorldContext, const FString& ReporteePlayFabId, const FString& Comment)
{
	auto* Action = NewObject<UEPFAsyncReportPlayer>();
	Action->ReporteeId = ReporteePlayFabId;
	Action->Comment = Comment;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncReportPlayer::Activate()
{
	auto* Sub = GetEPFSubsystemFromContext<UEPFReportingSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Reporting subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnPlayerReported.AddDynamic(this, &UEPFAsyncReportPlayer::HandleComplete);
	Sub->ReportPlayer(ReporteeId, Comment);
}

void UEPFAsyncReportPlayer::HandleComplete(const FEPFResult& Result)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFReportingSubsystem>(WorldContext.Get()))
		Sub->OnPlayerReported.RemoveDynamic(this, &UEPFAsyncReportPlayer::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to report player")));
	SetReadyToDestroy();
}
