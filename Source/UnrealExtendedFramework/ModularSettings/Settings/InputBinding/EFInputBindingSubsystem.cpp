#include "EFInputBindingSubsystem.h"

#include "EnhancedActionKeyMapping.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerMappableKeySettings.h"
#include "UnrealExtendedFramework.h"

void UEFInputBindingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UEFInputBindingSubsystem::Deinitialize()
{
    OnInputBindingsChanged.Clear();
    Super::Deinitialize();
}

bool UEFInputBindingSubsystem::HasEnhancedInputUserSettings() const
{
    return GetUserSettings() != nullptr;
}

FString UEFInputBindingSubsystem::GetActiveKeyProfileId() const
{
    if (const UEnhancedInputUserSettings* UserSettings = GetUserSettings())
    {
        return UserSettings->GetActiveKeyProfileId();
    }

    return FString();
}

TArray<FEFInputKeyProfileInfo> UEFInputBindingSubsystem::GetAvailableKeyProfiles() const
{
    TArray<FEFInputKeyProfileInfo> Profiles;

    const UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    if (!UserSettings)
    {
        return Profiles;
    }

    const FString ActiveProfileId = UserSettings->GetActiveKeyProfileId();
    for (const TPair<FString, TObjectPtr<UEnhancedPlayerMappableKeyProfile>>& ProfilePair : UserSettings->GetAllAvailableKeyProfiles())
    {
        if (!ProfilePair.Value)
        {
            continue;
        }

        FEFInputKeyProfileInfo ProfileInfo;
        ProfileInfo.ProfileId = ProfilePair.Key;
        ProfileInfo.DisplayName = ProfilePair.Value->GetProfileDisplayName();
        ProfileInfo.bIsActive = ProfilePair.Key == ActiveProfileId;
        Profiles.Add(ProfileInfo);
    }

    return Profiles;
}

bool UEFInputBindingSubsystem::SetActiveKeyProfile(const FString& ProfileId, bool bSaveImmediately)
{
    UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = GetEnhancedInputSubsystem();
    UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    if (!EnhancedInputSubsystem || !UserSettings || ProfileId.IsEmpty())
    {
        return false;
    }

    const bool bWasSet = UserSettings->SetActiveKeyProfile(ProfileId);
    if (!bWasSet)
    {
        UE_LOG(LogExtendedFramework, Warning, TEXT("[EFInputBindingSubsystem] Failed to activate key profile '%s'"), *ProfileId);
        return false;
    }

    return ApplyAndSave(EnhancedInputSubsystem, UserSettings, bSaveImmediately);
}

bool UEFInputBindingSubsystem::CreateKeyProfile(const FString& ProfileId, const FText& DisplayName, bool bSetAsActive, bool bSaveImmediately)
{
    UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = GetEnhancedInputSubsystem();
    UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (!EnhancedInputSubsystem || !UserSettings || !LocalPlayer || ProfileId.IsEmpty())
    {
        return false;
    }

    FPlayerMappableKeyProfileCreationArgs Args;
    Args.ProfileStringIdentifier = ProfileId;
    Args.DisplayName = DisplayName;
    Args.UserId = LocalPlayer->GetPlatformUserId();
    Args.bSetAsCurrentProfile = bSetAsActive;

    UEnhancedPlayerMappableKeyProfile* NewProfile = UserSettings->CreateNewKeyProfile(Args);
    if (!NewProfile)
    {
        UE_LOG(LogExtendedFramework, Warning, TEXT("[EFInputBindingSubsystem] Failed to create key profile '%s'"), *ProfileId);
        return false;
    }

    return ApplyAndSave(EnhancedInputSubsystem, UserSettings, bSaveImmediately);
}

TArray<FPlayerKeyMapping> UEFInputBindingSubsystem::GetPlayerKeyMappings(EPlayerMappableKeySlot Slot, bool bOnlyValidMappings, const FString& ProfileId) const
{
    TArray<FPlayerKeyMapping> PlayerKeyMappings;

    const UEnhancedPlayerMappableKeyProfile* KeyProfile = GetKeyProfile(ProfileId);
    if (!KeyProfile)
    {
        return PlayerKeyMappings;
    }

    for (const TPair<FName, FKeyMappingRow>& Row : KeyProfile->GetPlayerMappingRows())
    {
        for (const FPlayerKeyMapping& Mapping : Row.Value.Mappings)
        {
            if (Slot != EPlayerMappableKeySlot::Unspecified && Mapping.GetSlot() != Slot)
            {
                continue;
            }

            if (bOnlyValidMappings && !Mapping.IsValid())
            {
                continue;
            }

            PlayerKeyMappings.Add(Mapping);
        }
    }

    return PlayerKeyMappings;
}

FPlayerKeyMapping UEFInputBindingSubsystem::GetPlayerKeyMapping(const FText& KeyName) const
{
    const FString TargetName = KeyName.ToString();
    for (const FPlayerKeyMapping& Mapping : GetPlayerKeyMappings())
    {
        if (Mapping.GetDisplayName().ToString() == TargetName || Mapping.GetMappingName().ToString() == TargetName)
        {
            return Mapping;
        }
    }

    return FPlayerKeyMapping();
}

FPlayerKeyMapping UEFInputBindingSubsystem::GetPlayerKeyMappingByName(FName MappingName, EPlayerMappableKeySlot Slot, const FString& ProfileId) const
{
    const UEnhancedPlayerMappableKeyProfile* KeyProfile = GetKeyProfile(ProfileId);
    if (!KeyProfile || MappingName.IsNone())
    {
        return FPlayerKeyMapping();
    }

    const FKeyMappingRow* Row = KeyProfile->FindKeyMappingRow(MappingName);
    if (!Row)
    {
        return FPlayerKeyMapping();
    }

    for (const FPlayerKeyMapping& Mapping : Row->Mappings)
    {
        if (Slot == EPlayerMappableKeySlot::Unspecified || Mapping.GetSlot() == Slot)
        {
            return Mapping;
        }
    }

    return FPlayerKeyMapping();
}

FName UEFInputBindingSubsystem::GetMappingNameForAction(UInputAction* InputAction, UInputMappingContext* MappingContext) const
{
    return ResolveMappingNameForAction(InputAction, MappingContext);
}

TArray<FKey> UEFInputBindingSubsystem::GetKeysForMappingName(FName MappingName, EPlayerMappableKeySlot Slot, const FString& ProfileId) const
{
    TArray<FKey> Keys;

    const UEnhancedPlayerMappableKeyProfile* KeyProfile = GetKeyProfile(ProfileId);
    if (!KeyProfile || MappingName.IsNone())
    {
        return Keys;
    }

    const FKeyMappingRow* Row = KeyProfile->FindKeyMappingRow(MappingName);
    if (!Row)
    {
        return Keys;
    }

    for (const FPlayerKeyMapping& Mapping : Row->Mappings)
    {
        const FKey CurrentKey = Mapping.GetCurrentKey();
        if ((Slot == EPlayerMappableKeySlot::Unspecified || Mapping.GetSlot() == Slot) && CurrentKey.IsValid())
        {
            Keys.Add(CurrentKey);
        }
    }

    return Keys;
}

TArray<FKey> UEFInputBindingSubsystem::GetKeysForInputAction(UInputAction* InputAction, EPlayerMappableKeySlot Slot, const FString& ProfileId) const
{
    TArray<FKey> Keys;
    if (!InputAction)
    {
        return Keys;
    }

    const UEnhancedPlayerMappableKeyProfile* KeyProfile = GetKeyProfile(ProfileId);
    if (KeyProfile)
    {
        for (const TPair<FName, FKeyMappingRow>& Row : KeyProfile->GetPlayerMappingRows())
        {
            for (const FPlayerKeyMapping& Mapping : Row.Value.Mappings)
            {
                if (Mapping.GetAssociatedInputAction() != InputAction)
                {
                    continue;
                }

                const FKey CurrentKey = Mapping.GetCurrentKey();
                if ((Slot == EPlayerMappableKeySlot::Unspecified || Mapping.GetSlot() == Slot) && CurrentKey.IsValid())
                {
                    Keys.Add(CurrentKey);
                }
            }
        }
    }

    if (Keys.Num() == 0)
    {
        if (const UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = GetEnhancedInputSubsystem())
        {
            Keys = EnhancedInputSubsystem->QueryKeysMappedToAction(InputAction);
        }
    }

    return Keys;
}

FKey UEFInputBindingSubsystem::GetCurrentKeyBinding(UInputAction* InputAction, int32 MappingGroup) const
{
    const TArray<FKey> Keys = GetKeysForInputAction(InputAction, MappingGroupToSlot(MappingGroup));
    return Keys.Num() > 0 ? Keys[0] : FKey();
}

bool UEFInputBindingSubsystem::SetKeyBinding(UInputAction* InputAction, FKey NewKey, int32 MappingGroup, UInputMappingContext* MappingContext, bool bSaveImmediately, bool bCreateSlotIfNeeded)
{
    if (!InputAction || !NewKey.IsValid())
    {
        return false;
    }

    if (MappingContext)
    {
        RegisterMappingContext(MappingContext);
    }

    const FName MappingName = ResolveMappingNameForAction(InputAction, MappingContext);
    if (MappingName.IsNone())
    {
        UE_LOG(LogExtendedFramework, Warning, TEXT("[EFInputBindingSubsystem] Could not resolve a player mappable mapping name for action '%s'"), *GetNameSafe(InputAction));
        return false;
    }

    return SetPlayerKeyMapping(MappingName, NewKey, MappingGroupToSlot(MappingGroup), FString(), NAME_None, bSaveImmediately, bCreateSlotIfNeeded);
}

bool UEFInputBindingSubsystem::SetPlayerKeyMapping(FName MappingName, FKey NewKey, EPlayerMappableKeySlot Slot, const FString& ProfileId, FName HardwareDeviceId, bool bSaveImmediately, bool bCreateSlotIfNeeded)
{
    if (MappingName.IsNone() || !NewKey.IsValid())
    {
        return false;
    }

    FMapPlayerKeyArgs Args = MakeMapPlayerKeyArgs(MappingName, NewKey, Slot, ProfileId, HardwareDeviceId, bCreateSlotIfNeeded);
    FGameplayTagContainer FailureReason;
    return MapPlayerKey(Args, FailureReason, bSaveImmediately);
}

bool UEFInputBindingSubsystem::MapPlayerKey(const FMapPlayerKeyArgs& Args, FGameplayTagContainer& FailureReason, bool bSaveImmediately)
{
    FailureReason.Reset();

    UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = GetEnhancedInputSubsystem();
    UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    if (!EnhancedInputSubsystem || !UserSettings || Args.MappingName.IsNone() || !Args.NewKey.IsValid())
    {
        return false;
    }

    UserSettings->MapPlayerKey(Args, FailureReason);
    if (!FailureReason.IsEmpty())
    {
        UE_LOG(LogExtendedFramework, Warning, TEXT("[EFInputBindingSubsystem] MapPlayerKey failed for '%s': %s"), *Args.MappingName.ToString(), *FailureReason.ToString());
        return false;
    }

    return ApplyAndSave(EnhancedInputSubsystem, UserSettings, bSaveImmediately);
}

bool UEFInputBindingSubsystem::UnmapPlayerKey(const FMapPlayerKeyArgs& Args, FGameplayTagContainer& FailureReason, bool bSaveImmediately)
{
    FailureReason.Reset();

    UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = GetEnhancedInputSubsystem();
    UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    if (!EnhancedInputSubsystem || !UserSettings || Args.MappingName.IsNone())
    {
        return false;
    }

    UserSettings->UnMapPlayerKey(Args, FailureReason);
    if (!FailureReason.IsEmpty())
    {
        UE_LOG(LogExtendedFramework, Warning, TEXT("[EFInputBindingSubsystem] UnmapPlayerKey failed for '%s': %s"), *Args.MappingName.ToString(), *FailureReason.ToString());
        return false;
    }

    return ApplyAndSave(EnhancedInputSubsystem, UserSettings, bSaveImmediately);
}

bool UEFInputBindingSubsystem::ResetPlayerKeyMapping(FName MappingName, FGameplayTagContainer& FailureReason, EPlayerMappableKeySlot Slot, const FString& ProfileId, bool bSaveImmediately)
{
    FailureReason.Reset();

    UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = GetEnhancedInputSubsystem();
    UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    if (!EnhancedInputSubsystem || !UserSettings || MappingName.IsNone())
    {
        return false;
    }

    FMapPlayerKeyArgs Args;
    Args.MappingName = MappingName;
    Args.Slot = Slot;
    Args.ProfileIdString = ProfileId;

    UserSettings->ResetAllPlayerKeysInRow(Args, FailureReason);
    if (!FailureReason.IsEmpty())
    {
        UE_LOG(LogExtendedFramework, Warning, TEXT("[EFInputBindingSubsystem] ResetPlayerKeyMapping failed for '%s': %s"), *MappingName.ToString(), *FailureReason.ToString());
        return false;
    }

    return ApplyAndSave(EnhancedInputSubsystem, UserSettings, bSaveImmediately);
}

bool UEFInputBindingSubsystem::ResetActiveKeyProfileToDefault(FGameplayTagContainer& FailureReason, bool bSaveImmediately)
{
    FailureReason.Reset();

    UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = GetEnhancedInputSubsystem();
    UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    if (!EnhancedInputSubsystem || !UserSettings)
    {
        return false;
    }

    const FString ActiveProfileId = UserSettings->GetActiveKeyProfileId();
    UserSettings->ResetKeyProfileIdToDefault(ActiveProfileId, FailureReason);
    if (!FailureReason.IsEmpty())
    {
        UE_LOG(LogExtendedFramework, Warning, TEXT("[EFInputBindingSubsystem] ResetActiveKeyProfileToDefault failed: %s"), *FailureReason.ToString());
        return false;
    }

    return ApplyAndSave(EnhancedInputSubsystem, UserSettings, bSaveImmediately);
}

FEFInputKeyConflict UEFInputBindingSubsystem::FindKeyConflict(FKey Key, UInputAction* ActionToIgnore, EPlayerMappableKeySlot Slot, const FString& ProfileId) const
{
    FEFInputKeyConflict Conflict;
    if (!Key.IsValid())
    {
        return Conflict;
    }

    const UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    const UEnhancedPlayerMappableKeyProfile* KeyProfile = GetKeyProfile(ProfileId);
    if (!UserSettings || !KeyProfile)
    {
        return Conflict;
    }

    for (const TPair<FName, FKeyMappingRow>& Row : KeyProfile->GetPlayerMappingRows())
    {
        for (const FPlayerKeyMapping& Mapping : Row.Value.Mappings)
        {
            if (Mapping.GetCurrentKey() != Key)
            {
                continue;
            }

            if (Slot != EPlayerMappableKeySlot::Unspecified && Mapping.GetSlot() != Slot)
            {
                continue;
            }

            if (ActionToIgnore && Mapping.GetAssociatedInputAction() == ActionToIgnore)
            {
                continue;
            }

            Conflict.bHasConflict = true;
            Conflict.BoundAction = const_cast<UInputAction*>(Mapping.GetAssociatedInputAction());
            Conflict.MappingName = Mapping.GetMappingName();
            Conflict.DisplayName = Mapping.GetDisplayName();
            Conflict.Key = Key;
            Conflict.Slot = Mapping.GetSlot();
            Conflict.ProfileId = ProfileId.IsEmpty() ? UserSettings->GetActiveKeyProfileId() : ProfileId;
            return Conflict;
        }
    }

    return Conflict;
}

bool UEFInputBindingSubsystem::IsKeyAlreadyBound(FKey Key, UInputAction*& OutBoundAction, int32& OutMappingGroup) const
{
    OutBoundAction = nullptr;
    OutMappingGroup = 0;

    const FEFInputKeyConflict Conflict = FindKeyConflict(Key);
    if (!Conflict.bHasConflict)
    {
        return false;
    }

    OutBoundAction = Conflict.BoundAction;
    OutMappingGroup = SlotToMappingGroup(Conflict.Slot);
    return true;
}

bool UEFInputBindingSubsystem::SwapKeyBindings(UInputAction* ActionA, UInputAction* ActionB, int32 MappingGroupA, int32 MappingGroupB, bool bSaveImmediately)
{
    if (!ActionA || !ActionB)
    {
        return false;
    }

    const EPlayerMappableKeySlot SlotA = MappingGroupToSlot(MappingGroupA);
    const EPlayerMappableKeySlot SlotB = MappingGroupToSlot(MappingGroupB);
    const FName MappingNameA = ResolveMappingNameForAction(ActionA);
    const FName MappingNameB = ResolveMappingNameForAction(ActionB);
    const FKey KeyA = GetCurrentKeyBinding(ActionA, MappingGroupA);
    const FKey KeyB = GetCurrentKeyBinding(ActionB, MappingGroupB);

    if (MappingNameA.IsNone() || MappingNameB.IsNone() || !KeyA.IsValid() || !KeyB.IsValid())
    {
        UE_LOG(LogExtendedFramework, Warning, TEXT("[EFInputBindingSubsystem] SwapKeyBindings failed because one or both mappings were invalid"));
        return false;
    }

    const bool bSuccessA = SetPlayerKeyMapping(MappingNameA, KeyB, SlotA, FString(), NAME_None, false);
    if (!bSuccessA)
    {
        return false;
    }

    const bool bSuccessB = SetPlayerKeyMapping(MappingNameB, KeyA, SlotB, FString(), NAME_None, false);
    if (!bSuccessB)
    {
        SetPlayerKeyMapping(MappingNameA, KeyA, SlotA, FString(), NAME_None, false);
        return false;
    }

    if (bSaveImmediately)
    {
        SaveKeyBindings();
    }

    return true;
}

bool UEFInputBindingSubsystem::RegisterMappingContext(UInputMappingContext* MappingContext)
{
    UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    if (!UserSettings || !MappingContext)
    {
        return false;
    }

    const bool bRegistered = UserSettings->RegisterInputMappingContext(MappingContext);
    if (bRegistered)
    {
        OnInputBindingsChanged.Broadcast();
    }

    return bRegistered;
}

bool UEFInputBindingSubsystem::UnregisterMappingContext(UInputMappingContext* MappingContext)
{
    UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    if (!UserSettings || !MappingContext)
    {
        return false;
    }

    const bool bUnregistered = UserSettings->UnregisterInputMappingContext(MappingContext);
    if (bUnregistered)
    {
        OnInputBindingsChanged.Broadcast();
    }

    return bUnregistered;
}

bool UEFInputBindingSubsystem::IsMappingContextRegistered(UInputMappingContext* MappingContext) const
{
    const UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    return UserSettings && MappingContext && UserSettings->IsMappingContextRegistered(MappingContext);
}

void UEFInputBindingSubsystem::AddMappingContext(UInputMappingContext* MappingContext, int32 Priority, bool bRegisterWithUserSettings, bool bForceImmediately)
{
    UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = GetEnhancedInputSubsystem();
    if (!EnhancedInputSubsystem || !MappingContext)
    {
        return;
    }

    FModifyContextOptions Options;
    Options.bNotifyUserSettings = bRegisterWithUserSettings;
    Options.bForceImmediately = bForceImmediately;
    EnhancedInputSubsystem->AddMappingContext(MappingContext, Priority, Options);
}

void UEFInputBindingSubsystem::RemoveMappingContext(UInputMappingContext* MappingContext, bool bUnregisterFromUserSettings, bool bForceImmediately)
{
    UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = GetEnhancedInputSubsystem();
    if (!EnhancedInputSubsystem || !MappingContext)
    {
        return;
    }

    FModifyContextOptions Options;
    Options.bNotifyUserSettings = bUnregisterFromUserSettings;
    Options.bForceImmediately = bForceImmediately;
    EnhancedInputSubsystem->RemoveMappingContext(MappingContext, Options);
}

void UEFInputBindingSubsystem::ApplyKeyBindings(bool bForceImmediately)
{
    UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = GetEnhancedInputSubsystem();
    UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    if (!EnhancedInputSubsystem || !UserSettings)
    {
        return;
    }

    UserSettings->ApplySettings();

    FModifyContextOptions Options;
    Options.bForceImmediately = bForceImmediately;
    Options.bIgnoreAllPressedKeysUntilRelease = true;
    EnhancedInputSubsystem->RequestRebuildControlMappings(Options, EInputMappingRebuildType::RebuildWithFlush);
    OnInputBindingsChanged.Broadcast();
}

bool UEFInputBindingSubsystem::SaveKeyBindings()
{
    UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    if (!UserSettings)
    {
        return false;
    }

    UserSettings->SaveSettings();
    return true;
}

bool UEFInputBindingSubsystem::AsyncSaveKeyBindings()
{
    UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    if (!UserSettings)
    {
        return false;
    }

    UserSettings->AsyncSaveSettings();
    return true;
}

FString UEFInputBindingSubsystem::DumpActiveKeyProfileToString() const
{
    const UEnhancedPlayerMappableKeyProfile* KeyProfile = GetKeyProfile();
    return KeyProfile ? KeyProfile->ToString() : FString();
}

ULocalPlayer* UEFInputBindingSubsystem::GetLocalPlayer() const
{
    if (const UGameInstance* GameInstance = GetGameInstance())
    {
        if (ULocalPlayer* LocalPlayer = GameInstance->GetFirstGamePlayer())
        {
            return LocalPlayer;
        }
    }

    if (const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        return PlayerController->GetLocalPlayer();
    }

    return nullptr;
}

UEnhancedInputLocalPlayerSubsystem* UEFInputBindingSubsystem::GetEnhancedInputSubsystem() const
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    return LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
}

UEnhancedInputUserSettings* UEFInputBindingSubsystem::GetUserSettings() const
{
    UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = GetEnhancedInputSubsystem();
    return EnhancedInputSubsystem ? EnhancedInputSubsystem->GetUserSettings() : nullptr;
}

UEnhancedPlayerMappableKeyProfile* UEFInputBindingSubsystem::GetKeyProfile(const FString& ProfileId) const
{
    UEnhancedInputUserSettings* UserSettings = GetUserSettings();
    if (!UserSettings)
    {
        return nullptr;
    }

    return ProfileId.IsEmpty() ? UserSettings->GetActiveKeyProfile() : UserSettings->GetKeyProfileWithId(ProfileId);
}

FName UEFInputBindingSubsystem::ResolveMappingNameForAction(UInputAction* InputAction, UInputMappingContext* MappingContext) const
{
    if (!InputAction)
    {
        return NAME_None;
    }

    if (MappingContext)
    {
        for (const FEnhancedActionKeyMapping& ActionKeyMapping : MappingContext->GetMappings())
        {
            if (ActionKeyMapping.Action == InputAction && ActionKeyMapping.IsPlayerMappable())
            {
                return ActionKeyMapping.GetMappingName();
            }
        }
    }

    if (const UEnhancedPlayerMappableKeyProfile* KeyProfile = GetKeyProfile())
    {
        for (const TPair<FName, FKeyMappingRow>& Row : KeyProfile->GetPlayerMappingRows())
        {
            for (const FPlayerKeyMapping& Mapping : Row.Value.Mappings)
            {
                if (Mapping.GetAssociatedInputAction() == InputAction)
                {
                    return Mapping.GetMappingName();
                }
            }
        }
    }

    if (const UPlayerMappableKeySettings* KeySettings = InputAction->GetPlayerMappableKeySettings())
    {
        const FName MappingName = KeySettings->GetMappingName();
        if (!MappingName.IsNone())
        {
            return MappingName;
        }
    }

    return InputAction->GetFName();
}

FMapPlayerKeyArgs UEFInputBindingSubsystem::MakeMapPlayerKeyArgs(FName MappingName, FKey NewKey, EPlayerMappableKeySlot Slot, const FString& ProfileId, FName HardwareDeviceId, bool bCreateSlotIfNeeded) const
{
    FMapPlayerKeyArgs Args;
    Args.MappingName = MappingName;
    Args.NewKey = NewKey;
    Args.Slot = Slot == EPlayerMappableKeySlot::Unspecified ? EPlayerMappableKeySlot::First : Slot;
    Args.ProfileIdString = ProfileId;
    Args.HardwareDeviceId = HardwareDeviceId;
    Args.bCreateMatchingSlotIfNeeded = bCreateSlotIfNeeded;
    return Args;
}

bool UEFInputBindingSubsystem::ApplyAndSave(UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem, UEnhancedInputUserSettings* UserSettings, bool bSaveImmediately)
{
    if (!EnhancedInputSubsystem || !UserSettings)
    {
        return false;
    }

    UserSettings->ApplySettings();

    FModifyContextOptions Options;
    Options.bForceImmediately = true;
    Options.bIgnoreAllPressedKeysUntilRelease = true;
    EnhancedInputSubsystem->RequestRebuildControlMappings(Options, EInputMappingRebuildType::RebuildWithFlush);

    if (bSaveImmediately)
    {
        UserSettings->SaveSettings();
    }

    OnInputBindingsChanged.Broadcast();
    return true;
}

EPlayerMappableKeySlot UEFInputBindingSubsystem::MappingGroupToSlot(int32 MappingGroup)
{
    switch (MappingGroup)
    {
    case 0:
        return EPlayerMappableKeySlot::First;
    case 1:
        return EPlayerMappableKeySlot::Second;
    case 2:
        return EPlayerMappableKeySlot::Third;
    case 3:
        return EPlayerMappableKeySlot::Fourth;
    case 4:
        return EPlayerMappableKeySlot::Fifth;
    case 5:
        return EPlayerMappableKeySlot::Sixth;
    case 6:
        return EPlayerMappableKeySlot::Seventh;
    default:
        return EPlayerMappableKeySlot::First;
    }
}

int32 UEFInputBindingSubsystem::SlotToMappingGroup(EPlayerMappableKeySlot Slot)
{
    switch (Slot)
    {
    case EPlayerMappableKeySlot::First:
        return 0;
    case EPlayerMappableKeySlot::Second:
        return 1;
    case EPlayerMappableKeySlot::Third:
        return 2;
    case EPlayerMappableKeySlot::Fourth:
        return 3;
    case EPlayerMappableKeySlot::Fifth:
        return 4;
    case EPlayerMappableKeySlot::Sixth:
        return 5;
    case EPlayerMappableKeySlot::Seventh:
        return 6;
    default:
        return 0;
    }
}

