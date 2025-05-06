// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EFShaderCompileLibrary.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMaterialCompilationFinishedDelegate);

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFShaderCompileLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable,Category = "Shader Compilation")
	static TArray<TSoftObjectPtr<UMaterial>> EFGetAllMaterials();
	
	UFUNCTION(BlueprintCallable,Category = "Shader Compilation")
	static TArray<TSoftObjectPtr<UNiagaraSystem>> EFGetAllNiagaraParticleSystems();

	UFUNCTION(BlueprintCallable,Category = "Shader Compilation")
	static bool EFGetShadersCompiling();
};
