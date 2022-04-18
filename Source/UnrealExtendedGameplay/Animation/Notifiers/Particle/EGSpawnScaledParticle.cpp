// Fill out your copyright notice in the Description page of Project Settings.


#include "EGSpawnScaledParticle.h"
#include "Particles/ParticleSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "ParticleHelper.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Curves/CurveVector.h"
#include "Kismet/KismetMathLibrary.h"

UEGSpawnScaledParticle::UEGSpawnScaledParticle() : Super()
{
	Attached = true;

#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(192, 255, 99, 255);
#endif // WITH_EDITORONLY_DATA
}

#if ENGINE_MAJOR_VERSION == 5
void UEGSpawnScaledParticle::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float TotalDuration)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration);

	RotationOffsetQuat = FQuat(RotationOffset);
	CurrentTime = 0;
	
	if (PSTemplate)
	{
		if (Attached)
			ParticleSystem = UGameplayStatics::SpawnEmitterAttached(PSTemplate, MeshComp, SocketName, LocationOffset, RotationOffset, StartingScale);
		else
		{
			const FTransform MeshTransform = MeshComp->GetSocketTransform(SocketName);
			FTransform SpawnTransform;
			SpawnTransform.SetLocation(MeshTransform.TransformPosition(LocationOffset));
			SpawnTransform.SetRotation(MeshTransform.GetRotation() * RotationOffsetQuat);
			SpawnTransform.SetScale3D(StartingScale);
			ParticleSystem = UGameplayStatics::SpawnEmitterAtLocation(MeshComp->GetWorld(), PSTemplate, SpawnTransform);
		}
	}
	else
	{
		UE_LOG(LogBlueprint, Warning, TEXT("Particle Notify: Particle system is null for particle notify '%s' in anim: '%s'"), *GetNotifyName(), *GetPathNameSafe(Animation));
	}
}

void UEGSpawnScaledParticle::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (ParticleSystem)
	{
		CurrentTime += FrameDeltaTime;
		FVector TargetSale;

		if (ScaleCurve)
			TargetSale = ScaleCurve->GetVectorValue(CurrentTime);
		else
			TargetSale = UKismetMathLibrary::VInterpTo(ParticleSystem->GetComponentScale() , EndScale ,FrameDeltaTime,ScaleInterpSpeed);
		
		ParticleSystem->SetRelativeScale3D(TargetSale);
		
	}
	
}

void UEGSpawnScaledParticle::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (ParticleSystem)
		ParticleSystem->DestroyComponent();
}

#endif


#if ENGINE_MAJOR_VERSION != 5

void UEGSpawnScaledParticle::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime);

	if (ParticleSystem)
	{
		CurrentTime += FrameDeltaTime;
		FVector TargetSale;

		if (ScaleCurve)
			TargetSale = ScaleCurve->GetVectorValue(CurrentTime);
		else
			TargetSale = UKismetMathLibrary::VInterpTo(ParticleSystem->GetComponentScale(), EndScale, FrameDeltaTime, ScaleInterpSpeed);

		ParticleSystem->SetRelativeScale3D(TargetSale);

	}
}


void UEGSpawnScaledParticle::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration);

	RotationOffsetQuat = FQuat(RotationOffset);
	CurrentTime = 0;

	if (PSTemplate)
	{
		if (Attached)
			ParticleSystem = UGameplayStatics::SpawnEmitterAttached(PSTemplate, MeshComp, SocketName, LocationOffset, RotationOffset, StartingScale);
		else
		{
			const FTransform MeshTransform = MeshComp->GetSocketTransform(SocketName);
			FTransform SpawnTransform;
			SpawnTransform.SetLocation(MeshTransform.TransformPosition(LocationOffset));
			SpawnTransform.SetRotation(MeshTransform.GetRotation() * RotationOffsetQuat);
			SpawnTransform.SetScale3D(StartingScale);
			ParticleSystem = UGameplayStatics::SpawnEmitterAtLocation(MeshComp->GetWorld(), PSTemplate, SpawnTransform);
		}
	}
	else
	{
		UE_LOG(LogBlueprint, Warning, TEXT("Particle Notify: Particle system is null for particle notify '%s' in anim: '%s'"), *GetNotifyName(), *GetPathNameSafe(Animation));
	}

}


void UEGSpawnScaledParticle::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::NotifyEnd(MeshComp, Animation);
	if (ParticleSystem)
		ParticleSystem->DestroyComponent();
}


#endif

inline FString UEGSpawnScaledParticle::GetNotifyName_Implementation() const
{
	if (PSTemplate)
		return PSTemplate->GetName();
	else
		return Super::GetNotifyName_Implementation();
}
