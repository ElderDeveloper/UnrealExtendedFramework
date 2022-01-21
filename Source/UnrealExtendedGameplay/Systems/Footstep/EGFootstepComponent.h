// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "UnrealExtendedFramework/Data/EFTraceData.h"
#include "EGFootstepComponent.generated.h"


class UParticleEmitter;
class UDataTable;
class USoundBase;
class UMaterialInterface;
class UNiagaraSystem;

USTRUCT()
struct FFootStepStruct : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere ,BlueprintReadWrite, Category="FootStep|Surface")
	TEnumAsByte<EPhysicalSurface> SurfaceType;

	UPROPERTY(EditAnywhere ,BlueprintReadWrite, Category="FootStep|Niagara")
	UNiagaraSystem* NiagaraSystem;
	
	UPROPERTY(EditAnywhere ,BlueprintReadWrite, Category="FootStep|Niagara")
	FVector NiagaraScale {1,1,1};

	UPROPERTY(EditAnywhere ,BlueprintReadWrite, Category="FootStep|Particle")
	FVector NiagaraLocationPlus {0.f,0.f,0.f};
	
	UPROPERTY(EditAnywhere ,BlueprintReadWrite, Category="FootStep|Niagara")
	FRotator NiagaraRotatorPlus;


	
	UPROPERTY(EditAnywhere ,BlueprintReadWrite, Category="FootStep|Particle")
	UParticleSystem* Particle;
	
	UPROPERTY(EditAnywhere ,BlueprintReadWrite, Category="FootStep|Particle")
	FVector ParticleScale {1,1,1};

	UPROPERTY(EditAnywhere ,BlueprintReadWrite, Category="FootStep|Particle")
	FVector ParticleLocationPlus {0.f,0.f,0.f};
	
	UPROPERTY(EditAnywhere ,BlueprintReadWrite, Category="FootStep|Particle")
	FRotator ParticleRotatorPlus;


	
	UPROPERTY(EditAnywhere ,BlueprintReadWrite, Category="FootStep|Sound")
	USoundBase* Sound ;
	
	UPROPERTY(EditAnywhere ,BlueprintReadWrite, Category="FootStep|Sound")
	float SoundVolume = 1.f;


	
	UPROPERTY(EditAnywhere ,BlueprintReadWrite, Category="FootStep|Decal")
	UMaterialInterface* Decal;
	
	UPROPERTY(EditAnywhere ,BlueprintReadWrite, Category="FootStep|Decal")
	FVector DecalScale {45,45,45};;
	
	UPROPERTY(EditAnywhere ,BlueprintReadWrite, Category="FootStep|Decal")
	FRotator DecalRotatorPlus;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category="FootStep|Decal")
	float DecalLifetime = 3;
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category="Extended Foot Step")
	UDataTable* FootStepTable;

	
	UPROPERTY(EditAnywhere , BlueprintReadWrite, Category="Extended Foot Step")
	FLineTraceStruct LineTraceInformation;

	
	UFUNCTION(BlueprintCallable, Category="Extended Foot Step")
	void SpawnFootStepEvents(const FVector LegPosition);

private:

	void SpawnEffects(const FFootStepStruct* footStepStruct , const FLineTraceStruct* HitStruct);

	bool GetSurfaceEffects(const TEnumAsByte<EPhysicalSurface> surfaceType , FFootStepStruct*& surfaceStruct) const;
	
};
