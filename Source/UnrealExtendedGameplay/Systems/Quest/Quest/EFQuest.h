// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "EFQuest.generated.h"

class UEFQuestRequirements;
class UEFQuestTask;
class UEFQuestReward;
/**
 * 
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEFQuest : public UObject
{
	GENERATED_BODY()
	
public:

	UEFQuest();
	
	UFUNCTION(BlueprintNativeEvent, Category = "Quest")
	void OnQuestStarted(AActor* Owner);

	UFUNCTION(BlueprintNativeEvent, Category = "Quest")
	void OnQuestCompleted(AActor* Owner);

	UFUNCTION(BlueprintNativeEvent, Category = "Quest")
	void OnQuestFailed(AActor* Owner);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	FGameplayTag QuestId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	FText Title;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest", meta = (MultiLine = "true"))
	FText ShortDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest", meta = (MultiLine = "true"))
	FText LongDescription;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Quest")
	TArray<TSoftObjectPtr<UEFQuestTask>> QuestTasks;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Quest")
	TArray<TSoftObjectPtr<UEFQuestReward>> QuestRewards;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Quest")
	TArray<TSoftObjectPtr<UEFQuestRequirements>> QuestRequirements;
	

	UFUNCTION(BlueprintNativeEvent,BlueprintPure, Category = "Quest")
	bool IsCompleted();
	

};
