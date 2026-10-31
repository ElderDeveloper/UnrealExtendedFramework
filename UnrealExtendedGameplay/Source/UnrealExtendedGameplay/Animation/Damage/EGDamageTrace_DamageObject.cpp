// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGDamageTrace_DamageObject.h"

void UEGDamageTrace_DamageObject::ReceiveApplyDamageInformation_Implementation(USkeletalMeshComponent* OwnerMeshComp, FHitResult HitResult)
{
}

UWorld* UEGDamageTrace_DamageObject::GetWorld() const
{
	// The class default object has no world; returning null there (instead of the base UObject
	// implementation) is what lets the Blueprint editor offer world context nodes.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	const UObject* Outer = GetOuter();
	return Outer ? Outer->GetWorld() : nullptr;
}
