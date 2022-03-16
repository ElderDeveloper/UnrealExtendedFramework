// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EGPerceptionComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "UnrealExtendedFramework/Data/EFTraceData.h"
#include "EGPerceptionComponentSight.generated.h"



class UAISenseConfig_Sight;
class UEGPerceptionStimuliSource;


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGPerceptionComponentSight : public UEGPerceptionComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEGPerceptionComponentSight(const FObjectInitializer& ObjectInitializer);

protected:

	UPROPERTY(EditDefaultsOnly)
	TArray<FName> SightCheckBoneNames;

	UPROPERTY(EditDefaultsOnly)
	FLineTraceStruct SightLineTrace;
	
	
	UPROPERTY()
	UAISenseConfig_Sight* SightConfig;

	UPROPERTY()
	UEGPerceptionStimuliSource* HostileStimuliSource;
	FAIStimulus DetectedHostileStimulus;


	void TickDetectPercent(float Strength);


	void CheckCharacterVisibility();
	void CheckActorVisibility();
	

	virtual void EGTargetPerceptionStateChanged(EFPerceptionState State, EFPerceptionFaction Faction, AActor* Source, FAIStimulus Stimulus, bool IsSensed) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;

private:

	float SightDetectionPercent;

public:

	UFUNCTION(BlueprintPure)
	FORCEINLINE float GetSightDetectionPercent() const { return SightDetectionPercent; }


protected:
	
	UPROPERTY()
	AActor* DetectedHostile;
	UPROPERTY()
	ACharacter* DetectedHostileAsCharacter;
};
