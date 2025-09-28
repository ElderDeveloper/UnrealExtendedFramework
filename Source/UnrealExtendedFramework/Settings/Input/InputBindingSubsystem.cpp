
#include "InputBindingSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"

UEnhancedInputLocalPlayerSubsystem* UInputBindingSubsystem::GetEnhancedInputSubsystem() const
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


void UInputBindingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}


void UInputBindingSubsystem::Deinitialize()
{
    Super::Deinitialize();
}


TArray<FPlayerKeyMapping> UInputBindingSubsystem::GetPlayerKeyMappings() const
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
    
    const auto Mappings = UserSettings->GetCurrentKeyProfile()->GetPlayerMappingRows();
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


FPlayerKeyMapping UInputBindingSubsystem::GetPlayerKeyMapping(const FText& KeyName) const
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
    
    const auto Mappings = UserSettings->GetCurrentKeyProfile()->GetPlayerMappingRows();
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


FKey UInputBindingSubsystem::GetCurrentKeyBinding(UInputAction* InputAction) const
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

