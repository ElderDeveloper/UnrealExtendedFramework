// Fill out your copyright notice in the Description page of Project Settings.

#include "EFExtendedAbilityComponent.h"
#include "Abilities/EFExtendedAbility.h"
#include "UnrealExtendedFramework/Systems/Attribute/EFAttributeComponent.h"


UEFExtendedAbilityComponent::UEFExtendedAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}



// ADD and REMOVE Abilities
UEFExtendedAbility*  UEFExtendedAbilityComponent::AddExtendedAbility(AActor* Instigator , const TSubclassOf<UEFExtendedAbility> AbilityClass)
{
	if (!IsValid(AbilityClass))
	{
		UE_LOG( LogTemp , Warning , TEXT("Ability Class is not valid"));
		return nullptr;
	}
		

	if(const auto NewAbility = NewObject<UEFExtendedAbility>(this , AbilityClass))
	{
		ExtendedAbilities.Add(NewAbility);
		NewAbility->OnExtendedAbilityCreated(GetOwner());
		
		if (NewAbility->AbilityActivationPolicy==EFAbilityActivationPolicy::OnSpawn && ensure(NewAbility->CanStartExtendedAbility(Instigator)))
			NewAbility->StartExtendedAbilityPure(Instigator);
		
		if (NewAbility->bShouldUseExtendedTick)
			ExtendedTickAbilities.Add(NewAbility);

		return NewAbility;
	}
	
	return nullptr;
}



bool UEFExtendedAbilityComponent::RemoveExtendedAbilityByName(const FName AbilityName)
{
	for (const auto ability : ExtendedAbilities)
	{
		if (ability->ExtendedAbilityName == AbilityName)
		{
			ExtendedAbilities.Remove(ability);
			return true;
		}
	}
	return false;
}



bool UEFExtendedAbilityComponent::RemoveExtendedAbilityByClass(const TSubclassOf<UEFExtendedAbility> AbilityClass)
{
	for (const auto ability : ExtendedAbilities)
	{
		if (ability->GetClass() == AbilityClass)
		{
			ExtendedAbilities.Remove(ability);
			return true;
		}
	}
	return false;
}



// Start and Stop Ability By Name
bool UEFExtendedAbilityComponent::StartExtendedAbilityByName(AActor* Instigator, const FName AbilityName)
{
	for (const auto ability : ExtendedAbilities)
	{
		if (ability && ability->ExtendedAbilityName == AbilityName)
			return ability->StartExtendedAbilityPure(Instigator);
	}
	return false;
}



bool UEFExtendedAbilityComponent::StopExtendedAbilityByName(AActor* Instigator, const FName AbilityName)
{
	for (const auto ability : ExtendedAbilities)
	{
		if (ability && ability->ExtendedAbilityName == AbilityName)
			return ability->StopExtendedAbilityPure(Instigator);
	}
	return false;
}



// Start and Stop Ability By Class
bool UEFExtendedAbilityComponent::StartExtendedAbilityByClass(AActor* Instigator,const TSubclassOf<UEFExtendedAbility> AbilityClass)
{
	for (const auto ability : ExtendedAbilities)
	{
		if (ability && ability->GetClass() == AbilityClass)
			return ability->StartExtendedAbilityPure(Instigator);
	}
	return false;
}



bool UEFExtendedAbilityComponent::StopExtendedAbilityByClass(AActor* Instigator,const TSubclassOf<UEFExtendedAbility> AbilityClass)
{
	for (const auto ability : ExtendedAbilities)
	{
		if (ability && ability->GetClass() == AbilityClass)
			return ability->StopExtendedAbilityPure(Instigator);
	}
	return false;
}



// Start and Stop Ability By Index
bool UEFExtendedAbilityComponent::StartExtendedAbilityByIndex(AActor* Instigator, const int32 Index)
{
	if (ExtendedAbilities.IsValidIndex(Index))
		return ExtendedAbilities[Index]->StartExtendedAbilityPure(Instigator);
	return false;
}



bool UEFExtendedAbilityComponent::StopExtendedAbilityByIndex(AActor* Instigator, const int32 Index)
{
	if (ExtendedAbilities.IsValidIndex(Index))
		return ExtendedAbilities[Index]->StopExtendedAbilityPure(Instigator);
	return false;
}



bool UEFExtendedAbilityComponent::AddExtendedActiveTag(const FGameplayTag Tag)
{
	if(ActiveGameplayTags.HasTag(Tag))
		return false;
	ActiveGameplayTags.AddTag(Tag);
	OnExtendedGameplayTagAdded.Broadcast(nullptr , Tag );
	return true;
}



void UEFExtendedAbilityComponent::AppendExtendedActiveTags(const FGameplayTagContainer GrantedTags)
{
	for (const auto Tag : GrantedTags)
	{
		if (!ActiveGameplayTags.HasTag(Tag))
		{
			ActiveGameplayTags.AddTag(Tag);
			OnExtendedGameplayTagAdded.Broadcast(nullptr , Tag );
		}
	}
}



void UEFExtendedAbilityComponent::RemoveExtendedActiveTags(const FGameplayTagContainer RemovedTags)
{
	for (const auto Tag : RemovedTags)
	{
		if (ActiveGameplayTags.HasTag(Tag))
		{
			ActiveGameplayTags.RemoveTag(Tag);
			OnExtendedGameplayTagRemoved.Broadcast(nullptr , Tag );
		}
	}
}






bool UEFExtendedAbilityComponent::RemoveExtendedActiveTag(const FGameplayTag Tag)
{
	if(ActiveGameplayTags.HasTag(Tag))
	{
		ActiveGameplayTags.RemoveTag(Tag);
		OnExtendedGameplayTagRemoved.Broadcast(nullptr , Tag );
		return true;
	}
	return false;
}



bool UEFExtendedAbility::GetIsExtendedAbilityRunning() const
{
	return GetOwnerExtendedComponent()->CheckHasAnyTagInActiveTags(GrantsTags) || bIsAbilityActive;  
}



// Virtual Functions
void UEFExtendedAbilityComponent::BeginPlay()
{
	Super::BeginPlay();

	for(const auto i : StartupExtendedAbilities)
	{
		if (IsValid(i))
		{
			AddExtendedAbility(GetOwner() , i);
		}
	}

	if (const auto actorComponent = GetOwner()->GetComponentByClass(UEFAttributeComponent::StaticClass()))
	{
		if (const auto attributeComp = Cast<UEFAttributeComponent>(actorComponent))
		{
			AttributeComponent = attributeComp;
		}
	}
}



void UEFExtendedAbilityComponent::TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (const auto TickAbility : ExtendedTickAbilities )
	{
		if(TickAbility)
		{
			TickAbility->TickExtendedAbility(GetOwner() , DeltaTime);
		}
	}
}



bool UEFExtendedAbilityComponent::GetHasAbility(FName AbilityName) const
{
	for (const auto ability : ExtendedAbilities)
	{
		if (ability && ability->ExtendedAbilityName == AbilityName)
			return true;
	}
	return false;
}




UEFExtendedAbility* UEFExtendedAbilityComponent::GetExtendedAbilityByIndex(const int32 Index) const
{
	if (ExtendedAbilities.IsValidIndex(Index))
	{
		return ExtendedAbilities[Index];
	}
	return nullptr;
}



UEFExtendedAbility* UEFExtendedAbilityComponent::GetExtendedAbilityByName(const FName AbilityName) const
{
	for (const auto ability : ExtendedAbilities)
	{
		if (ability && ability->ExtendedAbilityName == AbilityName)
			return ability;
	}
	return nullptr;
}



UEFExtendedAbility* UEFExtendedAbilityComponent::GetExtendedAbilityByClass(const TSubclassOf<UEFExtendedAbility> AbilityClass) const
{
	for (const auto ability : ExtendedAbilities)
	{
		if (ability && ability->GetClass() == AbilityClass)
			return ability;
	}
	return nullptr;
}



void UEFExtendedAbilityComponent::PrintActiveTags(const float Time, FColor DisplayColor) const
{
	for (const auto& tag : ActiveGameplayTags)
	{
		GEngine->AddOnScreenDebugMessage(-1,Time,DisplayColor,tag.ToString());	
	}
}

