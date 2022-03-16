// Fill out your copyright notice in the Description page of Project Settings.


#include "EGMoveForwardNotify.h"

#if ENGINE_MAJOR_VERSION != 5 
void UEGMoveForwardNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::Notify(MeshComp, Animation);
	
	if (const auto actor = Cast<AActor>(MeshComp->GetOwner()))
	{
		const FVector NewLoc = actor->GetActorLocation() + actor->GetActorForwardVector() * MoveDistance;
		actor->SetActorLocation(NewLoc, bSweep   );
	}
}
#endif



#if	ENGINE_MAJOR_VERSION == 5
void UEGMoveForwardNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (const auto actor = Cast<AActor>(MeshComp->GetOwner()))
	{
		const FVector NewLoc = actor->GetActorLocation() + actor->GetActorForwardVector() * MoveDistance;
		actor->SetActorLocation(NewLoc, bSweep   );
	}
}
#endif