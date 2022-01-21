// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EFEnums.generated.h"



UENUM(BlueprintType,Blueprintable)
enum EHitDirection
{
	No , Front , Back , Left , Right
};

UENUM()
enum EConditionOutput
{
	OutTrue,
	OutIsFalse
};

UENUM()
enum EButtonAction
{
	Press,
	Release
};

#define PRINT_STRING(Time , Color , String) 	GEngine->AddOnScreenDebugMessage(-1, Time , FColor::Color , String);

UCLASS()
class UEFEnums : public UObject
{
	GENERATED_BODY()
};