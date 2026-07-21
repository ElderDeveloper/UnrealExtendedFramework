// Fill out your copyright notice in the Description page of Project Settings.


#include "EGPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "UnrealExtendedGameplay/AI/StimuliSource/EGPerceptionStimuliSource.h"




void UEGPerceptionComponent::StartDetectionLooseDelay()
{
	if(!DetectionLooseTimerHande.IsValid())
		GetWorld()->GetTimerManager().SetTimer(DetectionLooseTimerHande,this,&UEGPerceptionComponent::OnDetectionLooseDelay,DetectionDropDelay,false);
}

void UEGPerceptionComponent::OnDetectionLooseDelay()
{
	if (!DidDetectHostile)
		FullyDetectedHostile = false;
}



void UEGPerceptionComponent:: ReceivePerceptionUpdated(const TArray<AActor*>& UpdatedActors)
{
	EGOnPerceptionUpdated(UpdatedActors);
	BP_OnPerceptionUpdated(UpdatedActors);
}




void UEGPerceptionComponent::ReceiveOnTargetPerceptionUpdate(AActor* SourceActor, FAIStimulus Stimulus)
{
	if (SourceActor)
	{
		if (const auto EGStimuli = SourceActor->FindComponentByClass<UEGPerceptionStimuliSource>())
		{
			const bool IsDetectionSuccess = Stimulus.WasSuccessfullySensed();
			if (EGStimuli->GetIsHostile(ActorHostileFactions))
			{
				OnTargetPerceptionStateChanged.Broadcast(EFP_VaguelySensed,Hostile,SourceActor,Stimulus,IsDetectionSuccess);
				EGTargetPerceptionStateChanged(EFP_VaguelySensed,Hostile,SourceActor,Stimulus,IsDetectionSuccess);
				return;
			}
			if (EGStimuli->GetIsAlly(ActorAllyFactions))
			{
				OnTargetPerceptionStateChanged.Broadcast(EFP_VaguelySensed,Ally,SourceActor,Stimulus,IsDetectionSuccess);
				EGTargetPerceptionStateChanged(EFP_VaguelySensed,Ally,SourceActor,Stimulus,IsDetectionSuccess);
				return;
			}
			OnTargetPerceptionStateChanged.Broadcast(EFP_VaguelySensed,Neutral,SourceActor,Stimulus,IsDetectionSuccess);
			EGTargetPerceptionStateChanged(EFP_VaguelySensed,Neutral,SourceActor,Stimulus,IsDetectionSuccess);
		}
		
		EGOnPerceptionOnActorUpdated(SourceActor,Stimulus);
		BP_OnPerceptionOnActorUpdated(SourceActor,Stimulus);
	}
}




void UEGPerceptionComponent::ReceiveOnTargetInfoUpdate(const FActorPerceptionUpdateInfo& UpdateInfo)
{
}



void UEGPerceptionComponent::BeginPlay()
{
	Super::BeginPlay();
	
	OnPerceptionUpdated.AddDynamic(this,&UEGPerceptionComponent::ReceivePerceptionUpdated);
	OnTargetPerceptionUpdated.AddDynamic(this,&UEGPerceptionComponent::ReceiveOnTargetPerceptionUpdate);
	OnTargetPerceptionInfoUpdated.AddDynamic(this,&UEGPerceptionComponent::ReceiveOnTargetInfoUpdate);
}


