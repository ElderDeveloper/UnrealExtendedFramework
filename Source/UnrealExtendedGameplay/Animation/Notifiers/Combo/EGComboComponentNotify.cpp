// Fill out your copyright notice in the Description page of Project Settings.


#include "EGComboComponentNotify.h"

#include "UnrealExtendedGameplay/Systems/Combat/Components/Combo/EGComboComponent.h"
#include "UnrealExtendedGameplay/Systems/Combat/Components/Combo/EGRandomComboComponent.h"


UEGComboComponentNotify::UEGComboComponentNotify()
{
	
}


#if ENGINE_MAJOR_VERSION != 5

void UEGComboComponentNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::Notify(MeshComp, Animation);

	if (const auto ComboComponent = Cast<UEGComboComponent>(MeshComp->GetOwner()->FindComponentByClass<UEGComboComponent>()))
	{
	}

	if (const auto RandomCombo = Cast<UEGRandomComboComponent>(MeshComp->GetOwner()->FindComponentByClass<UEGRandomComboComponent>()))
	{
		RandomCombo->SetEnableNextCombo();
	}
}

#endif

#if ENGINE_MAJOR_VERSION == 5


void UEGComboComponentNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);


	if (const auto ComboComponent = Cast<UEGComboComponent>(MeshComp->GetOwner()->FindComponentByClass<UEGComboComponent>()))
	{
	}

	if (const auto RandomCombo = Cast<UEGRandomComboComponent>(MeshComp->GetOwner()->FindComponentByClass<UEGRandomComboComponent>()))
	{
		RandomCombo->SetEnableNextCombo();
	}

}
#endif

