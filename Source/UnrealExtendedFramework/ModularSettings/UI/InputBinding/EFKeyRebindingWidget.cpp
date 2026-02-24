// EFKeyRebindingWidget.cpp
// Implementation of the EFKeyRebindingWidget class

#include "EFKeyRebindingWidget.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/InputBinding/EFInputBindingSubsystem.h"


void UEFKeyRebindingWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Get the input binding subsystem
    InputBindingSubsystem = GetGameInstance()->GetSubsystem<UEFInputBindingSubsystem>();
    
    // Initialize default values
    bIsListeningForInput = false;
    TargetInputAction = nullptr;
    CurrentMappingGroup = 0;
    NewKey = FKey();
}

void UEFKeyRebindingWidget::InitializeForRebinding(UInputAction* ActionToRebind, int32 MappingGroup)
{
    TargetInputAction = ActionToRebind;
    CurrentMappingGroup = MappingGroup;
    
    // Reset the new key
    NewKey = FKey();
    
    // Not listening for input yet
    bIsListeningForInput = false;
}

void UEFKeyRebindingWidget::StartRebinding()
{
    if (TargetInputAction)
    {
        // Start listening for input
        bIsListeningForInput = true;
    }
}

void UEFKeyRebindingWidget::CancelRebinding()
{
    // Stop listening for input
    bIsListeningForInput = false;
    
    // Reset the new key
    NewKey = FKey();
    
    // Notify that rebinding was cancelled
    OnRebindingComplete.Broadcast(TargetInputAction, NewKey, false);
}

void UEFKeyRebindingWidget::ConfirmRebinding()
{
    if (TargetInputAction && NewKey.IsValid() && InputBindingSubsystem)
    {
        // Set the new key binding
        bool bSuccess = InputBindingSubsystem->SetKeyBinding(TargetInputAction, NewKey, CurrentMappingGroup);
        
        // Notify that rebinding is complete
        OnRebindingComplete.Broadcast(TargetInputAction, NewKey, bSuccess);
    }
    else
    {
        // Notify that rebinding failed
        OnRebindingComplete.Broadcast(TargetInputAction, NewKey, false);
    }
    
    // Stop listening for input
    bIsListeningForInput = false;
}

FText UEFKeyRebindingWidget::GetCurrentKeyBindingText() const
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

FText UEFKeyRebindingWidget::GetActionNameText() const
{
    if (TargetInputAction)
    {
        return FText::FromName(TargetInputAction->GetFName());
    }
    
    return FText::GetEmpty();
}

FReply UEFKeyRebindingWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
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

FReply UEFKeyRebindingWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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

void UEFKeyRebindingWidget::HandleKeySelected(const FKey& SelectedKey)
{
    if (SelectedKey.IsValid() && TargetInputAction && InputBindingSubsystem)
    {
        // Store the selected key
        NewKey = SelectedKey;
        
        // Check if the key is already bound to another action
        UInputAction* BoundAction = nullptr;
        int32 BoundMappingGroup = 0;

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
        }
        
        // Stop listening for input
        bIsListeningForInput = false;
    }
}
