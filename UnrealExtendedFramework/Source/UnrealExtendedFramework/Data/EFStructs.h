// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EFEnums.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "UObject/Object.h"
#include "EFStructs.generated.h"





USTRUCT(BlueprintType)
struct FExtendedFrameworkDamageReaction : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	FGameplayTagContainer DamageTag = FGameplayTagContainer();
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	TSubclassOf<UDamageType> DamageType = nullptr;

	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	TEnumAsByte<EFDamageDirection> DamageDirection = EFDamageDirection::UEF_DamageForward;

	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	UAnimMontage* MontageReaction = nullptr;
};





UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFStructs : public UObject
{
	GENERATED_BODY()
};
