// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EGDamageTrace_DamageObject.generated.h"

/**
 * 
 */
UCLASS(BlueprintType , Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API UEGDamageTrace_DamageObject : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintNativeEvent)
	void ReceiveApplyDamageInformation(USkeletalMeshComponent* OwnerMeshComp , FHitResult HitResult);
};

inline void UEGDamageTrace_DamageObject::ReceiveApplyDamageInformation_Implementation(USkeletalMeshComponent* OwnerMeshComp, FHitResult HitResult)
{
}
