// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

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
