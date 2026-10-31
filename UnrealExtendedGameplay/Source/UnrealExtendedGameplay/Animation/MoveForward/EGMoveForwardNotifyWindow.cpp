// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGMoveForwardNotifyWindow.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "UnrealExtendedGameplay/Animation/EGAnimNotifyHelpers.h"

void UEGMoveForwardNotifyWindow::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!EGAnimNotify::CanDriveOwnerTransform(MeshComp))
	{
		return;
	}

	AActor* const Owner = MeshComp->GetOwner();
	const FVector Direction = Owner->GetActorQuat().RotateVector(ForwardRotationPlus.Vector());
	Owner->SetActorLocation(Owner->GetActorLocation() + Direction * (MoveDistance * FrameDeltaTime), bSweep);
}
