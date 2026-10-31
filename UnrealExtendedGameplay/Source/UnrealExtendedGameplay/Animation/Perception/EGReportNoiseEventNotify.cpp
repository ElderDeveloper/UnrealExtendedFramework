// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGReportNoiseEventNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Perception/AISense_Hearing.h"

void UEGReportNoiseEventNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* const Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!Owner)
	{
		return;
	}

	UAISense_Hearing::ReportNoiseEvent(MeshComp, Owner->GetActorLocation(), Loudness, Owner, MaxRange, Tag);

	if (PrintToLog)
	{
		UE_LOG(LogBlueprint, Log, TEXT("Extended Gameplay Report Noise Event Trigger. Tag: %s"), *Tag.ToString());
	}
}
