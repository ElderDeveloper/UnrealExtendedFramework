// Fill out your copyright notice in the Description page of Project Settings.


#include "EGQuestManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Quest/EFQuest.h"
#include "Quest/Requirements/EFQuestRequirements.h"
#include "Quest/Reward/EFQuestReward.h"
#include "Quest/Tasks/EFQuestTask.h"
#include "Net/UnrealNetwork.h"

// Sets default values for this component's properties
UEGQuestManager::UEGQuestManager()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UEGQuestManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEGQuestManager, ReplicatedActiveQuests);
	DOREPLIFETIME(UEGQuestManager, CompletedQuestIds);
	DOREPLIFETIME(UEGQuestManager, FailedQuestIds);
}

void UEGQuestManager::BeginPlay()
{
	Super::BeginPlay();
	
	// Only discover quests on server or in standalone
	if (GetOwnerRole() == ROLE_Authority)
	{
		DiscoverQuestAssets();
	}
}

void UEGQuestManager::DiscoverQuestAssets()
{
    UEGQuestSettings* Settings = GetMutableDefault<UEGQuestSettings>();
    if (!Settings)
    {
        return;
    }
    Settings->RegisteredQuests.Empty();

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    // Ensure asset registry has scanned all paths
    TArray<FString> PathsToScan;
    PathsToScan.Add(TEXT("/Game"));
    AssetRegistry.ScanPathsSynchronous(PathsToScan);

    // Find all Blueprint assets that are children of UEFQuest
    FARFilter Filter;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    Filter.bRecursivePaths = true;
    
    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);

    for (const FAssetData& AssetData : AssetDataList)
    {
        // Check if this blueprint's parent class is UEFQuest or derived from it
        FAssetDataTagMapSharedView::FFindTagResult GeneratedClassPath = AssetData.TagsAndValues.FindTag(TEXT("GeneratedClass"));
        if (!GeneratedClassPath.IsSet())
        {
            continue;
        }

        FString ClassPath = GeneratedClassPath.GetValue();
        FTopLevelAssetPath AssetClassPath(ClassPath);
        
        // Load the blueprint to check if it's a Quest
        if (UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset()))
        {
            if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(UEFQuest::StaticClass()))
            {
                // Get the CDO to extract the QuestId
                UEFQuest* QuestCDO = Cast<UEFQuest>(Blueprint->GeneratedClass->GetDefaultObject());
                if (QuestCDO && QuestCDO->QuestId.IsValid())
                {
                    TSoftObjectPtr<UEFQuest> SoftPtr(AssetData.ToSoftObjectPath());
                    Settings->RegisteredQuests.Add(QuestCDO->QuestId, SoftPtr);
                }
            }
        }
    }
}

bool UEGQuestManager::StartQuestByTag(FGameplayTag QuestId)
{
	// In multiplayer, call server RPC
	if (GetOwnerRole() != ROLE_Authority)
	{
		ServerStartQuest(QuestId);
		return true;
	}

	if (!QuestId.IsValid() || ActiveQuests.Contains(QuestId))
	{
		return false;
	}

	// Don't start if already completed
	if (CompletedQuestIds.Contains(QuestId))
	{
		return false;
	}

	UEFQuest* QuestCDO = LoadQuestByTag(QuestId);
	if (!QuestCDO)
	{
		return false;
	}

	// Create a transient instance so we can track per-player progress
	UEFQuest* QuestInstance = NewObject<UEFQuest>(this, QuestCDO->GetClass());
	if (!QuestInstance)
	{
		return false;
	}

	// Optionally validate requirements
	AActor* OwnerActor = GetOwner();
	bool bRequirementsMet = true;
	for (const TSoftObjectPtr<UEFQuestRequirements>& ReqPtr : QuestCDO->QuestRequirements)
	{
		if (ReqPtr.IsValid())
		{
			if (!ReqPtr->IsRequirementsMet(OwnerActor))
			{
				bRequirementsMet = false;
				break;
			}
		}
	}
	if (!bRequirementsMet)
	{
		return false;
	}

	ActiveQuests.Add(QuestId, QuestInstance);
	ReplicatedActiveQuests.Add(FEGQuest(QuestId, QuestInstance));
	QuestInstance->OnQuestStarted(OwnerActor);
	OnQuestStarted.Broadcast(QuestInstance);
	return true;
}

bool UEGQuestManager::CompleteQuestByTag(FGameplayTag QuestId)
{
	// In multiplayer, call server RPC
	if (GetOwnerRole() != ROLE_Authority)
	{
		ServerCompleteQuest(QuestId);
		return true;
	}

	TObjectPtr<UEFQuest>* Found = ActiveQuests.Find(QuestId);
	if (!Found || !*Found)
	{
		return false;
	}
	UEFQuest* Quest = *Found;

	// Grant rewards from the CDO to avoid per-instance duplication beyond one-time grant
	UEFQuest* QuestCDO = LoadQuestByTag(QuestId);
	AActor* OwnerActor = GetOwner();
	if (QuestCDO)
	{
		for (TSoftObjectPtr<UEFQuestReward> RewardPtr : QuestCDO->QuestRewards)
		{
			if (RewardPtr.IsValid())
			{
				RewardPtr->GrantReward(OwnerActor);
			}
		}
	}

	Quest->OnQuestCompleted(OwnerActor);
	
	// Add to completed list
	CompletedQuestIds.AddUnique(QuestId);
	
	ActiveQuests.Remove(QuestId);
	const int32 Index = FindReplicatedQuestIndex(QuestId);
	if (Index != INDEX_NONE)
	{
		ReplicatedActiveQuests.RemoveAt(Index);
	}
	OnQuestCompleted.Broadcast(Quest);
	return true;
}

bool UEGQuestManager::FailQuestByTag(FGameplayTag QuestId)
{
	// In multiplayer, call server RPC
	if (GetOwnerRole() != ROLE_Authority)
	{
		ServerFailQuest(QuestId);
		return true;
	}

	TObjectPtr<UEFQuest>* Found = ActiveQuests.Find(QuestId);
	if (!Found || !*Found)
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	(*Found)->OnQuestFailed(OwnerActor);
	
	// Add to failed list
	FailedQuestIds.AddUnique(QuestId);
	
	ActiveQuests.Remove(QuestId);
	const int32 Index = FindReplicatedQuestIndex(QuestId);
	if (Index != INDEX_NONE)
	{
		ReplicatedActiveQuests.RemoveAt(Index);
	}
	return true;
}

bool UEGQuestManager::AddTaskProgress(FGameplayTag QuestId, const FString& TaskId, int32 Amount)
{
	// In multiplayer, call server RPC
	if (GetOwnerRole() != ROLE_Authority)
	{
		ServerAddTaskProgress(QuestId, TaskId, Amount);
		return true;
	}

	TObjectPtr<UEFQuest>* Found = ActiveQuests.Find(QuestId);
	if (!Found || !*Found)
	{
		return false;
	}

	UEFQuest* Quest = *Found;
	bool bUpdated = false;

	// Iterate tasks on the instance; if none exist yet, clone from CDO
	if (Quest->QuestTasks.Num() == 0)
	{
		if (UEFQuest* QuestCDO = LoadQuestByTag(QuestId))
		{
			Quest->QuestTasks = QuestCDO->QuestTasks;
		}
	}

	for (TSoftObjectPtr<UEFQuestTask>& TaskPtr : Quest->QuestTasks)
	{
		if (TaskPtr.IsValid())
		{
			UEFQuestTask* Task = TaskPtr.Get();
			if (Task->TaskId == TaskId)
			{
				Task->IncrementProgress(Amount);
				bUpdated = true;
			}
		}
	}

	// Auto-complete if all tasks complete
	if (bUpdated)
	{
		OnQuestUpdated.Broadcast(Quest);
		
		if (UEFQuest* QuestObj = *Found)
		{
			if (QuestObj->IsCompleted())
			{
				CompleteQuestByTag(QuestId);
			}
		}
	}

	return bUpdated;
}

bool UEGQuestManager::IsQuestActive(FGameplayTag QuestId) const
{
	return ActiveQuests.Contains(QuestId);
}

UEFQuest* UEGQuestManager::GetActiveQuest(FGameplayTag QuestId) const
{
	if (const TObjectPtr<UEFQuest>* Found = ActiveQuests.Find(QuestId))
	{
		return Found->Get();
	}
	return nullptr;
}

UEFQuest* UEGQuestManager::LoadQuestByTag(const FGameplayTag& QuestId) const
{
	const UEGQuestSettings* Settings = UEGQuestSettings::Get();
	if (!Settings)
	{
		return nullptr;
	}
	if (const TSoftObjectPtr<UEFQuest>* SoftPtr = Settings->RegisteredQuests.Find(QuestId))
	{
		return Cast<UEFQuest>(SoftPtr->LoadSynchronous());
	}
	return nullptr;
}

bool UEGQuestManager::IsQuestCompleted(FGameplayTag QuestId) const
{
	return CompletedQuestIds.Contains(QuestId);
}

FQuestManagerSaveData UEGQuestManager::SaveQuestState() const
{
	FQuestManagerSaveData SaveData;

	// Save active quests with their progress
	for (const auto& Pair : ActiveQuests)
	{
		const FGameplayTag& QuestId = Pair.Key;
		UEFQuest* Quest = Pair.Value;

		if (Quest)
		{
			FQuestSaveData QuestSave;
			QuestSave.QuestId = QuestId;
			QuestSave.bCompleted = false;
			QuestSave.bFailed = false;

			// Save task progress
			for (const TSoftObjectPtr<UEFQuestTask>& TaskPtr : Quest->QuestTasks)
			{
				if (TaskPtr.IsValid())
				{
					UEFQuestTask* Task = TaskPtr.Get();
					QuestSave.TaskProgress.Add(Task->TaskId, Task->CurrentProgress);
				}
			}

			SaveData.ActiveQuests.Add(QuestSave);
		}
	}

	// Save completed and failed quest lists
	SaveData.CompletedQuests = CompletedQuestIds;
	SaveData.FailedQuests = FailedQuestIds;

	return SaveData;
}

void UEGQuestManager::LoadQuestState(const FQuestManagerSaveData& SaveData)
{
	// Only allow loading on authority
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	// Clear current state
	ActiveQuests.Empty();
	ReplicatedActiveQuests.Empty();
	CompletedQuestIds = SaveData.CompletedQuests;
	FailedQuestIds = SaveData.FailedQuests;

	// Restore active quests
	for (const FQuestSaveData& QuestSave : SaveData.ActiveQuests)
	{
		UEFQuest* QuestCDO = LoadQuestByTag(QuestSave.QuestId);
		if (!QuestCDO)
		{
			continue;
		}

		// Create instance
		UEFQuest* QuestInstance = NewObject<UEFQuest>(this, QuestCDO->GetClass());
		if (!QuestInstance)
		{
			continue;
		}

		// Restore task progress
		for (TSoftObjectPtr<UEFQuestTask>& TaskPtr : QuestInstance->QuestTasks)
		{
			if (TaskPtr.IsValid())
			{
				UEFQuestTask* Task = TaskPtr.Get();
				if (const int32* Progress = QuestSave.TaskProgress.Find(Task->TaskId))
				{
					Task->CurrentProgress = *Progress;
				}
			}
		}

		ActiveQuests.Add(QuestSave.QuestId, QuestInstance);
		ReplicatedActiveQuests.Add(FEGQuest(QuestSave.QuestId, QuestInstance));
	}
}

// Server RPC Implementations
void UEGQuestManager::ServerStartQuest_Implementation(FGameplayTag QuestId)
{
	StartQuestByTag(QuestId);
}

void UEGQuestManager::ServerCompleteQuest_Implementation(FGameplayTag QuestId)
{
	CompleteQuestByTag(QuestId);
}

void UEGQuestManager::ServerFailQuest_Implementation(FGameplayTag QuestId)
{
	FailQuestByTag(QuestId);
}

void UEGQuestManager::ServerAddTaskProgress_Implementation(FGameplayTag QuestId, const FString& TaskId, int32 Amount)
{
	AddTaskProgress(QuestId, TaskId, Amount);
}

void UEGQuestManager::OnRep_ActiveQuests()
{
	ActiveQuests.Empty();
	for (const FEGQuest& QuestData : ReplicatedActiveQuests)
	{
		ActiveQuests.Add(QuestData.QuestId, QuestData.QuestInstance);
	}
}


FEGQuest* UEGQuestManager::FindReplicatedQuest(FGameplayTag QuestId)
{
	return ReplicatedActiveQuests.FindByPredicate([QuestId](const FEGQuest& Quest){ return Quest.QuestId == QuestId; });
}

int32 UEGQuestManager::FindReplicatedQuestIndex(FGameplayTag QuestId)
{
	return ReplicatedActiveQuests.IndexOfByPredicate([QuestId](const FEGQuest& Quest){ return Quest.QuestId == QuestId; });
}
