// KeyRebindingWidget.cpp
// Implementation of the KeyRebindingWidget class

#include "KeyRebindingWidget.h"
#include "UnrealExtendedFramework/Settings/Input/InputBindingSubsystem.h"


void UKeyRebindingWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Get the input binding subsystem
    InputBindingSubsystem = GetGameInstance()->GetSubsystem<UInputBindingSubsystem>();
    
    // Initialize default values
    bIsListeningForInput = false;
    TargetInputAction = nullptr;
    CurrentMappingGroup = 0;
    NewKey = FKey();
}

void UKeyRebindingWidget::InitializeForRebinding(UInputAction* ActionToRebind, int32 MappingGroup)
{
    TargetInputAction = ActionToRebind;
    CurrentMappingGroup = MappingGroup;
    
    // Reset the new key
    NewKey = FKey();
    
    // Not listening for input yet
    bIsListeningForInput = false;
}

void UKeyRebindingWidget::StartRebinding()
{
    if (TargetInputAction)
    {
        // Start listening for input
        bIsListeningForInput = true;
    }
}

void UKeyRebindingWidget::CancelRebinding()
{
    // Stop listening for input
    bIsListeningForInput = false;
    
    // Reset the new key
    NewKey = FKey();
    
    // Notify that rebinding was cancelled
    OnRebindingComplete.Broadcast(TargetInputAction, NewKey, false);
}

void UKeyRebindingWidget::ConfirmRebinding()
{
    if (TargetInputAction && NewKey.IsValid() && InputBindingSubsystem)
    {
        // Set the new key binding
        //bool bSuccess = InputBindingSubsystem->SetKeyBinding(TargetInputAction, NewKey, CurrentMappingGroup);
        
        // Notify that rebinding is complete
       // OnRebindingComplete.Broadcast(TargetInputAction, NewKey, bSuccess);
    }
    else
    {
        // Notify that rebinding failed
        OnRebindingComplete.Broadcast(TargetInputAction, NewKey, false);
    }
    
    // Stop listening for input
    bIsListeningForInput = false;
}

FText UKeyRebindingWidget::GetCurrentKeyBindingText() const
{
    if (TargetInputAction && InputBindingSubsystem)
    {
        FKey CurrentKey = InputBindingSubsystem->GetCurrentKeyBinding(TargetInputAction);
        if (CurrentKey.IsValid())
        {
            return CurrentKey.GetDisplayName();
        }
    }
    
    return FText::FromString(TEXT("Not Bound"));
}

FText UKeyRebindingWidget::GetActionNameText() const
{
    if (TargetInputAction)
    {
        return FText::FromName(TargetInputAction->GetFName());
    }
    
    return FText::GetEmpty();
}

FReply UKeyRebindingWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (bIsListeningForInput)
    {
        // Get the pressed key
        FKey PressedKey = InKeyEvent.GetKey();
        
        // Handle the key selection
        HandleKeySelected(PressedKey);
        
        // Consume the input
        return FReply::Handled();
    }
    
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UKeyRebindingWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsListeningForInput)
    {
        // Get the pressed mouse button
        FKey PressedKey = InMouseEvent.GetEffectingButton();
        
        // Handle the key selection
        HandleKeySelected(PressedKey);
        
        // Consume the input
        return FReply::Handled();
    }
    
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UKeyRebindingWidget::HandleKeySelected(const FKey& SelectedKey)
{
    if (SelectedKey.IsValid() && TargetInputAction && InputBindingSubsystem)
    {
        // Store the selected key
        NewKey = SelectedKey;
        
        // Check if the key is already bound to another action
        UInputAction* BoundAction = nullptr;
        int32 BoundMappingGroup = 0;

        /*
        if (InputBindingSubsystem->IsKeyAlreadyBound(NewKey, BoundAction, BoundMappingGroup) && 
            BoundAction != TargetInputAction)
        {
            // Notify about the conflict
            OnKeyConflictDetected.Broadcast(TargetInputAction, BoundAction, NewKey);
        }
        else
        {
            // No conflict, confirm the rebinding
            ConfirmRebinding();
        }*/
        
        // Stop listening for input
        bIsListeningForInput = false;
    }
}