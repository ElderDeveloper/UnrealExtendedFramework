// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealExtendedFramework/Systems/Attribute/Data/EFAttributeData.h"
#include "EFAttributeLibrary.generated.h"

class UEFAttributeComponent;



/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAttributeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable , Category= "Extended Attribute")
	static void AddAttributeValue(UPARAM(Ref) TArray<FExtendedAttribute>& TargetAttributes ,  const FGameplayTag Attribute, const float Value = 0);

	UFUNCTION(BlueprintCallable , Category= "Extended Attribute")
	static void AddAttributes(UPARAM(Ref) TArray<FExtendedAttribute>& TargetAttributes , const TArray<FExtendedAttribute>& Attributes);

	UFUNCTION(BlueprintCallable , Category= "Extended Attribute")
	static void RemoveAttributeValue(UPARAM(Ref) TArray<FExtendedAttribute>& TargetAttributes , const FGameplayTag Attribute, const float Value = 0);


	// Getters

	UFUNCTION(BlueprintPure , Category= "Extended Attribute")
	static TArray<FExtendedAttribute> GetMultipliedAttributes(const TArray<FExtendedAttribute>& Attributes , const float Multiplier);

	UFUNCTION(BlueprintPure , Category= "Extended Attribute")
	static UEFAttributeComponent* GetAttributeComponent(const AActor* Actor);

	UFUNCTION(BlueprintPure , Category= "Extended Attribute")
	static FExtendedAttribute FindAttribute(const TArray<FExtendedAttribute>& TargetAttributes , const FGameplayTag Attribute);
	
	UFUNCTION(BlueprintPure , Category= "Extended Attribute")
	static bool GetAttributeValue(const TArray<FExtendedAttribute>& TargetAttributes , const FGameplayTag Attribute , float& OutValue);

	UFUNCTION(BlueprintPure , Category= "Extended Attribute")
	static FText GetAttributeName(const TArray<FExtendedAttribute>& TargetAttributes , const FGameplayTag Attribute);

	UFUNCTION(BlueprintPure , Category= "Extended Attribute")
	static bool GetAttributeTagEqual(const FExtendedAttribute& TargetAttribute , const FGameplayTag Attribute);

	UFUNCTION(BlueprintPure , Category= "Extended Attribute")
	static float GetAttributeValueFromAttribute(const FExtendedAttribute& Attribute);

	UFUNCTION(BlueprintPure , Category= "Extended Attribute")
	static void GetAttributeDebugDetails(const FExtendedAttribute& Attribute , FString& AttributeTag , float& AttributeValue);

	// Compares Attribute A with Attribute B and returns the result in Result, Bigger = AttributeA > AttributeB
	UFUNCTION(BlueprintCallable ,meta=(ExpandBoolAsExecs=Result), Category= "Extended Attribute")
	static void CompareTwoAttributes(const UEFAttributeComponent* AttributeComponent , const FGameplayTag AttributeA , const FGameplayTag AttributeB , EExtendedAttributeCompare CompareType , float& AttributeAValue , float& AttributeBValue , bool& Result);
	
};
