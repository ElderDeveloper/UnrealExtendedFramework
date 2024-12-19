// Fill out your copyright notice in the Description page of Project Settings.


#include "EGFootstepComponent.h"


#include "NiagaraFunctionLibrary.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UnrealExtendedFramework/Libraries/Trace/EFTraceLibrary.h"


void UEGFootstepComponent::SpawnFootStepEvents(const FVector LegPosition)
{
	LineTraceInformation.Start = LegPosition+FVector(0,0,20);
	LineTraceInformation.End = LegPosition+FVector(0,0,-500);
	if(UEFTraceLibrary::ExtendedLineTraceSingle(GetWorld(),LineTraceInformation))
	{
		FFootStepStruct* FoundStruct;
		if(GetSurfaceEffects(LineTraceInformation.HitResult.PhysMaterial->SurfaceType,FoundStruct))
		{
			SpawnEffects(FoundStruct,&LineTraceInformation);
		}
	}
}


bool UEGFootstepComponent::GetSurfaceEffects(const TEnumAsByte<EPhysicalSurface> surfaceType ,FFootStepStruct*& surfaceStruct) const
{
	if (FootStepTable)
	{
		TArray<FName> RowNames = FootStepTable->GetRowNames();
		for (const auto name : RowNames)
		{
			if (const auto FPStruct = FootStepTable->FindRow<FFootStepStruct>(name,""))
			{
				if(FPStruct->SurfaceType==surfaceType)
				{
					surfaceStruct=FPStruct;
					return true;
				}
			}
		}
	}
	return false;
}





void UEGFootstepComponent::SpawnEffects(const FFootStepStruct* footStepStruct , const FLineTraceStruct* HitStruct)
{
	if (footStepStruct->Particle)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),
			footStepStruct->Particle,
			HitStruct->HitResult.Location + footStepStruct->ParticleLocationPlus,
			footStepStruct->ParticleRotatorPlus,
			footStepStruct->ParticleScale
			);
	}
	if (footStepStruct->NiagaraSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),
			footStepStruct->NiagaraSystem,
			HitStruct->HitResult.Location + footStepStruct->NiagaraLocationPlus,
			footStepStruct->NiagaraRotatorPlus,
			footStepStruct->NiagaraScale
			);
	}

	if (footStepStruct->Sound)
	{
		UGameplayStatics::SpawnSoundAtLocation(GetWorld(),
			footStepStruct->Sound,
			GetOwner()->GetActorLocation(),
			FRotator::ZeroRotator,
			footStepStruct->SoundVolume
			);
	}

	if (footStepStruct->Decal)
	{
		const FRotator DecalRotator = GetOwner()->GetActorRotation()+FRotator(-90,90,0)+footStepStruct->DecalRotatorPlus;
		UGameplayStatics::SpawnDecalAtLocation(GetWorld(),
			footStepStruct->Decal,
			footStepStruct->DecalScale,
			HitStruct->HitResult.Location,
			DecalRotator,
			footStepStruct->DecalLifetime
			);
	}
}




