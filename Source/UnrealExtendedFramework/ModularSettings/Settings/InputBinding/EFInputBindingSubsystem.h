// EFInputBindingSubsystem.h
// A subsystem that manages key bindings for Enhanced Input actions

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "EnhancedInput/Public/UserSettings/EnhancedInputUserSettings.h"
#include "EnhancedInputSubsystems.h"
#include "EFInputBindingSubsystem.generated.h"

/**
 * Subsystem that manages key bindings for Enhanced Input actions
 * Provides functions to get, set, save, and load key bindings
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFInputBindingSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // Initialize and shutdown functions
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintPure)
    TArray<FPlayerKeyMapping> GetPlayerKeyMappings() const;

    UFUNCTION(BlueprintPure)
    FPlayerKeyMapping GetPlayerKeyMapping(const FText& KeyName) const;
    
    // Get the current key binding for an input action
    UFUNCTION(BlueprintCallable, Category = "Input")
    FKey GetCurrentKeyBinding(UInputAction* InputAction) const;

    // Set a new key binding for an input action
    // MappingContext is optional — if nullptr, the system will search all active contexts
    UFUNCTION(BlueprintCallable, Category = "Input")
    bool SetKeyBinding(UInputAction* InputAction, FKey NewKey, int32 MappingGroup = 0, UInputMappingContext* MappingContext = nullptr);

    // Check if a key is already bound to another action
    // Returns true if bound, and fills OutBoundAction / OutMappingGroup
    UFUNCTION(BlueprintCallable, Category = "Input")
    bool IsKeyAlreadyBound(FKey Key, UInputAction*& OutBoundAction, int32& OutMappingGroup) const;

    // Swap key bindings between two actions
    UFUNCTION(BlueprintCallable, Category = "Input")
    bool SwapKeyBindings(UInputAction* ActionA, UInputAction* ActionB);

private:
    // Get the Enhanced Input subsystem for the local player
    UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;

    // Helper: get user settings from the Enhanced Input subsystem
    UEnhancedInputUserSettings* GetUserSettings() const;
};
