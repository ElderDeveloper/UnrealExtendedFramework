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

TMap<TPair<TWeakObjectPtr<AActor>, FName>, int32> UEGWarpTargetNotifyState::OpenWindowCounts;

UEGWarpTargetNotifyState::UEGWarpTargetNotifyState()
{
	// Pawn in the default collision setup.
	SearchObjectTypes.Add(ObjectTypeQuery3);
}

void UEGWarpTargetNotifyState::ApplyWarpTarget(USkeletalMeshComponent* MeshComp, AActor* Target) const
{
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

	if (!EGAnimNotify::CanDriveOwnerTransform(MeshComp))
	{
		return;
	}

	for (auto It = Targets.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = OpenWindowCounts.CreateIterator(); It; ++It)
	{
		if (!It->Key.Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}

	AActor* const Owner = MeshComp->GetOwner();
	++OpenWindowCounts.FindOrAdd(TPair<TWeakObjectPtr<AActor>, FName>(Owner, WarpTargetName));

	AActor* const Target = EGAnimTargeting::FindTarget(Owner, SearchRadius, SearchHalfAngle, SearchObjectTypes);
	Targets.FindOrAdd(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp)).Add(Target);
	ApplyWarpTarget(MeshComp, Target);
}

void UEGWarpTargetNotifyState::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!bTrackTarget || !EGAnimNotify::CanDriveOwnerTransform(MeshComp))
	{
		return;
	}

	// The tick cannot say which overlapping window it is for; follow the newest window's target.
	const TArray<TWeakObjectPtr<AActor>>* MeshTargets = Targets.Find(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp));
	if (MeshTargets && MeshTargets->Num() > 0)
	{
		ApplyWarpTarget(MeshComp, MeshTargets->Last().Get());
	}
}

void UEGWarpTargetNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	TArray<TWeakObjectPtr<AActor>>* MeshTargets = Targets.Find(MeshKey);
	if (!MeshTargets || MeshTargets->Num() == 0)
	{
		// Began on a machine that does not drive the owner; nothing was set here.
		return;
	}
	MeshTargets->RemoveAt(0);
	if (MeshTargets->Num() == 0)
	{
		Targets.Remove(MeshKey);
	}

	AActor* const Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	const TPair<TWeakObjectPtr<AActor>, FName> CountKey(Owner, WarpTargetName);
	int32* const OpenCount = OpenWindowCounts.Find(CountKey);
	const bool bLastWindow = !OpenCount || --(*OpenCount) <= 0;
	if (bLastWindow)
	{
		OpenWindowCounts.Remove(CountKey);
	}

	// Another window with this warp target name is still open on the owner: it owns the target now.
	if (bLastWindow && bClearOnEnd && Owner)
	{
		if (UMotionWarpingComponent* const MotionWarping = Owner->FindComponentByClass<UMotionWarpingComponent>())
		{
			MotionWarping->RemoveWarpTarget(WarpTargetName);
		}
	}
}
