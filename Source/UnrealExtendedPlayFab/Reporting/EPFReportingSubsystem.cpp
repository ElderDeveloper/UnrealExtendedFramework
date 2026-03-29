// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFReportingSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

void UEPFReportingSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UEPFReportingSubsystem::Deinitialize() { Super::Deinitialize(); }

void UEPFReportingSubsystem::ReportPlayer(const FString& ReporteePlayFabId, const FString& Comment)
{
	if (ReporteePlayFabId.IsEmpty()) { OnPlayerReported.Broadcast(FEPFResult::Failure(TEXT("ReporteePlayFabId cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("ReporteeId"), ReporteePlayFabId);
	if (!Comment.IsEmpty()) Body->SetStringField(TEXT("Comment"), Comment);

	SendPlayFabRequestDetailed(TEXT("/Client/ReportPlayer"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this, ReporteePlayFabId](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFReporting — Player reported: %s"), *ReporteePlayFabId);
			OnPlayerReported.Broadcast(Result);
		}));
}
