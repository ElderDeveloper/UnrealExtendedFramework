// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EFExtendedAbilityInterface.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UEFExtendedAbilityInterface : public UInterface
{
	GENERATED_BODY()
};

class UEFExtendedAbilityComponent;

class UNREALEXTENDEDFRAMEWORK_API IEFExtendedAbilityInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	UFUNCTION(BlueprintNativeEvent , BlueprintCallable , Category="ExtendedAbility")
	void GetExtendedMovementInputAxis(float& ForwardAxis , float& RightAxis);

	UFUNCTION(BlueprintNativeEvent , BlueprintCallable , Category="ExtendedAbility")
	void GetExtendedCameraInputAxis(float& UpAxis , float& RightAxis);

	UFUNCTION(BlueprintNativeEvent , BlueprintCallable , Category="ExtendedAbility")
	UEFExtendedAbilityComponent* GetExtendedAbilityComponent();
	
};
