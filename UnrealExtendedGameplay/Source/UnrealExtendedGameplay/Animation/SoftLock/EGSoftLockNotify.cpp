// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSoftLockNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetMathLibrary.h"
#include "UnrealExtendedFramework/Libraries/Math/EFMathLibrary.h"
#include "UnrealExtendedFramework/Libraries/Trace/EFTraceLibrary.h"
#include "UnrealExtendedGameplay/Animation/EGAnimNotifyHelpers.h"
#include "UnrealExtendedGameplay/Animation/EGAnimTargeting.h"
#include "UnrealExtendedGameplay/Animation/TurnInPlace/EGTurnInPlaceComponent.h"

void UEGSoftLockNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (EGAnimNotify::CanDriveOwnerTransform(MeshComp))
	{
		GetSoftLockActor(MeshComp);

		if (UEGTurnInPlaceComponent* const TurnInPlace = UEGTurnInPlaceComponent::FindFor(MeshComp->GetOwner()))
		{
			TurnInPlacePauses.FindOrAdd(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp)).Add(TurnInPlace->PushPause(TEXT("SoftLock")));
		}
	}
}

void UEGSoftLockNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	ClearSoftLockActor(MeshComp);

	const TWeakObjectPtr<USkeletalMeshComponent> Key(MeshComp);
	if (TArray<FEGTurnInPlaceRequestHandle>* Pauses = TurnInPlacePauses.Find(Key))
	{
		if (Pauses->Num() > 0)
		{
			FEGTurnInPlaceRequestHandle Handle = (*Pauses)[0];
			Pauses->RemoveAt(0);
			if (UEGTurnInPlaceComponent* const TurnInPlace = MeshComp ? UEGTurnInPlaceComponent::FindFor(MeshComp->GetOwner()) : nullptr)
			{
				TurnInPlace->RemoveRequest(Handle);
			}
		}
		if (Pauses->Num() == 0)
		{
			TurnInPlacePauses.Remove(Key);
		}
	}
}

void UEGSoftLockNotify::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	TickSoftLock(MeshComp, FrameDeltaTime);
}

void UEGSoftLockNotify::TickSoftLock(USkeletalMeshComponent* MeshComp, float FrameDeltaTime)
{
	if (!IsValid(MeshComp))
	{
		return;
	}

	AActor* const Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		ClearSoftLockActor(MeshComp);
		return;
	}

	// Simulated proxies receive the rotation from the server.
	if (!EGAnimNotify::CanDriveOwnerTransform(MeshComp))
	{
		return;
	}

	AActor* const SoftLockActor = GetSoftLockActor(MeshComp);
	if (!IsValid(SoftLockActor) || SoftLockActor == Owner)
	{
		return;
	}

	const FRotator OwnerRot = Owner->GetActorRotation();
	const FRotator TargetRotator = FRotator
	{
		OwnerRot.Pitch,
		UEFMathLibrary::FindLookAtRotationYaw(Owner->GetActorLocation(), SoftLockActor->GetActorLocation()),
		OwnerRot.Roll
	};
	Owner->SetActorRotation(UKismetMathLibrary::RInterpTo(OwnerRot, TargetRotator, FrameDeltaTime, SoftLockInterpSpeed));
}

AActor* UEGSoftLockNotify::GetSoftLockActor(USkeletalMeshComponent* MeshComp)
{
	if (!IsValid(MeshComp))
	{
		return nullptr;
	}

	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	if (const TWeakObjectPtr<AActor>* ExistingActor = SoftLockActors.Find(MeshKey))
	{
		if (ExistingActor->IsValid())
		{
			return ExistingActor->Get();
		}

		SoftLockActors.Remove(MeshKey);
	}

	AActor* const FoundActor = FindSoftLockActor(MeshComp);
	if (IsValid(FoundActor))
	{
		SoftLockActors.Add(MeshKey, FoundActor);
	}

	return FoundActor;
}

void UEGSoftLockNotify::ClearSoftLockActor(USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp)
	{
		return;
	}

	SoftLockActors.Remove(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp));
}

AActor* UEGSoftLockNotify::FindSoftLockActor(USkeletalMeshComponent* MeshComp) const
{
	if (!IsValid(MeshComp))
	{
		return nullptr;
	}

	AActor* const Owner = MeshComp->GetOwner();
	UWorld* const World = MeshComp->GetWorld();
	if (!IsValid(Owner) || World == nullptr)
	{
		return nullptr;
	}

	// Agree with the lock-on systems before running a search of our own.
	if (AActor* const Locked = EGAnimTargeting::GetLockedTarget(Owner))
	{
		return Locked;
	}

	FSphereTraceStruct SoftLockTrace = SoftLockActorTrace;
	SoftLockTrace.Start = Owner->GetActorLocation();
	SoftLockTrace.End = Owner->GetActorLocation();

	// The mesh as world context lets bIgnoreSelf skip the owner.
	UEFTraceLibrary::ExtendedSphereTraceMulti(MeshComp, SoftLockTrace);

	TArray<AActor*> HitActorArray;
	HitActorArray.Reserve(SoftLockTrace.HitResults.Num());

	for (const FHitResult& Hit : SoftLockTrace.HitResults)
	{
		APawn* const Pawn = Cast<APawn>(Hit.GetActor());
		if (IsValid(Pawn) && Pawn != Owner)
		{
			HitActorArray.Add(Pawn);
		}
	}

	return UEFMathLibrary::GetClosestActorFromActorArray(Owner, HitActorArray);
}
	