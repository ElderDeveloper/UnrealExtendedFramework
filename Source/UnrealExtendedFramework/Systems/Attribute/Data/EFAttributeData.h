 // Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "UObject/Object.h"
#include "EFAttributeData.generated.h"


 class UEFAttributeObject;

USTRUCT(BlueprintType)
struct FExtendedAttribute : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category= "Attribute")
	FGameplayTag AttributeTag = FGameplayTag::EmptyTag;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category= "Attribute")
	float AttributeValue = 0;

	FExtendedAttribute()
	{
		AttributeTag = FGameplayTag::EmptyTag;
		AttributeValue = 0;
	}
	FExtendedAttribute(FGameplayTag attributeTag , float attributeValue)
	{
		AttributeTag = attributeTag;
		AttributeValue = attributeValue;
	}
};


USTRUCT(BlueprintType)
struct FExtendedAttributeDependency
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category= "Attribute")
	FGameplayTag AttributeTag = FGameplayTag::EmptyTag;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category= "Attribute")
	TArray<FExtendedAttribute> AttributeDependencies;
	
};


USTRUCT(BlueprintType)
struct FExtendedAttributeSettings : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category= "Attribute")
	FGameplayTag AttributeTag = FGameplayTag::EmptyTag;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category= "Attribute")
	FText Name = FText::FromString("Attribute Name");

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category= "Attribute")
	FText Description = FText::FromString("Attribute Description");

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category= "Attribute")
	TSubclassOf<UEFAttributeObject> AttributeObjectClass;
};


UENUM(BlueprintType)
enum EExtendedAttributeCompare
{
	Bigger,
	Smaller,
	Equal,
	NotEqual,
	BiggerOrEqual,
	SmallerOrEqual
};


UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAttributeData : public UObject
{
	GENERATED_BODY()
};
