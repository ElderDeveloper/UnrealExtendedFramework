// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "EGQuestManager.generated.h"

class UEFQuest;
class UEFQuestTask;
class UEFQuestRequirements;
class UEFQuestReward;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestStateChanged, UEFQuest*, Quest);

// Serializable quest state for saving/loading
USTRUCT(BlueprintType)
struct FQuestSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite)
	FGameplayTag QuestId;

	UPROPERTY(SaveGame, BlueprintReadWrite)
	bool bCompleted = false;

	UPROPERTY(SaveGame, BlueprintReadWrite)
	bool bFailed = false;

	// Map of TaskId -> CurrentProgress
	UPROPERTY(SaveGame, BlueprintReadWrite)
	TMap<FString, int32> TaskProgress;
};

// Container for all quest save data
USTRUCT(BlueprintType)
struct FQuestManagerSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite)
	TArray<FQuestSaveData> ActiveQuests;

	UPROPERTY(SaveGame, BlueprintReadWrite)
	TArray<FGameplayTag> CompletedQuests;

	UPROPERTY(SaveGame, BlueprintReadWrite)
	TArray<FGameplayTag> FailedQuests;
};


UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="UEF Quest System Settings"))
class UEGQuestSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UEGQuestSettings()
	{
		bAutoDiscoverQuests = true;
	}

	// If true, will scan /Game for UEFQuest assets on BeginPlay of the manager
	UPROPERTY(EditAnywhere, Config, Category="Quest")
	bool bAutoDiscoverQuests;

	// Map of registered quests by their GameplayTag identifier
	UPROPERTY(VisibleAnywhere, Config, Category="Quest")
	TMap<FGameplayTag, TSoftObjectPtr<UEFQuest>> RegisteredQuests;

	static const UEGQuestSettings* Get()
	{
		return GetDefault<UEGQuestSettings>();
	}
	
};

USTRUCT(blueprintType)
struct FEGQuest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuestId;

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UEFQuest> QuestInstance = nullptr;

	FEGQuest() = default;
	FEGQuest(FGameplayTag InQuestId, UEFQuest* InQuestInstance)
		: QuestId(InQuestId), QuestInstance(InQuestInstance)
	{
	}

	bool operator == (const FGameplayTag& Other) const
	{
		return QuestId == Other;
	}
};



UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGQuestManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UEGQuestManager();

	UPROPERTY(BlueprintAssignable, Category = "Quest")
	FOnQuestStateChanged OnQuestUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Quest")
	FOnQuestStateChanged OnQuestStarted;

	UPROPERTY(BlueprintAssignable, Category = "Quest")
	FOnQuestStateChanged OnQuestCompleted;

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// Start a quest by its GameplayTag identifier
	UFUNCTION(BlueprintCallable, Category="Quest|Manager")
	bool StartQuestByTag(FGameplayTag QuestId);

	// Complete an active quest by tag, grant rewards
	UFUNCTION(BlueprintCallable, Category="Quest|Manager")
	bool CompleteQuestByTag(FGameplayTag QuestId);

	// Fail an active quest by tag
	UFUNCTION(BlueprintCallable, Category="Quest|Manager")
	bool FailQuestByTag(FGameplayTag QuestId);

	// Add progress to a task within a quest by task id string
	UFUNCTION(BlueprintCallable, Category="Quest|Manager")
	bool AddTaskProgress(FGameplayTag QuestId, const FString& TaskId, int32 Amount);

	// Query if quest is active
	UFUNCTION(BlueprintPure, Category="Quest|Manager")
	bool IsQuestActive(FGameplayTag QuestId) const;

	// Get a currently active quest object
	UFUNCTION(BlueprintPure, Category="Quest|Manager")
	UEFQuest* GetActiveQuest(FGameplayTag QuestId) const;

	// Check if quest was completed
	UFUNCTION(BlueprintPure, Category="Quest|Manager")
	bool IsQuestCompleted(FGameplayTag QuestId) const;

	// Save/Load Functions
	UFUNCTION(BlueprintCallable, Category="Quest|Manager|Save")
	FQuestManagerSaveData SaveQuestState() const;

	UFUNCTION(BlueprintCallable, Category="Quest|Manager|Save")
	void LoadQuestState(const FQuestManagerSaveData& SaveData);

	// Server RPC for multiplayer
	UFUNCTION(Server, Reliable, Category="Quest|Manager")
	void ServerStartQuest(FGameplayTag QuestId);

	UFUNCTION(Server, Reliable, Category="Quest|Manager")
	void ServerCompleteQuest(FGameplayTag QuestId);

	UFUNCTION(Server, Reliable, Category="Quest|Manager")
	void ServerFailQuest(FGameplayTag QuestId);

	UFUNCTION(Server, Reliable, Category="Quest|Manager")
	void ServerAddTaskProgress(FGameplayTag QuestId, const FString& TaskId, int32 Amount);

private:
	// Active quest instances keyed by tag
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UEFQuest>> ActiveQuests;

	UPROPERTY(Transient, ReplicatedUsing=OnRep_ActiveQuests)
	TArray<FEGQuest> ReplicatedActiveQuests;

	// Track completed quests
	UPROPERTY(Replicated, SaveGame)
	TArray<FGameplayTag> CompletedQuestIds;

	// Track failed quests
	UPROPERTY(Replicated, SaveGame)
	TArray<FGameplayTag> FailedQuestIds;

	void DiscoverQuestAssets();
	UEFQuest* LoadQuestByTag(const FGameplayTag& QuestId) const;

	// Replication callbacks
	UFUNCTION()
	void OnRep_ActiveQuests();

	FEGQuest* FindReplicatedQuest(FGameplayTag QuestId);
	int32 FindReplicatedQuestIndex(FGameplayTag QuestId);
};
