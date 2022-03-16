// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/Data/EFEnums.h"
#include "EFJsonLibrary.generated.h"


class UEFJsonObject;

UCLASS()
class UNREALEXTENDEDBACKEND_API UEFJsonLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable , Category="ExtendedFramework|JSonLibrary")
	static UEFJsonObject* LoadJSonFile(TEnumAsByte<EFProjectDirectory> DirectoryType , FString Directory);
	
};
