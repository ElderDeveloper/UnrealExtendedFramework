// Fill out your copyright notice in the Description page of Project Settings.


#include "EFShaderCompileActor.h"

#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Library/EFShaderCompileLibrary.h"


// Sets default values
AEFShaderCompileActor::AEFShaderCompileActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

void AEFShaderCompileActor::FinishCompiling()
{
}

// Called when the game starts or when spawned
void AEFShaderCompileActor::BeginPlay()
{
	Super::BeginPlay();

	SaveTextureQuality = UGameUserSettings::GetGameUserSettings()->GetTextureQuality();
	UGameUserSettings::GetGameUserSettings()->SetTextureQuality(0);
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(),5.f);

	bIsCompilingMaterials = true;
	bIsCompilingSystems = true;

	//TODO: Show Load Widget

	
	MaterialReferences = UEFShaderCompileLibrary::EFGetAllMaterials();
	ParticleSystemReferences = UEFShaderCompileLibrary::EFGetAllNiagaraParticleSystems();
	
}

// Called every frame
void AEFShaderCompileActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (UEFShaderCompileLibrary::EFGetShadersCompiling())
	{
		return;
	}

	if (!(bIsCompilingMaterials || bIsCompilingSystems))
	{
		FinishCompiling();
	}

	
}

