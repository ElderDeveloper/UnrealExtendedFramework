// KeyRebindingWidget.h
// A widget that allows players to rebind keys for input actions

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputAction.h"
#include "KeyRebindingWidget.generated.h"

// Delegate for when rebinding is complete
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRebindingComplete, UInputAction*, InputAction, FKey, NewKey, bool, bSuccessful);

// Delegate for when a key conflict is detected
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnKeyConflictDetected, UInputAction*, NewAction, UInputAction*, ExistingAction, FKey, ConflictKey);

/**
 * Widget that allows players to rebind keys for input actions
 * Captures key presses and assigns them to the specified input action
 */
UCLASS()
class UKeyRebindingWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Initialize the widget
    virtual void NativeConstruct() override;
    
    // Initialize the widget with an input action to rebind
    UFUNCTION(BlueprintCallable, Category = "Input")
    void InitializeForRebinding(UInputAction* ActionToRebind, int32 MappingGroup = 0);
    
    // Start listening for key presses
    UFUNCTION(BlueprintCallable, Category = "Input")
    void StartRebinding();
    
    // Cancel the rebinding process
    UFUNCTION(BlueprintCallable, Category = "Input")
    void CancelRebinding();
    
    // Confirm the new key binding
    UFUNCTION(BlueprintCallable, Category = "Input")
    void ConfirmRebinding();
    
    // Get the name of the current key binding
    UFUNCTION(BlueprintCallable, Category = "Input")
    FText GetCurrentKeyBindingText() const;
    
    // Get the name of the input action being rebound
    UFUNCTION(BlueprintCallable, Category = "Input")
    FText GetActionNameText() const;
    
    // Event dispatched when rebinding is complete
    UPROPERTY(BlueprintAssignable, Category = "Input")
    FOnRebindingComplete OnRebindingComplete;
    
    // Event dispatched when a key conflict is detected
    UPROPERTY(BlueprintAssignable, Category = "Input")
    FOnKeyConflictDetected OnKeyConflictDetected;

protected:
    // Override keyboard input to capture key presses
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    
    // Override mouse input to capture mouse button presses
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    // The input action being rebound
    UPROPERTY()
    UInputAction* TargetInputAction;
    
    // The mapping group for the input action
    int32 CurrentMappingGroup;
    
    // The newly selected key
    FKey NewKey;
    
    // Whether we're currently listening for input
    bool bIsListeningForInput;
    
    // Reference to the input binding subsystem
    UPROPERTY()
    class UInputBindingSubsystem* InputBindingSubsystem;
    
    // Handle a new key selection
    void HandleKeySelected(const FKey& SelectedKey);
};