// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UEExtendedLibrary.generated.h"

/**
 * 
 */

UENUM()
enum EExtendedLog
{
	Log,
	Warning,
	Error
};
UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable , Category="Extended Log")
	static void ExtendedBlueprintLOG(TEnumAsByte<EExtendedLog> ExtendedLogType , FString Log);
};
