// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "EFGlobalSubsystem.generated.h"






UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFGlobalSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
	
public:
	void SetGlobalActor(FGameplayTag Tag , AActor* Actor);
	AActor* GetGlobalActor(FGameplayTag Tag , bool& Valid);
	void SetGlobalObject(FGameplayTag Tag , UObject* Object);
	UObject* GetGlobalObject(FGameplayTag Tag , bool& Valid);


	UFUNCTION(BlueprintCallable)
	void GetAllGlobalActors(bool Print , TArray<FGameplayTag>& Tags , TArray<AActor*>& Actors  );
	
	UFUNCTION(BlueprintCallable)
	void GetAllGlobalObjects(bool Print ,TArray<FGameplayTag>& Tags , TArray<UObject*>& Objects );

	
	UPROPERTY(BlueprintReadWrite)
	TMap<FGameplayTag , AActor*> EFGlobalActors;
	
	UPROPERTY(BlueprintReadWrite)
	TMap<FGameplayTag , UObject*> EFGlobalObjects;
	
};
