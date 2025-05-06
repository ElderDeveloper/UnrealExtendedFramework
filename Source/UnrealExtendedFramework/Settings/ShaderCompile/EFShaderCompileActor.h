// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EFShaderCompileActor.generated.h"

class UNiagaraSystem;

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API AEFShaderCompileActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEFShaderCompileActor();

protected:

	int32 SaveTextureQuality = 0;
	bool bIsCompilingSystems = false;
	bool bIsCompilingMaterials = false;

	TArray<TSoftObjectPtr<UMaterial>> MaterialReferences;
	TArray<TSoftObjectPtr<UNiagaraSystem>> ParticleSystemReferences;

	void FinishCompiling();
	
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
};
