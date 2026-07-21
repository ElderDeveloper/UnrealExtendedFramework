// Fill out your copyright notice in the Description page of Project Settings.


#include "EFExtendedAbility.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UnrealExtendedFramework/Systems/Ability/EFExtendedAbilityComponent.h"


UEFExtendedAbility::UEFExtendedAbility(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer), AbilityActivationPolicy(EFAbilityActivationPolicy::OnInputTriggered)
{
	AbilityLevel = 1;
	AbilityMaxLevel = -1;
}




void UEFExtendedAbility::StopInstanceAbility_Implementation(UEFExtendedAbility* InstanceAbility)
{
	Instances.Remove(InstanceAbility);
}


void UEFExtendedAbility::OnStartInstanceAbility_Implementation(AActor* Instigator,UEFExtendedAbility* InstanceOwnerAbility, int32 InstanceIndex)
{
}


void UEFExtendedAbility::OnAbilityLevelChanged_Implementation(int32 NewLevel, int32 OldLevel)
{
}


bool UEFExtendedAbility::CanStartExtendedAbility_Implementation(AActor* Instigator)
{
	if(GetOwnerExtendedComponent()->CheckHasAnyTagInActiveTags(BlockedTags))
		return false;
	
	return true;
}

bool UEFExtendedAbility::StartExtendedAbilityPure(AActor* Instigator)
{
	if (CanStartExtendedAbility(Instigator))
	{
		GetOwnerExtendedComponent()->AppendExtendedActiveTags(GrantsTags);
		bIsAbilityActive = true;
		StartExtendedAbility(Instigator);
		UE_LOG(LogTemp , Log , TEXT("Running Ability: %s") , *GetNameSafe(this));
		return true;
	}
	return false;

}

bool UEFExtendedAbility::StopExtendedAbilityPure(AActor* Instigator)
{
	if (bIsAbilityActive)
	{
		bIsAbilityActive = false;
		UE_LOG(LogTemp , Log , TEXT("Stopped Ability: %s") , *GetNameSafe(this));
		StopExtendedAbility(Instigator);
		GetOwnerExtendedComponent()->RemoveExtendedActiveTags(GrantsTags);
		return true;
	}
	return false;
}




void UEFExtendedAbility::StopExtendedAbility_Implementation(AActor* Instigator) {}



void UEFExtendedAbility::StopExtendedAbilityProtected(AActor* Instigator)
{
	if (Instigator == nullptr)
		Instigator = GetOwnerAsActor();
	StopExtendedAbilityPure(Instigator);
}

void UEFExtendedAbility::SetAbilityLevel(int32 NewLevel)
{
	if (AbilityMaxLevel != -1)
	{
		if (NewLevel > AbilityMaxLevel)
			NewLevel = AbilityMaxLevel;
	}
	const int32 OldLevel = AbilityLevel;
	AbilityLevel = NewLevel;
	OnAbilityLevelChanged(AbilityLevel , OldLevel);
}

void UEFExtendedAbility::IncreaseAbilityLevel(int32 Amount)
{
	if (AbilityMaxLevel != -1)
	{
		if (AbilityLevel + Amount > AbilityMaxLevel)
		{
			SetAbilityLevel(AbilityMaxLevel);
			return;
		}
	}
	SetAbilityLevel(AbilityLevel + Amount);
	AbilityLevel = AbilityLevel + 1;
}

void UEFExtendedAbility::DecreaseAbilityLevel(int32 Amount)
{
	if (AbilityLevel - Amount < 1)
	{
		 GetOwnerExtendedComponent()->RemoveExtendedAbilityByClass(GetClass());
		return;
	}

	SetAbilityLevel(AbilityLevel - Amount);
}



void UEFExtendedAbility::TickExtendedAbility_Implementation(AActor* Instigator, float DeltaTime){}
void UEFExtendedAbility::StartExtendedAbility_Implementation(AActor* Instigator)
{
	if(bShouldStartAsInstance)
	{
		for (int32 i = 0 ; i < InstanceCount ; i++)
		{
			if (const auto NewAbility = NewObject<UEFExtendedAbility>(GetOwnerExtendedComponent() , GetClass()))
			{
				Instances.Add(NewAbility);
				NewAbility->OnExtendedAbilityCreated(Instigator);
				NewAbility->OnStartInstanceAbility(Instigator , this , i);
			}
		}
	}
}
void UEFExtendedAbility::OnExtendedAbilityCreated_Implementation(AActor* Instigator){}




UWorld* UEFExtendedAbility::GetWorld() const
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && ensureMsgf(GetOuter(), TEXT("Actor: %s has a null OuterPrivate in AActor::GetWorld()"), *GetFullName())
		&& !GetOuter()->HasAnyFlags(RF_BeginDestroyed) && !GetOuter()->IsUnreachable())
	{
		if (ULevel* Level = GetLevel())
		{
			return Level->OwningWorld;
		}
	}
	return nullptr;
}


ULevel* UEFExtendedAbility::GetLevel() const
{
	return GetTypedOuter<ULevel>();
}


UEFExtendedAbilityComponent* UEFExtendedAbility::GetOwnerExtendedComponent() const
{
	return Cast<UEFExtendedAbilityComponent>(GetOuter());
}


AActor* UEFExtendedAbility::GetOwnerAsActor() const
{
	if (GetOwnerExtendedComponent())
	{
		if (GetOwnerExtendedComponent()->GetOwner())
		{
			if (const auto actor = Cast<AActor>(GetOwnerExtendedComponent()->GetOwner()))
				return actor;
		}
	}

	return nullptr;
}


ACharacter* UEFExtendedAbility::GetOwnerAsCharacter() const
{
	if (GetOwnerExtendedComponent())
	{
		if (GetOwnerExtendedComponent()->GetOwner())
		{
			if (const auto character = Cast<ACharacter>(GetOwnerExtendedComponent()->GetOwner()))
				return character;
		}
	}

	return nullptr;
}


UCharacterMovementComponent* UEFExtendedAbility::GetOwnerAsCharacterMovement() const
{
	if(GetOwnerAsCharacter())	
		return GetOwnerAsCharacter()->GetCharacterMovement();
	return nullptr;
}


USkeletalMeshComponent* UEFExtendedAbility::GetOwnerAsSkeletalMesh() const
{
	if(GetOwnerAsCharacter())	
		return GetOwnerAsCharacter()->GetMesh();
	return nullptr;
}