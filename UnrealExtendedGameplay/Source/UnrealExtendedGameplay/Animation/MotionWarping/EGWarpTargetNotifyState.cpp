// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGWarpTargetNotifyState.h"

#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MotionWarpingComponent.h"
#include "UnrealExtendedGameplay/Animation/Debug/EGAnimDebug.h"
#include "UnrealExtendedGameplay/Animation/EGAnimNotifyHelpers.h"
#include "UnrealExtendedGameplay/Animation/EGAnimTargeting.h"

UEGWarpTargetNotifyState::UEGWarpTargetNotifyState()
{
	// Pawn in the default collision setup.
	SearchObjectTypes.Add(ObjectTypeQuery3);
}

void UEGWarpTargetNotifyState::UpdateWarpTarget(USkeletalMeshComponent* MeshComp, bool bSearch)
{
	if (!EGAnimNotify::CanDriveOwnerTransform(MeshComp))
	{
		return;
	}

	AActor* const Owner = MeshComp->GetOwner();
	UMotionWarpingComponent* const MotionWarping = Owner->FindComponentByClass<UMotionWarpingComponent>();
	if (!MotionWarping)
	{
		const UWorld* World = Owner->GetWorld();
		if (World && World->IsGameWorld())
		{
			EGAnimDebug::WarnOnce(this, FString::Printf(TEXT("%s plays Set Warp Target but has no UMotionWarpingComponent."), *GetNameSafe(Owner->GetClass())));
		}
		return;
	}

	const TWeakObjectPtr<USkeletalMeshComponent> Key(MeshComp);
	AActor* Target = nullptr;
	if (bSearch)
	{
		Target = EGAnimTargeting::FindTarget(Owner, SearchRadius, SearchHalfAngle, SearchObjectTypes);
		Targets.Add(Key, Target);
	}
	else if (const TWeakObjectPtr<AActor>* Existing = Targets.Find(Key))
	{
		Target = Existing->Get();
	}

	if (!IsValid(Target))
	{
		if (bClearWhenNoTarget)
		{
			MotionWarping->RemoveWarpTarget(WarpTargetName);
		}
		return;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	FVector ToOwner = OwnerLocation - TargetLocation;
	ToOwner.Z = 0.0;
	const double Separation = ToOwner.Size();
	const FVector Away = Separation > KINDA_SMALL_NUMBER ? ToOwner / Separation : -Owner->GetActorForwardVector().GetSafeNormal2D();

	// Stop in front of the target, at the character's own height (root motion warps are usually flat).
	FVector Destination = TargetLocation + Away * StopDistance;
	Destination.Z = OwnerLocation.Z;

	if (MaxWarpDistance > 0.0f)
	{
		const FVector Offset = Destination - OwnerLocation;
		if (Offset.SizeSquared() > FMath::Square(MaxWarpDistance))
		{
			Destination = OwnerLocation + Offset.GetSafeNormal() * MaxWarpDistance;
		}
	}

	const FRotator Facing = FRotator(0.0, (TargetLocation - Destination).Rotation().Yaw, 0.0);
	MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName, Destination, Facing);

#if ENABLE_DRAW_DEBUG
	if (EGAnimDebug::IsEnabled())
	{
		DrawDebugDirectionalArrow(Owner->GetWorld(), OwnerLocation, Destination, 40.0f, FColor::Cyan, false, 0.0f, 0, 2.0f);
		DrawDebugSphere(Owner->GetWorld(), TargetLocation, 30.0f, 12, FColor::Orange, false, 0.0f);
	}
#endif
}

void UEGWarpTargetNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	for (auto It = Targets.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
	UpdateWarpTarget(MeshComp, true);
}

void UEGWarpTargetNotifyState::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	if (bTrackTarget)
	{
		UpdateWarpTarget(MeshComp, false);
	}
}

void UEGWarpTargetNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	Targets.Remove(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp));
	if (bClearOnEnd && EGAnimNotify::CanDriveOwnerTransform(MeshComp))
	{
		if (UMotionWarpingComponent* const MotionWarping = MeshComp->GetOwner()->FindComponentByClass<UMotionWarpingComponent>())
		{
			MotionWarping->RemoveWarpTarget(WarpTargetName);
		}
	}
}
