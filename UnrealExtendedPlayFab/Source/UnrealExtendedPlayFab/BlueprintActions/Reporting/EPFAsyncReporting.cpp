// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncReporting.h"
#include "Reporting/EPFReportingSubsystem.h"

UEPFAsyncReportPlayer* UEPFAsyncReportPlayer::ReportPlayer(UObject* WorldContext, const FString& ReporteePlayFabId, const FString& Comment)
{
	auto* Action = NewObject<UEPFAsyncReportPlayer>();
	Action->ReporteeId = ReporteePlayFabId;
	Action->ReportComment = Comment;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncReportPlayer::ExecuteAction()
{
	auto* Sub = RequireGameInstanceSubsystem<UEPFReportingSubsystem>();
	if (!Sub) return;
	Sub->OnPlayerReported.AddDynamic(this, &UEPFAsyncReportPlayer::HandleComplete);
	CallWithContext([&] { Sub->ReportPlayer(ReporteeId, ReportComment); });
}

void UEPFAsyncReportPlayer::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFReportingSubsystem>())
		Sub->OnPlayerReported.RemoveDynamic(this, &UEPFAsyncReportPlayer::HandleComplete);
}

void UEPFAsyncReportPlayer::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to report player")));
}
