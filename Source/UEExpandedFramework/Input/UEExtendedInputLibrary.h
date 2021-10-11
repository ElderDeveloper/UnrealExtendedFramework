// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/InputSettings.h"
#include "InputCore/Classes/InputCoreTypes.h"
#include "UEExtendedInputLibrary.generated.h"

/**
 * 
 */
class UInputSettings;
UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedInputLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure)
	static bool DoesKeyExistInActionMapping(UInputSettings* InputSettings,FKey Key,FInputActionKeyMapping& FoundMapping);

	static bool DoesKeyExistInAxisMapping(UInputSettings* InputSettings,FKey Key,FInputAxisKeyMapping& FoundMapping);

	static bool DoesKeyBindingExist(UInputSettings* InputSettings,FKey Key);

	static FName GetKeyBindingName(UInputSettings* InputSettings,FKey Key);
};
