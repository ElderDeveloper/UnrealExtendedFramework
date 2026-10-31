// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSpawnScaledParticle.h"

#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveVector.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"

UEGSpawnScaledParticle::UEGSpawnScaledParticle()
{
	Attached = true;
	NiagaraSystem = nullptr;
	PSTemplate = nullptr;
	LocationOffset = FVector::ZeroVector;
	RotationOffset = FRotator::ZeroRotator;
	SocketName = NAME_None;
	StartingScale = FVector(1, 1, 1);
	EndScale = FVector(1, 1, 1);
	ScaleInterpSpeed = 4;
	ScaleCurve = nullptr;

#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(192, 255, 99, 255);
#endif // WITH_EDITORONLY_DATA
}

UFXSystemComponent* UEGSpawnScaledParticle::SpawnEffect(USkeletalMeshComponent* MeshComp) const
{
	if (Attached)
	{
		if (NiagaraSystem)
		{
			UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAttached(NiagaraSystem, MeshComp, SocketName, LocationOffset, RotationOffset, EAttachLocation::KeepRelativeOffset, true);
			if (Component)
			{
				Component->SetRelativeScale3D(StartingScale);
			}
			return Component;
		}
		return UGameplayStatics::SpawnEmitterAttached(PSTemplate, MeshComp, SocketName, LocationOffset, RotationOffset, StartingScale);
	}

	const FTransform SocketTransform = MeshComp->GetSocketTransform(SocketName);
	const FVector Location = SocketTransform.TransformPosition(LocationOffset);
	const FRotator Rotation = (SocketTransform.GetRotation() * FQuat(RotationOffset)).Rotator();

	if (NiagaraSystem)
	{
		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(MeshComp, NiagaraSystem, Location, Rotation, StartingScale, true);
	}
	return UGameplayStatics::SpawnEmitterAtLocation(MeshComp->GetWorld(), PSTemplate, FTransform(Rotation, Location, StartingScale));
}

void UEGSpawnScaledParticle::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp)
	{
		return;
	}

	if (!NiagaraSystem && !PSTemplate)
	{
		UE_LOG(LogBlueprint, Warning, TEXT("Particle Notify: No Niagara or Cascade system set for particle notify '%s' in anim: '%s'"), *GetNotifyName(), *GetPathNameSafe(Animation));
		return;
	}

	// Drop entries for meshes destroyed mid-window, and any effect left from an unfinished window.
	for (auto It = ActiveEffects.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	DestroyEffect(MeshKey);

	if (UFXSystemComponent* Component = SpawnEffect(MeshComp))
	{
		FActiveEffect& Effect = ActiveEffects.Add(MeshKey);
		Effect.Component = Component;
		Effect.ElapsedTime = 0.0f;
	}
}

void UEGSpawnScaledParticle::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	FActiveEffect* Effect = ActiveEffects.Find(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp));
	UFXSystemComponent* Component = Effect ? Effect->Component.Get() : nullptr;
	if (!Component)
	{
		return;
	}

	Effect->ElapsedTime += FrameDeltaTime;

	// Relative scale on both sides: reading the world scale here compounded the parent's scale.
	const FVector TargetScale = ScaleCurve
		? ScaleCurve->GetVectorValue(Effect->ElapsedTime)
		: FMath::VInterpTo(Component->GetRelativeScale3D(), EndScale, FrameDeltaTime, ScaleInterpSpeed);

	Component->SetRelativeScale3D(TargetScale);
}

void UEGSpawnScaledParticle::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	DestroyEffect(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp));
}

void UEGSpawnScaledParticle::DestroyEffect(const TWeakObjectPtr<USkeletalMeshComponent>& MeshKey)
{
	if (FActiveEffect* Effect = ActiveEffects.Find(MeshKey))
	{
		if (UFXSystemComponent* Component = Effect->Component.Get())
		{
			Component->DestroyComponent();
		}
		ActiveEffects.Remove(MeshKey);
	}
}

FString UEGSpawnScaledParticle::GetNotifyName_Implementation() const
{
	if (NiagaraSystem)
	{
		return NiagaraSystem->GetName();
	}
	if (PSTemplate)
	{
		return PSTemplate->GetName();
	}
	return Super::GetNotifyName_Implementation();
}
