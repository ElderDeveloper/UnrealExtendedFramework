// Fill out your copyright notice in the Description page of Project Settings.


#include "EGSoftLockNotify.h"

#include "Kismet/KismetMathLibrary.h"
#include "UnrealExtendedFramework/Libraries/Math/EFMathLibrary.h"
#include "UnrealExtendedFramework/Libraries/Trace/EFTraceLibrary.h"


void UEGSoftLockNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float TotalDuration)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration);
	FindSoftLockActor(MeshComp);
}


void UEGSoftLockNotify::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float FrameDeltaTime)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime);
	if (SoftLockActor == nullptr)
	{
		FindSoftLockActor(MeshComp);
		return;
	}

	
	if (auto const Owner = MeshComp->GetOwner())
	{
		const auto OwnerRot = Owner->GetActorRotation();
		const FRotator TargetRotator = FRotator
		(
			OwnerRot.Pitch,
			UEFMathLibrary::FindLookAtRotationYaw(Owner->GetActorLocation(),SoftLockActor->GetActorLocation()),
			OwnerRot.Roll
		);
		Owner->SetActorRotation(UKismetMathLibrary::RInterpTo(OwnerRot,TargetRotator,FrameDeltaTime,SoftLockInterpSpeed));
	}
}

void UEGSoftLockNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::NotifyEnd(MeshComp, Animation);
	SoftLockActor=nullptr;
}


void UEGSoftLockNotify::FindSoftLockActor(USkeletalMeshComponent* MeshComp)
{
	if (SoftLockActor == nullptr)
	{
		
		SoftLockActorTrace.Start = MeshComp->GetOwner()->GetActorLocation();
		SoftLockActorTrace.End = MeshComp->GetOwner()->GetActorLocation();
		
		UEFTraceLibrary::ExtendedSphereTraceMulti(MeshComp->GetWorld(),SoftLockActorTrace);
		TArray<AActor*> HitActorArray;
			
		for ( const auto hit : SoftLockActorTrace.HitResults)
		{
			HitActorArray.Add(hit.GetActor());
		}
		
		SoftLockActor= UEFMathLibrary::GetClosestActorFromActorArray(MeshComp->GetOwner(),HitActorArray);
	}
}
	