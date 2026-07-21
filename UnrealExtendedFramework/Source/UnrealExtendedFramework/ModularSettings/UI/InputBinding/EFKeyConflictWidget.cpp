// EFKeyConflictWidget.cpp
// Implementation of the EFKeyConflictWidget class

#include "EFKeyConflictWidget.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/InputBinding/EFInputBindingSubsystem.h"

void UEFKeyConflictWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Get the input binding subsystem
    InputBindingSubsystem = GetGameInstance()->GetSubsystem<UEFInputBindingSubsystem>();
    
    // Initialize default values
    NewInputAction = nullptr;
    ExistingInputAction = nullptr;
    ConflictingKey = FKey();
}

void UEFKeyConflictWidget::InitializeWithConflict(UInputAction* NewAction, UInputAction* ExistingAction, const FKey& ConflictKey)
{
    // Store the conflict information
    NewInputAction = NewAction;
    ExistingInputAction = ExistingAction;
    ConflictingKey = ConflictKey;
}

void UEFKeyConflictWidget::ChooseSwapBindings()
{
    bool bSuccess = false;
    
    if (NewInputAction && ExistingInputAction && ConflictingKey.IsValid() && InputBindingSubsystem)
    {
        // Swap the key bindings between the two actions
        bSuccess = InputBindingSubsystem->SwapKeyBindings(NewInputAction, ExistingInputAction);
    }
    
    // Notify that the conflict has been resolved
    OnConflictResolved.Broadcast(bSuccess);
}

void UEFKeyConflictWidget::ChooseOverrideBinding()
{
    bool bSuccess = false;
    
    if (NewInputAction && ConflictingKey.IsValid() && InputBindingSubsystem)
    {
        // Set the new key binding, which will override any existing bindings
        bSuccess = InputBindingSubsystem->SetKeyBinding(NewInputAction, ConflictingKey);
    }
    
    // Notify that the conflict has been resolved
    OnConflictResolved.Broadcast(bSuccess);
}

void UEFKeyConflictWidget::ChooseCancel()
{
    // No changes made, just notify that the conflict resolution was cancelled
    OnConflictResolved.Broadcast(false);
}

FText UEFKeyConflictWidget::GetNewActionNameText() const
{
    if (NewInputAction)
    {
        return FText::FromName(NewInputAction->GetFName());
    }
    
    return FText::GetEmpty();
}

FText UEFKeyConflictWidget::GetExistingActionNameText() const
{
    if (ExistingInputAction)
    {
        return FText::FromName(ExistingInputAction->GetFName());
    }
    
    return FText::GetEmpty();
}

FText UEFKeyConflictWidget::GetConflictingKeyText() const
{
    if (ConflictingKey.IsValid())
    {
        return ConflictingKey.GetDisplayName();
    }
    
    return FText::FromString(TEXT("None"));
}
