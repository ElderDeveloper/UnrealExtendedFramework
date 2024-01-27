// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EFEditorLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable , Category = "Extended Editor")
	static void SetDefaultGameMap(const FString& MapName);

	UFUNCTION(BlueprintPure , Category = "Extended Editor")
	static FString GetDefaultGameMap();
};
