// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"

#include "EGDamageTrace_DamageObject.generated.h"

class USkeletalMeshComponent;

/**
 * Custom damage handling for UEGDamageTrace_Notify. Subclass in Blueprint and override
 * ReceiveApplyDamageInformation; a new instance is created for each hit, outered to the attacker,
 * so world context nodes work inside it.
 */
UCLASS(BlueprintType, Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API UEGDamageTrace_DamageObject : public UObject
{
	GENERATED_BODY()

public:
	/** The notify's DamageCustomTag, set before ReceiveApplyDamageInformation is called. */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FGameplayTag DamageTag;

	UFUNCTION(BlueprintNativeEvent, Category = "Damage")
	void ReceiveApplyDamageInformation(USkeletalMeshComponent* OwnerMeshComp, FHitResult HitResult);

	virtual UWorld* GetWorld() const override;
};
