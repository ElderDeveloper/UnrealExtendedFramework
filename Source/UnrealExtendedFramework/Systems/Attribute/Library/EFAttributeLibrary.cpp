// Fill out your copyright notice in the Description page of Project Settings.


#include "EFAttributeLibrary.h"

#include "UnrealExtendedFramework/Systems/Attribute/EFAttributeComponent.h"

void UEFAttributeLibrary::AddAttributeValue(TArray<FExtendedAttribute>& TargetAttributes, const FGameplayTag Attribute,const float Value)
{
	for (int32 i = 0 ; i < TargetAttributes.Num() ; i++)
	{
		if (TargetAttributes.IsValidIndex(i))
		{
			if (TargetAttributes[i].AttributeTag == Attribute)
			{
				TargetAttributes[i].AttributeValue += Value;
				return;
			}
		}
	}
	TargetAttributes.Add(FExtendedAttribute(Attribute , Value));
}

void UEFAttributeLibrary::AddAttributes(TArray<FExtendedAttribute>& TargetAttributes,const TArray<FExtendedAttribute>& Attributes)
{
	for (const auto i : Attributes)
	{
		AddAttributeValue(TargetAttributes , i.AttributeTag , i.AttributeValue);
	}
}

void UEFAttributeLibrary::RemoveAttributeValue(TArray<FExtendedAttribute>& TargetAttributes,const FGameplayTag Attribute, const float Value)
{
	for ( int i = 0; i < TargetAttributes.Num(); i++)
	{
		if (TargetAttributes.IsValidIndex(i))
		{
			if (TargetAttributes[i].AttributeTag == Attribute)
			{
				TargetAttributes[i].AttributeValue -= Value;
				return;
			}
		}
	}
}

TArray<FExtendedAttribute> UEFAttributeLibrary::GetMultipliedAttributes(const TArray<FExtendedAttribute>& Attributes,const float Multiplier)
{
	TArray<FExtendedAttribute>  MultipliedAttributes;
	for (const auto i : Attributes)
	{
		MultipliedAttributes.Add(FExtendedAttribute(i.AttributeTag , i.AttributeValue * Multiplier));
	}
	return MultipliedAttributes;
}


UEFAttributeComponent* UEFAttributeLibrary::GetAttributeComponent(const AActor* Actor)
{
	if (Actor)
	{
		if (const auto AttributeComp = Actor->FindComponentByClass<UEFAttributeComponent>())
		{
			return AttributeComp;
		}
		else
		{
			UE_LOG(LogTemp , Warning , TEXT("Actor %s does not have a EFAttributeComponent") , *Actor->GetName());
		}
	}
	return nullptr;
}


FExtendedAttribute UEFAttributeLibrary::FindAttribute(const TArray<FExtendedAttribute>& TargetAttributes,const FGameplayTag Attribute)
{
	for (int32 i = 0 ; i < TargetAttributes.Num() ; i++)
	{
		if (TargetAttributes.IsValidIndex(i))
		{
			if (TargetAttributes[i].AttributeTag == Attribute)
			{
				return TargetAttributes[i];
			}
		}
	}
	return FExtendedAttribute( Attribute , -1);
}


bool UEFAttributeLibrary::GetAttributeValue(const TArray<FExtendedAttribute>& TargetAttributes,const FGameplayTag Attribute, float& OutValue)
{
	OutValue = FindAttribute(TargetAttributes , Attribute).AttributeValue;
	return OutValue != -1;
}


FText UEFAttributeLibrary::GetAttributeName(const TArray<FExtendedAttribute>& TargetAttributes,const FGameplayTag Attribute)
{
	return FText::FromString(FindAttribute(TargetAttributes , Attribute).AttributeTag.ToString());
}


bool UEFAttributeLibrary::GetAttributeTagEqual(const FExtendedAttribute& TargetAttribute,const FGameplayTag Attribute)
{
	return  TargetAttribute.AttributeTag == Attribute;
}


float UEFAttributeLibrary::GetAttributeValueFromAttribute(const FExtendedAttribute& Attribute)
{
	return Attribute.AttributeValue;
}

void UEFAttributeLibrary::GetAttributeDebugDetails(const FExtendedAttribute& Attribute, FString& AttributeTag,float& AttributeValue)
{
	AttributeTag = Attribute.AttributeTag.ToString();
	AttributeValue = Attribute.AttributeValue;
}

void UEFAttributeLibrary::CompareTwoAttributes(const UEFAttributeComponent* AttributeComponent,const FGameplayTag AttributeA, const FGameplayTag AttributeB, EExtendedAttributeCompare CompareType, float& AttributeAValue , float& AttributeBValue ,bool& Result)
{
	if (AttributeComponent)
	{
		AttributeAValue = -1;
		AttributeBValue = -1;
		AttributeComponent->GetTwoAttributeValue(AttributeA , AttributeB , AttributeAValue , AttributeBValue);

		switch (CompareType)
		{
		case Bigger:
			Result = AttributeAValue > AttributeBValue;
			break;

		case Smaller:
			Result = AttributeAValue < AttributeBValue;
			break;

		case Equal:
			Result = AttributeAValue == AttributeBValue;
			break;

		case NotEqual:
			Result = AttributeAValue != AttributeBValue;
			break;

		case BiggerOrEqual:
			Result = AttributeAValue >= AttributeBValue;
			break;

		case SmallerOrEqual:
			Result = AttributeAValue <= AttributeBValue;
			break;

			default: Result = false;
		}
	}
	else
	{
		Result = false;
	}
}

