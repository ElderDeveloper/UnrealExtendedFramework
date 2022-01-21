// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerInput.h"
#include "ESInputSubsystem.generated.h"


class UInputSettings;

DECLARE_LOG_CATEGORY_EXTERN(LogSettingsInput,Error,All);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnButtonStateUpdated , FName , ButtonName , FKey , NewKey , FKey , OldKey );

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDSETTINGS_API UESInputSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()


	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override {}


public:

	FOnButtonStateUpdated OnButtonStateUpdated;

	UFUNCTION(BlueprintPure , Category="Input Subsystem")
	bool DoesKeyExistInActionMapping(UInputSettings* InputSettings,FKey Key,FInputActionKeyMapping& FoundMapping);

	
	UFUNCTION(BlueprintPure , Category="Input Subsystem")
	bool DoesKeyExistInAxisMapping(UInputSettings* InputSettings,FKey Key,FInputAxisKeyMapping& FoundMapping);

	
	UFUNCTION(BlueprintCallable , Category="Input Subsystem | Action")
	bool UpdateKeyBindingAction(FInputActionKeyMapping ChangeActionMapping);

	
	UFUNCTION(BlueprintCallable, Category="Input Subsystem | Action")
	bool AddKeyBindingAction(FInputActionKeyMapping ChangeActionMapping);

	
	UFUNCTION(BlueprintCallable, Category="Input Subsystem | Action")
	bool RemoveKeyBindingAction(FInputActionKeyMapping ChangeActionMapping);

	UFUNCTION(BlueprintCallable, Category="Input Subsystem | Action")
	bool UpdateKeyBindingActionForAll(FInputActionKeyMapping ChangeActionMapping , FKey OldKey);

	
	UFUNCTION(BlueprintCallable, Category="Input Subsystem | Axis")
	bool UpdateKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping);

	
	UFUNCTION(BlueprintCallable, Category="Input Subsystem | Axis")
	bool AddKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping);

	
	UFUNCTION(BlueprintCallable, Category="Input Subsystem | Axis")
	bool RemoveKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping);

	
	UFUNCTION(BlueprintCallable, Category="Input Subsystem | Axis")
	bool UpdateKeyBindingAxisForAll(FInputAxisKeyMapping ChangeAxisMapping , FKey OldKey);

	UFUNCTION(BlueprintCallable, Category="Input Subsystem | All")
	bool UpdateKeyBindingForAll(FKey NewKey , FKey OldKey , FName BindingName);
};
