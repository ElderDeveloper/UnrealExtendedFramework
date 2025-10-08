// Fill out your copyright notice in the Description page of Project Settings.


#include "EFQuest.h"
#include "Tasks/EFQuestTask.h"


UEFQuest::UEFQuest()
{
}

void UEFQuest::OnQuestStarted_Implementation(AActor* Owner)
{
}

void UEFQuest::OnQuestCompleted_Implementation(AActor* Owner)
{
}

void UEFQuest::OnQuestFailed_Implementation(AActor* Owner)
{
}

bool UEFQuest::IsCompleted_Implementation()
{
	bool Completed = true;

	for (const TSoftObjectPtr<UEFQuestTask>& TaskPtr : QuestTasks)
	{
		if (TaskPtr.IsValid())
		{
			if (!TaskPtr->IsTaskCompleted())
			{
				Completed = false;
				break;
			}
		}
	}

	return Completed;
}

