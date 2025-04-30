// InputBindingSubsystem.h
// A subsystem that manages key bindings for Enhanced Input actions

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "EnhancedInput/Public/UserSettings/EnhancedInputUserSettings.h"
#include "EnhancedInputSubsystems.h"
#include "InputBindingSubsystem.generated.h"

/**
 * Subsystem that manages key bindings for Enhanced Input actions
 * Provides functions to get, set, save, and load key bindings
 */
UCLASS()
class UInputBindingSubsystem : public UGameInstanceSubsystem
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

    /*
    // Set a new key binding for an input action
    UFUNCTION(BlueprintCallable, Category = "Input")
    bool SetKeyBinding(UInputAction* InputAction, const FKey& NewKey, int32 MappingGroup = 0);
    
    // Check if a key is already bound to another action
    UFUNCTION(BlueprintCallable, Category = "Input")
    bool IsKeyAlreadyBound(const FKey& Key, const UInputAction* BoundAction, int32& MappingGroup) const;
    
    // Swap key bindings between two actions
    UFUNCTION(BlueprintCallable, Category = "Input")
    bool SwapKeyBindings(UInputAction* FirstAction, UInputAction* SecondAction, int32 FirstMappingGroup = 0, int32 SecondMappingGroup = 0);
    
    // Save current key bindings to config
    UFUNCTION(BlueprintCallable, Category = "Input")
    void SaveKeyBindings();
    
    // Load key bindings from config
    UFUNCTION(BlueprintCallable, Category = "Input")
    void LoadKeyBindings();
    
    // Apply current key bindings to the Enhanced Input system
    UFUNCTION(BlueprintCallable, Category = "Input")
    void ApplyKeyBindings();
    */

private:
    // Get the Enhanced Input subsystem for the local player
    UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;
    

};