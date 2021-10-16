﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedSoftLockNotify.h"

#include "../../../../../KontraGamePlugin/Source/KontraGamePlugin/Module/KontraGamePlugin.h"
#include "Kismet/KismetMathLibrary.h"
#include "UEExpandedFramework/Gameplay/Trace/UEExtendedTraceLibrary.h"
#include "UEExpandedFramework/Math/UEExtendedMathLibrary.h"



void UUEExtendedSoftLockNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float TotalDuration)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration);
	FindSoftLockActor(MeshComp);
}


void UUEExtendedSoftLockNotify::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float FrameDeltaTime)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime);
	if (!SoftLockActor)
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
			UUEExtendedMathLibrary::FindLookAtRotationYaw(Owner->GetActorLocation(),SoftLockActor->GetActorLocation()),
			OwnerRot.Roll
		);
		Owner->SetActorRotation(UKismetMathLibrary::RInterpTo(OwnerRot,TargetRotator,FrameDeltaTime,SoftLockInterpSpeed));
	}
}

void UUEExtendedSoftLockNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::NotifyEnd(MeshComp, Animation);
	SoftLockActor=nullptr;
}


void UUEExtendedSoftLockNotify::FindSoftLockActor(USkeletalMeshComponent* MeshComp)
{
	if (!SoftLockActor)
	{
		SoftLockActorTrace.Start = MeshComp->GetOwner()->GetActorLocation();
		
		if(UUEExtendedTraceLibrary::ExtendedSphereTraceMulti(MeshComp->GetWorld(),SoftLockActorTrace))
		{
			TArray<AActor*> HitActorArray;
			
			for ( const auto hit : SoftLockActorTrace.HitResults)
			{
				HitActorArray.Add(hit.GetActor());
			}
			SoftLockActor= UUEExtendedMathLibrary::GetClosestActorFromActorArray(MeshComp->GetOwner(),HitActorArray);


			if (SoftLockActor)
			{
				PRINT_STRING(2.f,Red,SoftLockActor->GetName());
			}
		}
	}
	else
	{
		PRINT_STRING(2.f,Red,"Softlock Actor Valid = " + SoftLockActor->GetName());
	}

	
}