// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EGComboComponentNotify.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGComboComponentNotify : public UAnimNotify
{
	GENERATED_BODY()

	UEGComboComponentNotify();

protected:

	//virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
	

#if ENGINE_MAJOR_VERSION != 5
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
#endif

#if ENGINE_MAJOR_VERSION == 5
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
#endif
};


