// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGMoveForwardNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "UnrealExtendedGameplay/Animation/EGAnimNotifyHelpers.h"

void UEGMoveForwardNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!EGAnimNotify::CanDriveOwnerTransform(MeshComp))
	{
		return;
	}

	AActor* const Owner = MeshComp->GetOwner();
	const FVector Direction = Owner->GetActorQuat().RotateVector(ForwardRotationPlus.Vector());
	Owner->SetActorLocation(Owner->GetActorLocation() + Direction * MoveDistance, bSweep);
}
