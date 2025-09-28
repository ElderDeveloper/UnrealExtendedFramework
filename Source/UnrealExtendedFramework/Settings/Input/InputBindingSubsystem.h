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

private:
    // Get the Enhanced Input subsystem for the local player
    UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;
    

};