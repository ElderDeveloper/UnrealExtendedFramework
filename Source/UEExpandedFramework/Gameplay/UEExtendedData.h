// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UEExtendedData.generated.h"

UENUM()
enum EButtonAction
{
	Press,
	Release
};

UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedData : public UObject
{
	GENERATED_BODY()
};
