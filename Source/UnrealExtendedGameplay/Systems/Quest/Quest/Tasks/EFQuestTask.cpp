// Fill out your copyright notice in the Description page of Project Settings.


#include "EFQuestTask.h"

void UEFQuestTask::IncrementProgress(int32 Amount)
{
	CurrentProgress += Amount;
	if (CurrentProgress > TargetProgress)
	{
		CurrentProgress = TargetProgress;
	}
}

bool UEFQuestTask::IsTaskCompleted() const
{
	return CurrentProgress >= TargetProgress;
}
