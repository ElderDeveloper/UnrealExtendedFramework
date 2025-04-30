// KeyConflictWidget.h
// A widget that handles key binding conflicts

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputAction.h"
#include "KeyConflictWidget.generated.h"

// Delegate for when a conflict is resolved
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConflictResolved, bool, bSuccessful);

/**
 * Widget that handles key binding conflicts
 * Allows the user to swap bindings, override, or cancel
 */
UCLASS()
class UKeyConflictWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Initialize the widget
    virtual void NativeConstruct() override;
    
    // Initialize the widget with conflict information
    UFUNCTION(BlueprintCallable, Category = "Input")
    void InitializeWithConflict(UInputAction* NewAction, UInputAction* ExistingAction, const FKey& ConflictKey);
    
    // Choose to swap the key bindings
    UFUNCTION(BlueprintCallable, Category = "Input")
    void ChooseSwapBindings();
    
    // Choose to override the existing binding
    UFUNCTION(BlueprintCallable, Category = "Input")
    void ChooseOverrideBinding();
    
    // Cancel and keep the original bindings
    UFUNCTION(BlueprintCallable, Category = "Input")
    void ChooseCancel();
    
    // Get the name of the new action
    UFUNCTION(BlueprintCallable, Category = "Input")
    FText GetNewActionNameText() const;
    
    // Get the name of the existing action
    UFUNCTION(BlueprintCallable, Category = "Input")
    FText GetExistingActionNameText() const;
    
    // Get the name of the conflicting key
    UFUNCTION(BlueprintCallable, Category = "Input")
    FText GetConflictingKeyText() const;
    
    // Event dispatched when a decision is made
    UPROPERTY(BlueprintAssignable, Category = "Input")
    FOnConflictResolved OnConflictResolved;

private:
    // The input action being newly bound
    UPROPERTY()
    UInputAction* NewInputAction;
    
    // The input action that already uses the key
    UPROPERTY()
    UInputAction* ExistingInputAction;
    
    // The key causing the conflict
    FKey ConflictingKey;
    
    // Reference to the input binding subsystem
    UPROPERTY()
    class UInputBindingSubsystem* InputBindingSubsystem;
};