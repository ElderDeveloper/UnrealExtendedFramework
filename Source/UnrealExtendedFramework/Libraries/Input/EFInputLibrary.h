// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/InputSettings.h"
#include "EFInputLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFInputLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	
public:

	UFUNCTION(BlueprintPure , Category="Extended Input")
	static bool DoesKeyExistInActionMapping(UInputSettings* InputSettings,FKey Key,FInputActionKeyMapping& FoundMapping);

	UFUNCTION(BlueprintPure , Category="Extended Input")
	static bool DoesKeyExistInAxisMapping(UInputSettings* InputSettings,FKey Key,FInputAxisKeyMapping& FoundMapping);

	UFUNCTION(BlueprintPure , Category="Extended Input")
	static FName GetKeyBindingName(UInputSettings* InputSettings,FKey Key);


	
	UFUNCTION(BlueprintCallable , Category="Extended Input | Action")
	static bool UpdateKeyBindingAction(FInputActionKeyMapping ChangeActionMapping);

	UFUNCTION(BlueprintCallable, Category="Extended Input | Action")
	static bool AddKeyBindingAction(FInputActionKeyMapping ChangeActionMapping);

	UFUNCTION(BlueprintCallable, Category="Extended Input | Action")
	static bool RemoveKeyBindingAction(FInputActionKeyMapping ChangeActionMapping);


	
	UFUNCTION(BlueprintCallable, Category="Extended Input | Axis")
	static bool UpdateKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping);

	UFUNCTION(BlueprintCallable, Category="Extended Input | Axis")
	static bool AddKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping);

	UFUNCTION(BlueprintCallable, Category="Extended Input | Axis")
	static bool RemoveKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping);
};
