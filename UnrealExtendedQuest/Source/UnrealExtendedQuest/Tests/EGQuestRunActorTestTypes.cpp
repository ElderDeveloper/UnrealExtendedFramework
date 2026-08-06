// Copyright Devil of the Plague. All Rights Reserved.
#include "EGQuestRunActorTestTypes.h"

TArray<FName> AEGQuestTestRunActor::Recorded;
int32 AEGQuestTestRunActor::SpawnCount = 0;
EEGQuestResult AEGQuestTestRunActor::LastResult = EEGQuestResult::Completed;

void AEGQuestTestRunActor::OnQuestStarted_Implementation()
{
	Recorded.Add(TEXT("QuestStarted"));
}

void AEGQuestTestRunActor::OnStageEntered_Implementation(const FGuid& StageGuid, const FName StageId, const FText& StageTitle)
{
	Recorded.Add(StageId.IsNone() ? FName(TEXT("StageEntered")) : FName(*FString::Printf(TEXT("StageEntered:%s"), *StageId.ToString())));
}

void AEGQuestTestRunActor::OnStageExited_Implementation(const FGuid& StageGuid, const FName StageId, const EEGQuestStageExitReason Reason)
{
	Recorded.Add(TEXT("StageExited"));
}

void AEGQuestTestRunActor::OnObjectiveResolved_Implementation(const FEGQuestSnapshotObjective& Objective)
{
	Recorded.Add(TEXT("ObjectiveResolved"));
}

void AEGQuestTestRunActor::OnQuestEnded_Implementation(const EEGQuestResult Result)
{
	LastResult = Result;
	Recorded.Add(TEXT("QuestEnded"));
}
