// Copyright Kemal Erdem YILMAZ. All Rights Reserved.


#include "EGReportNoiseEventNotify.h"

#include "Perception/AISense_Hearing.h"


#if	ENGINE_MAJOR_VERSION != 5
void UEGReportNoiseEventNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::Notify(MeshComp, Animation);
	UAISense_Hearing::ReportNoiseEvent(MeshComp->GetWorld() , MeshComp->GetOwner()->GetActorLocation() , Loudness , MeshComp->GetOwner() , MaxRange , Tag);
	if (PrintToLog) UE_LOG(LogBlueprint , Log , TEXT("Extended Gameplay Report Noise Event Trigger. Tag: %s") , *Tag.ToString());
}

#endif



#if	ENGINE_MAJOR_VERSION == 5
void UEGReportNoiseEventNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	UAISense_Hearing::ReportNoiseEvent(MeshComp->GetWorld() , MeshComp->GetOwner()->GetActorLocation() , Loudness , MeshComp->GetOwner() , MaxRange , Tag);
	if (PrintToLog) UE_LOG(LogBlueprint , Log , TEXT("Extended Gameplay Report Noise Event Trigger. Tag: %s") , *Tag.ToString());
}
#endif