// Fill out your copyright notice in the Description page of Project Settings.


#include "EGMoveForwardNotifyWindow.h"


#if ENGINE_MAJOR_VERSION != 5
void UEGMoveForwardNotifyWindow::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float FrameDeltaTime)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime);

	if (const auto actor = Cast<AActor>(MeshComp->GetOwner()))
	{
		const FVector NewLoc = actor->GetActorLocation() + actor->GetActorForwardVector() * ( MoveDistance * FrameDeltaTime);
		actor->SetActorLocation(NewLoc, bSweep );
	}
}

#endif


#if ENGINE_MAJOR_VERSION == 5
void UEGMoveForwardNotifyWindow::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	
	if (const auto actor = Cast<AActor>(MeshComp->GetOwner()))
	{
		const FVector NewLoc = actor->GetActorLocation() + actor->GetActorForwardVector() * ( MoveDistance * FrameDeltaTime);
		actor->SetActorLocation(NewLoc, bSweep );
	}
}
#endif