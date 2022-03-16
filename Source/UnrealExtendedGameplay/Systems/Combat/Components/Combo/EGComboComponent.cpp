// Fill out your copyright notice in the Description page of Project Settings.


#include "EGComboComponent.h"


// Sets default values for this component's properties
UEGComboComponent::UEGComboComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UEGComboComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void UEGComboComponent::OnCombatMontagesEnd()
{
}

void UEGComboComponent::OnMontageBlendOut(UAnimMontage* Montage, bool bInterrupted)
{
}

