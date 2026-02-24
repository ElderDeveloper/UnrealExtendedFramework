
#include "EFInputBindingSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerMappableKeySettings.h"

UEnhancedInputLocalPlayerSubsystem* UEFInputBindingSubsystem::GetEnhancedInputSubsystem() const
{
    // Get the local player
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PlayerController)
    {
        return nullptr;
    }
    
    // Get the local player subsystem
    ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
    if (!LocalPlayer)
    {
        return nullptr;
    }
    
    // Return the Enhanced Input subsystem
    return LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
}


void UEFInputBindingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}


void UEFInputBindingSubsystem::Deinitialize()
{
    Super::Deinitialize();
}


TArray<FPlayerKeyMapping> UEFInputBindingSubsystem::GetPlayerKeyMappings() const
{
    UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = GetEnhancedInputSubsystem();
    if (!EnhancedInputSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get Enhanced Input subsystem!"));
        return TArray<FPlayerKeyMapping>();
    }
    
    const auto UserSettings = EnhancedInputSubsystem->GetUserSettings();
    if (!UserSettings)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get Enhanced Input user settings!"));
        return TArray<FPlayerKeyMapping>();
    }
    
    const auto Mappings = UserSettings->GetActiveKeyProfile()->GetPlayerMappingRows();
    UE_LOG(LogTemp, Warning, TEXT("Mappings: %d"), Mappings.Num());

    TArray<FPlayerKeyMapping> PlayerKeyMappings;
    for (const auto& Mapping : Mappings)
    {
        for (const auto& KeyMapping : Mapping.Value.Mappings)
        {
            FString SlotName = *UEnum::GetValueAsString(KeyMapping.GetSlot());
            UE_LOG(LogTemp, Warning, TEXT("Mappings SlotName: %s"), *SlotName);
            if (KeyMapping.GetSlot() == EPlayerMappableKeySlot::First)
            {
                PlayerKeyMappings.Add(KeyMapping);
            }
        }
    }
    return PlayerKeyMappings;
}


FPlayerKeyMapping UEFInputBindingSubsystem::GetPlayerKeyMapping(const FText& KeyName) const
{
    UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = GetEnhancedInputSubsystem();
    if (!EnhancedInputSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get Enhanced Input subsystem!"));
        return FPlayerKeyMapping();
    }
    
    const auto UserSettings = EnhancedInputSubsystem->GetUserSettings();
    if (!UserSettings)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get Enhanced Input user settings!"));
        return FPlayerKeyMapping();
    }
    
    const auto Mappings = UserSettings->GetActiveKeyProfile()->GetPlayerMappingRows();
    for (const auto& Mapping : Mappings)
    {
        for (const auto& KeyMapping : Mapping.Value.Mappings)
        {
            if (KeyMapping.GetDisplayName().ToString() == KeyName.ToString())
            {
                return KeyMapping;
            }
        }
    }
    
    return FPlayerKeyMapping();
}


FKey UEFInputBindingSubsystem::GetCurrentKeyBinding(UInputAction* InputAction) const
{
    if (!InputAction)
        return FKey();
    
    if (UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = GetEnhancedInputSubsystem())
    {
        TArray<FKey> MappedKeys = EnhancedInputSubsystem->QueryKeysMappedToAction(InputAction);
        if (MappedKeys.Num() > 0)
        {
            return MappedKeys[0];
        }
    }
    
    return FKey();
}


UEnhancedInputUserSettings* UEFInputBindingSubsystem::GetUserSettings() const
{
    if (UEnhancedInputLocalPlayerSubsystem* EIS = GetEnhancedInputSubsystem())
    {
        return EIS->GetUserSettings();
    }
    return nullptr;
}


bool UEFInputBindingSubsystem::SetKeyBinding(UInputAction* InputAction, FKey NewKey, int32 MappingGroup, UInputMappingContext* MappingContext)
{
    if (!InputAction || !NewKey.IsValid())
    {
        return false;
    }
    
    UEnhancedInputLocalPlayerSubsystem* EIS = GetEnhancedInputSubsystem();
    UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    if (!EIS || !UserSettings)
    {
        UE_LOG(LogTemp, Error, TEXT("[EFInputBindingSubsystem] SetKeyBinding: Enhanced Input subsystem or user settings not available"));
        return false;
    }
    
    FMapPlayerKeyArgs Args;
    
    // Get the mapping name from the input action's player mappable key settings
    if (const UPlayerMappableKeySettings* KeySettings = InputAction->GetPlayerMappableKeySettings())
    {
        Args.MappingName = KeySettings->GetMappingName();
    }
    else
    {
        // Fallback to the input action's FName
        Args.MappingName = InputAction->GetFName();
    }
    
    Args.NewKey = NewKey;
    Args.Slot = EPlayerMappableKeySlot::First;
    
    FGameplayTagContainer FailureReason;
    UserSettings->MapPlayerKey(Args, FailureReason);
    
    if (!FailureReason.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EFInputBindingSubsystem] SetKeyBinding failed: %s"), *FailureReason.ToString());
        return false;
    }
    
    // Rebuild control mappings so the change takes effect immediately
    EIS->RequestRebuildControlMappings(FModifyContextOptions());
    
    UE_LOG(LogTemp, Log, TEXT("[EFInputBindingSubsystem] SetKeyBinding: %s -> %s"), *InputAction->GetName(), *NewKey.ToString());
    return true;
}


bool UEFInputBindingSubsystem::IsKeyAlreadyBound(FKey Key, UInputAction*& OutBoundAction, int32& OutMappingGroup) const
{
    OutBoundAction = nullptr;
    OutMappingGroup = 0;
    
    if (!Key.IsValid())
    {
        return false;
    }
    
    UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    if (!UserSettings)
    {
        return false;
    }
    
    UEnhancedPlayerMappableKeyProfile* KeyProfile = UserSettings->GetActiveKeyProfile();
    if (!KeyProfile)
    {
        return false;
    }
    
    const TMap<FName, FKeyMappingRow>& MappingRows = KeyProfile->GetPlayerMappingRows();
    
    for (const auto& Row : MappingRows)
    {
        for (const FPlayerKeyMapping& Mapping : Row.Value.Mappings)
        {
            if (Mapping.GetCurrentKey() == Key)
            {
                OutBoundAction = const_cast<UInputAction*>(Mapping.GetAssociatedInputAction());
                OutMappingGroup = 0; // Enhanced Input uses slots, not groups — report slot 0
                return true;
            }
        }
    }
    
    return false;
}


bool UEFInputBindingSubsystem::SwapKeyBindings(UInputAction* ActionA, UInputAction* ActionB)
{
    if (!ActionA || !ActionB)
    {
        return false;
    }
    
    // Get current keys for both actions
    FKey KeyA = GetCurrentKeyBinding(ActionA);
    FKey KeyB = GetCurrentKeyBinding(ActionB);
    
    if (!KeyA.IsValid() || !KeyB.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EFInputBindingSubsystem] SwapKeyBindings: one or both actions have no valid key binding"));
        return false;
    }
    
    // Set ActionA to KeyB and ActionB to KeyA
    // We pass nullptr for MappingContext — the system will resolve it automatically
    bool bSuccessA = SetKeyBinding(ActionA, KeyB);
    bool bSuccessB = SetKeyBinding(ActionB, KeyA);
    
    if (bSuccessA && bSuccessB)
    {
        UE_LOG(LogTemp, Log, TEXT("[EFInputBindingSubsystem] SwapKeyBindings: %s <-> %s"), *ActionA->GetName(), *ActionB->GetName());
        return true;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("[EFInputBindingSubsystem] SwapKeyBindings: partial failure (A=%s, B=%s)"), 
        bSuccessA ? TEXT("OK") : TEXT("FAIL"), bSuccessB ? TEXT("OK") : TEXT("FAIL"));
    return false;
}

