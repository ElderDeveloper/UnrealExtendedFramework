// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "UnrealExtendedFramework/Data/EFEnums.h"
#include "EGPerceptionComponent.generated.h"



DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnTargetPerceptionStateChanged, EFPerceptionState , State , EFPerceptionFaction ,  Faction ,  AActor* , Source , FAIStimulus , Stimulus , bool , IsSensed );





UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGPerceptionComponent : public UAIPerceptionComponent
{
	GENERATED_BODY()

public:
	

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Factions")
	FGameplayTagContainer ActorAllyFactions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Factions")
	FGameplayTagContainer ActorHostileFactions;


	
	
	UPROPERTY(EditDefaultsOnly , BlueprintReadWrite , Category="Detection")
	float DetectionFillRateMin;

	UPROPERTY(EditDefaultsOnly , BlueprintReadWrite , Category="Detection")
	float DetectionFillRateMax;

	UPROPERTY(EditDefaultsOnly , BlueprintReadWrite , Category="Detection")
	float DetectionDropDelay = 4;

	UPROPERTY(EditDefaultsOnly , BlueprintReadWrite , Category="Detection")
	float DetectionDropRate = 0.01;
	
	UPROPERTY(EditDefaultsOnly , BlueprintReadWrite , Category="Detection")
	float DetectionDistanceMin;

	UPROPERTY(EditDefaultsOnly , BlueprintReadWrite , Category="Detection")
	float DetectionDistanceMax;
	

	virtual void EGOnPerceptionUpdated(const TArray<AActor*>& UpdatedActors) {}
	virtual void EGOnPerceptionOnActorUpdated(AActor* Actor, FAIStimulus Stimulus) {}
	

	virtual void EGTargetPerceptionStateChanged(EFPerceptionState State , EFPerceptionFaction  Faction ,  AActor* Source , FAIStimulus Stimulus , bool IsSensed ) { }

	
	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnPerceptionUpdated(const TArray<AActor*>& UpdatedActors);
	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnPerceptionOnActorUpdated(AActor* Actor, FAIStimulus Stimulus);


	
protected:


	bool DidDetectHostile;
	bool DidDetectNeutral;
	bool DidDetectAlly;

	bool FullyDetectedHostile;
	bool FullyDetectedAlly;

	FAIStimulus HostileStimulus;
	FAIStimulus AllyStimulus;

	FTimerHandle DetectionLooseTimerHande;


	void StartDetectionLooseDelay();
	void OnDetectionLooseDelay();
	

	UFUNCTION()
	void ReceivePerceptionUpdated(const TArray<AActor*>& UpdatedActors);

	/**
	 * Note - This delegate will not be called if source actor is no longer valid 
	 * by the time a stimulus gets processed. 
	 * Use OnTargetPerceptionInfoUpdated providing a source id to handle those cases.
	 * @param	SourceActor	Actor associated to the stimulus (can not be null)
	 * @param	Stimulus	Updated stimulus
	 */
	UFUNCTION()
	void ReceiveOnTargetPerceptionUpdate( AActor* SourceActor, FAIStimulus Stimulus);
	
	/**
	 * Note - This delegate will be called even if source actor is no longer valid 
	 * by the time a stimulus gets processed so it is better to use source id for bookkeeping.
	 *
	 * @param	UpdateInfo	Data structure providing information related to the updated perceptual data
	 */
	UFUNCTION()
	void ReceiveOnTargetInfoUpdate(const FActorPerceptionUpdateInfo& UpdateInfo);


	
	virtual void BeginPlay() override;

	
public:
	
	UPROPERTY(BlueprintAssignable)
	FOnTargetPerceptionStateChanged OnTargetPerceptionStateChanged;
	
};



/*
In DefaultGame.ini, I’ve set:
[/Script/AIModule.AISense_Hearing]
bAutoRegisterAllPawnsAsSources=true

And I even do it manually also on each actor OnBegin:
UAIPerceptionSystem::RegisterPerceptionStimuliSource(this, UAISense_Hearing::StaticClass(), this);
*/