// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EFPrintScreenLibrary.generated.h"


UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFPrintScreenLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject",DisplayName = "Print Zero String"), Category="Math|Library")
	static void PrintZeroString(const UObject* WorldContextObject ,FString String = "" , float Time = 0 , FLinearColor Color = FLinearColor::Green);
};
