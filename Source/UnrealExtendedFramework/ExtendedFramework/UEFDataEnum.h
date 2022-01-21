// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UEFDataEnum.generated.h"

UENUM()
enum EExtendedLoopOutput
{
	ExtendedLoop,
	ExtendedComplete
};

UENUM(BlueprintType)
enum EUEFConditionOutput
{
	UEF_True,
	UEF_False
};



UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UUEFDataEnum : public UObject
{
	GENERATED_BODY()
};
