// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/InputSettings.h"
#include "EFInputLibrary.generated.h"

/**
 * Blueprint function library for input key binding management and clipboard operations.
 *
 * @note The Action/Axis mapping functions use the legacy UE4 input system.
 * In UE 5.x, the recommended approach is Enhanced Input (UInputAction, UInputMappingContext).
 * These functions still work but are marked deprecated — consider migrating to Enhanced Input.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFInputLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	
public:

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Legacy Action Mappings >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Checks if a key is bound to any action mapping.
	 * @param InputSettings The input settings to search
	 * @param Key The key to find
	 * @param FoundMapping Output: the found action mapping
	 * @return True if the key was found in an action mapping
	 * @deprecated Use Enhanced Input (UInputAction) in UE 5.x.
	 */
	UFUNCTION(BlueprintPure, Category="Extended Input", meta=(DeprecatedFunction, DeprecationMessage="Use Enhanced Input system in UE 5.x"))
	static bool DoesKeyExistInActionMapping(UInputSettings* InputSettings, FKey Key, FInputActionKeyMapping& FoundMapping);

	/**
	 * Checks if a key is bound to any axis mapping.
	 * @param InputSettings The input settings to search
	 * @param Key The key to find
	 * @param FoundMapping Output: the found axis mapping
	 * @return True if the key was found in an axis mapping
	 * @deprecated Use Enhanced Input (UInputAction) in UE 5.x.
	 */
	UFUNCTION(BlueprintPure, Category="Extended Input", meta=(DeprecatedFunction, DeprecationMessage="Use Enhanced Input system in UE 5.x"))
	static bool DoesKeyExistInAxisMapping(UInputSettings* InputSettings, FKey Key, FInputAxisKeyMapping& FoundMapping);

	/**
	 * Returns the action or axis name associated with a key.
	 * Searches action mappings first, then axis mappings.
	 * @deprecated Use Enhanced Input (UInputAction) in UE 5.x.
	 */
	UFUNCTION(BlueprintPure, Category="Extended Input", meta=(DeprecatedFunction, DeprecationMessage="Use Enhanced Input system in UE 5.x"))
	static FName GetKeyBindingName(UInputSettings* InputSettings, FKey Key);

	/**
	 * Replaces the key in an existing action mapping (removes old, adds new).
	 * @deprecated Use Enhanced Input (UInputAction) in UE 5.x.
	 */
	UFUNCTION(BlueprintCallable, Category="Extended Input | Action", meta=(DeprecatedFunction, DeprecationMessage="Use Enhanced Input system in UE 5.x"))
	static bool UpdateKeyBindingAction(FInputActionKeyMapping ChangeActionMapping);

	/**
	 * Adds a new action mapping if the action name doesn't already exist.
	 * @deprecated Use Enhanced Input (UInputAction) in UE 5.x.
	 */
	UFUNCTION(BlueprintCallable, Category="Extended Input | Action", meta=(DeprecatedFunction, DeprecationMessage="Use Enhanced Input system in UE 5.x"))
	static bool AddKeyBindingAction(FInputActionKeyMapping ChangeActionMapping);

	/**
	 * Removes an action mapping by action name.
	 * @deprecated Use Enhanced Input (UInputAction) in UE 5.x.
	 */
	UFUNCTION(BlueprintCallable, Category="Extended Input | Action", meta=(DeprecatedFunction, DeprecationMessage="Use Enhanced Input system in UE 5.x"))
	static bool RemoveKeyBindingAction(FInputActionKeyMapping ChangeActionMapping);

	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Legacy Axis Mappings >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Replaces the key in an existing axis mapping (removes old, adds new).
	 * @deprecated Use Enhanced Input (UInputAction) in UE 5.x.
	 */
	UFUNCTION(BlueprintCallable, Category="Extended Input | Axis", meta=(DeprecatedFunction, DeprecationMessage="Use Enhanced Input system in UE 5.x"))
	static bool UpdateKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping);

	/**
	 * Adds a new axis mapping if the axis name doesn't already exist.
	 * @deprecated Use Enhanced Input (UInputAction) in UE 5.x.
	 */
	UFUNCTION(BlueprintCallable, Category="Extended Input | Axis", meta=(DeprecatedFunction, DeprecationMessage="Use Enhanced Input system in UE 5.x"))
	static bool AddKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping);

	/**
	 * Removes an axis mapping by axis name.
	 * @deprecated Use Enhanced Input (UInputAction) in UE 5.x.
	 */
	UFUNCTION(BlueprintCallable, Category="Extended Input | Axis", meta=(DeprecatedFunction, DeprecationMessage="Use Enhanced Input system in UE 5.x"))
	static bool RemoveKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Listing >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Returns all action mapping names currently registered.
	 * @deprecated Use Enhanced Input (UInputAction) in UE 5.x.
	 */
	UFUNCTION(BlueprintPure, Category="Extended Input", meta=(DeprecatedFunction, DeprecationMessage="Use Enhanced Input system in UE 5.x"))
	static TArray<FName> GetAllActionMappingNames();

	/**
	 * Returns all axis mapping names currently registered.
	 * @deprecated Use Enhanced Input (UInputAction) in UE 5.x.
	 */
	UFUNCTION(BlueprintPure, Category="Extended Input", meta=(DeprecatedFunction, DeprecationMessage="Use Enhanced Input system in UE 5.x"))
	static TArray<FName> GetAllAxisMappingNames();


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Clipboard >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Copies a string to the system clipboard. Cross-platform.
	 * @param TextToCopy The string to copy
	 * @return True on success
	 */
	UFUNCTION(BlueprintCallable, Category="Extended Input | Clipboard")
	static bool CopyStringToClipboard(const FString& TextToCopy);

	/**
	 * Returns the current text content of the system clipboard. Cross-platform.
	 * @return Clipboard text, or empty string if unavailable
	 */
	UFUNCTION(BlueprintPure, Category="Extended Input | Clipboard")
	static FString GetStringFromClipboard();
};
