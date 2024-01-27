// Fill out your copyright notice in the Description page of Project Settings.
#include "EFExtendedAbility_Effect.h"
#include "UnrealExtendedFramework/Systems/Ability/EFExtendedAbilityComponent.h"


UEFExtendedAbility_Effect::UEFExtendedAbility_Effect(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AbilityActivationPolicy = EFAbilityActivationPolicy::OnSpawn;
}


void UEFExtendedAbility_Effect::StartExtendedAbility_Implementation(AActor* Instigator)
{
	Super::StartExtendedAbility_Implementation(Instigator);
	
	if(Duration > 0.0f)
	{
		FTimerDelegate Delegate;
		Delegate.BindUFunction(this ,"StopExtendedAbility",Instigator);
		GetWorld()->GetTimerManager().SetTimer(DurationHandle , Delegate , Duration , false);
	}
	if (Period >0.0f)
	{
		FTimerDelegate Delegate;
		Delegate.BindUFunction(this ,"ExecutePeriodEffect",Instigator);
		GetWorld()->GetTimerManager().SetTimer(PeriodHandle , Delegate , Period , true);
	}
}

void UEFExtendedAbility_Effect::StopExtendedAbility_Implementation(AActor* Instigator)
{
	if (GetWorld()->GetTimerManager().GetTimerRemaining(PeriodHandle) < KINDA_SMALL_NUMBER)
		ExecutePeriodEffect(Instigator);
	
	Super::StopExtendedAbility_Implementation(Instigator);

	GetWorld()->GetTimerManager().ClearTimer(PeriodHandle);
	GetWorld()->GetTimerManager().ClearTimer(DurationHandle);

	if (const auto AbilityComp = GetOwnerExtendedComponent())
		AbilityComp->RemoveExtendedAbilityByClass(GetClass());
}

void UEFExtendedAbility_Effect::ExecutePeriodEffect_Implementation(AActor* Instigator)
{
}
