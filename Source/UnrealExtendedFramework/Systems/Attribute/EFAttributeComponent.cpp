// Fill out your copyright notice in the Description page of Project Settings.

#include "EFAttributeComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Library/EFAttributeLibrary.h"


UEFAttributeComponent::UEFAttributeComponent()
{
	OnAttributeModified.AddDynamic(this , &UEFAttributeComponent::OnAttributeModifiedTrigger);
}



void UEFAttributeComponent::OnAttributeModifiedTrigger(const FExtendedAttribute& Attribute)
{
	if (Attribute.AttributeTag == MovementSpeedAttribute)
	{
		UpdateSpeedAttribute(Attribute);
	}
}




float UEFAttributeComponent::AddAttributeValue(const FGameplayTag Attribute, const float Value)
{
	if (Attribute == FGameplayTag::EmptyTag)
	{
		return -1;
	}
	
	for (int32 i = 0 ; i < EffectAttributes.Num() ; i++)
	{
		if (EffectAttributes.IsValidIndex(i))
		{
			if (EffectAttributes[i].AttributeTag == Attribute)
			{
				EffectAttributes[i].AttributeValue += Value;
				CalculateAttributes();
				UpdateAndBroadcastLastModifiedAttribute(GetAttribute(Attribute));
				return EffectAttributes[i].AttributeValue;
			}
		}
	}
	EffectAttributes.Add(FExtendedAttribute(Attribute , Value));
	CalculateAttributes();
	UpdateAndBroadcastLastModifiedAttribute(GetAttribute(Attribute));
	return Value;
}



float UEFAttributeComponent::AddExtendedAttribute(FExtendedAttribute Attribute)
{
	if (Attribute.AttributeTag == FGameplayTag::EmptyTag)
	{
		return -1;
	}
	
	for (int32 i = 0 ; i < EffectAttributes.Num() ; i++)
	{
		if (EffectAttributes.IsValidIndex(i))
		{
			if (EffectAttributes[i].AttributeTag == Attribute.AttributeTag)
			{
				EffectAttributes[i].AttributeValue += Attribute.AttributeValue;
				CalculateAttributes();
				UpdateAndBroadcastLastModifiedAttribute(GetAttribute(Attribute.AttributeTag));
				return EffectAttributes[i].AttributeValue;
			}
		}
	}
	const int32 index = EffectAttributes.Add(FExtendedAttribute(Attribute.AttributeTag , Attribute.AttributeValue));
	CalculateAttributes();
	UpdateAndBroadcastLastModifiedAttribute(Attribute);
	return EffectAttributes[index].AttributeValue;
}



bool UEFAttributeComponent::RemoveExtendedAttribute(FExtendedAttribute Attribute)
{
	if (Attribute.AttributeTag == FGameplayTag::EmptyTag)
	{
		return false;
	}
	
	for ( int i = 0; i < EffectAttributes.Num(); i++)
	{
		if (EffectAttributes.IsValidIndex(i))
		{
			if (EffectAttributes[i].AttributeTag == Attribute.AttributeTag)
			{
				EffectAttributes[i].AttributeValue -= Attribute.AttributeValue;
				CalculateAttributes();
				UpdateAndBroadcastLastModifiedAttribute(Attribute);
				return true;
			}
		}
	}
	return false;
}



bool UEFAttributeComponent::RemoveAttributeValue(const FGameplayTag Attribute, const float Value)
{
	if (Attribute == FGameplayTag::EmptyTag)
	{
		return false;
	}
	
	for ( int i = 0; i < EffectAttributes.Num(); i++)
	{
		if (EffectAttributes.IsValidIndex(i))
		{
			if (EffectAttributes[i].AttributeTag == Attribute)
			{
				EffectAttributes[i].AttributeValue -= Value;
				CalculateAttributes();
				UpdateAndBroadcastLastModifiedAttribute(GetAttribute(Attribute));
				return true;
			}
		}
	}
	return false;
}



bool UEFAttributeComponent::RemoveAttribute(const FGameplayTag AttributeTag)
{
	for ( int i = 0; i < EffectAttributes.Num(); i++)
	{
		if (EffectAttributes[i].AttributeTag == AttributeTag)
		{
			EffectAttributes.RemoveAt(i);
			CalculateAttributes();
			return true;
		}
	}
	return false;
}



void UEFAttributeComponent::BlockDamageAny()
{
	bBlockDamage = true;
	
	FTimerHandle BlockDamageTimer;
	GetWorld()->GetTimerManager().SetTimer(BlockDamageTimer , [this]()
	{
		bBlockDamage = false;
	} , 0.01f , false);
}



void UEFAttributeComponent::InitializeStartupAttributes()
{
	if (StartupAttributeTable)
	{
		TArray<FExtendedAttribute*> StartupAttributes;
		StartupAttributeTable->GetAllRows("", StartupAttributes);
		
		for (const auto Attribute : StartupAttributes)
		{
			if (Attribute)
			{
				BaseAttributes.Add(*Attribute);
			}
		}
	}
}



void UEFAttributeComponent::CalculateAttributes()
{
	CalculatedAttributes.Empty();
	AddAttributes(CalculatedAttributes,BaseAttributes);
	AddAttributes(CalculatedAttributes , EffectAttributes);
}



void UEFAttributeComponent::UpdateAndBroadcastLastModifiedAttribute(const FExtendedAttribute& Attribute)
{
	LastModifiedAttribute = Attribute;
	LastModifiedAttribute.AttributeValue = GetAttributeValue(Attribute.AttributeTag);
	OnAttributeModified.Broadcast(LastModifiedAttribute);
}



void UEFAttributeComponent::AddAttribute(TArray<FExtendedAttribute>& TargetAttributes,const FExtendedAttribute& SourceAttribute)
{
	for (int32 i = 0 ; i < TargetAttributes.Num() ; i++)
	{
		if (TargetAttributes.IsValidIndex(i))
		{
			if (TargetAttributes[i].AttributeTag == SourceAttribute.AttributeTag)
			{
				TargetAttributes[i].AttributeValue += SourceAttribute.AttributeValue;
				return;
			}
		}
	}
	TargetAttributes.Add(SourceAttribute);
}



void UEFAttributeComponent::AddAttributes(TArray<FExtendedAttribute>& TargetAttributes,const TArray<FExtendedAttribute>& SourceAttributes)
{
	for (const auto i : SourceAttributes)
	{
		AddAttribute(TargetAttributes , i);
	}
}



void UEFAttributeComponent::ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,AController* InstigatedBy, AActor* DamageCauser, FHitResult HitInfo)
{
	if (!GetHasAnyAttribute(DamageImmunityTags))
	{
		AddAttributeValue(HealthAttribute , -Damage);
		OnTakeDamage.Broadcast( DamagedActor, Damage ,DamageType ,InstigatedBy ,DamageCauser ,HitInfo);
		
		if (GetAttributeValue(HealthAttribute) <= 0 && bIsAlive)
		{
			bIsAlive = false;
			OnOwnerDeath(GetOwner(), Damage, DamageType, InstigatedBy, DamageCauser);
			OnActorDeath.Broadcast(GetOwner());
		}
	}
}

void UEFAttributeComponent::OnOwnerDeath_Implementation(AActor* DamagedActor, float Damage,
	const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
}



void UEFAttributeComponent::UpdateSpeedAttribute(const FExtendedAttribute& Attribute)
{
	if (Attribute.AttributeTag == MovementSpeedAttribute)
	{
		if (const auto Character = Cast<ACharacter>(GetOwner()))
		{
			const float Speed = Attribute.AttributeValue;
			Character->GetCharacterMovement()->MaxWalkSpeed = Speed;
			OnSpeedAttributeModified.Broadcast(Speed);
		}
	}
}



bool UEFAttributeComponent::CheckCanTakeDamage() const
{
	for ( const auto tag : DamageImmunityTags)
	{
		if (GetHasAttribute(tag))
			return false;
	}
	return true;
}



void UEFAttributeComponent::OwnerTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,AController* InstigatedBy, AActor* DamageCauser)
{
	FTimerHandle Lambda;
	GetWorld()->GetTimerManager().SetTimer(Lambda , [this , DamagedActor , Damage , DamageType , InstigatedBy , DamageCauser]()
	{
		if (!bBlockDamage && DamagedActor)
		{
			UE_LOG (LogTemp , Log , TEXT ("Damage Taken , %f Damage") , Damage);
			const FHitResult HitInfo = FHitResult(DamagedActor ,nullptr, DamagedActor->GetActorLocation(), DamagedActor->GetActorForwardVector());
			ReceiveDamage( DamagedActor , Damage , nullptr , nullptr , nullptr , HitInfo);
		}
	} , 0.005f , false);

}



void UEFAttributeComponent::OwnerTakePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const class UDamageType* DamageType, AActor* DamageCauser)
{
	BlockDamageAny();
	FHitResult HitInfo;
	HitInfo.BoneName = BoneName;
	HitInfo.ImpactPoint = HitLocation;
	HitInfo.ImpactNormal = ShotFromDirection;
	HitInfo.Component = FHitComponent;
	
	ReceiveDamage( DamagedActor , Damage , DamageType , InstigatedBy , DamageCauser , HitInfo);
}


void UEFAttributeComponent::OwnerTakeRadialDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,FVector Origin, const FHitResult& HitInfo, AController* InstigatedBy, AActor* DamageCauser)
{
	BlockDamageAny();
	ReceiveDamage( DamagedActor , Damage , DamageType , InstigatedBy , DamageCauser , HitInfo);
}



float UEFAttributeComponent::GetBaseAttributeValue(const FGameplayTag AttributeTag) const
{
	for (const auto attribute : BaseAttributes)
	{
		if (attribute.AttributeTag == AttributeTag)
			return attribute.AttributeValue;
	}
	return -1;
}



float UEFAttributeComponent::GetEffectAttributeValue(const FGameplayTag AttributeTag) const
{
	for (const auto attribute : EffectAttributes)
	{
		if (attribute.AttributeTag == AttributeTag)
			return attribute.AttributeValue;
	}
	return -1;
}

FExtendedAttribute UEFAttributeComponent::GetAttribute(const FGameplayTag AttributeTag) const
{
	for (const auto i : CalculatedAttributes)
	{
		if (i.AttributeTag == AttributeTag)
			return i;
	}
	return FExtendedAttribute();
}


float UEFAttributeComponent::GetAttributeValue(const FGameplayTag AttributeTag) const
{
	for ( const auto attribute : CalculatedAttributes)
	{
		if (attribute.AttributeTag == AttributeTag)
			return attribute.AttributeValue;
	}
	return -1;
}



void UEFAttributeComponent::GetTwoAttributeValue(const FGameplayTag AttributeOne, const FGameplayTag AttributeTwo,float& ValueOne, float& ValueTwo) const
{
	ValueOne = GetAttributeValue(AttributeOne);
	ValueTwo = GetAttributeValue(AttributeTwo);
}



float UEFAttributeComponent::GetTwoAttributePercent(const FGameplayTag AttributeOne, const FGameplayTag AttributeTwo) const
{
	const float ValueOne = GetAttributeValue(AttributeOne);
	const float ValueTwo = GetAttributeValue(AttributeTwo);
	return ValueOne / ValueTwo;
}



bool UEFAttributeComponent::GetHasAttribute(const FGameplayTag AttributeTag) const
{
	for (const auto attribute : CalculatedAttributes)
	{
		if (attribute.AttributeTag == AttributeTag)
			return true;
	}
	return false;
}



bool UEFAttributeComponent::GetHasAnyAttribute(const TArray<FGameplayTag>& AttributeTags) const
{
	for (const auto attribute : CalculatedAttributes)
	{
		for (const auto tag : AttributeTags)
		{
			if (attribute.AttributeTag == tag)
				return true;
		}
	}
	return false;
	
}



bool UEFAttributeComponent::GetHasAllAttributes(const TArray<FGameplayTag>& AttributeTags) const
{
	for (const auto tag : AttributeTags)
	{
		if (!GetHasAttribute(tag))
			return false;
	}
	return true;
}



void UEFAttributeComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeStartupAttributes();

	if (GetOwner())
	{
		GetOwner()->OnTakePointDamage.AddDynamic(this, &UEFAttributeComponent::OwnerTakePointDamage);
		GetOwner()->OnTakeRadialDamage.AddDynamic(this, &UEFAttributeComponent::OwnerTakeRadialDamage);
		GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UEFAttributeComponent::OwnerTakeAnyDamage); 
	}

	CalculateAttributes();
	
	for (const auto i : CalculatedAttributes)
	{
		UpdateAndBroadcastLastModifiedAttribute(i);
	}
}

