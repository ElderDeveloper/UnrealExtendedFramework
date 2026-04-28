// EFInputBindingSubsystem.h
// A Blueprint-friendly facade for Enhanced Input user key mappings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "EFInputBindingSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEFInputBindingsChanged);

USTRUCT(BlueprintType)
struct FEFInputKeyProfileInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Input|Profiles")
    FString ProfileId;

    UPROPERTY(BlueprintReadOnly, Category = "Input|Profiles")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Input|Profiles")
    bool bIsActive = false;
};

USTRUCT(BlueprintType)
struct FEFInputKeyConflict
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Input|Conflicts")
    bool bHasConflict = false;

    UPROPERTY(BlueprintReadOnly, Category = "Input|Conflicts")
    TObjectPtr<UInputAction> BoundAction = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Input|Conflicts")
    FName MappingName = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Input|Conflicts")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Input|Conflicts")
    FKey Key;

    UPROPERTY(BlueprintReadOnly, Category = "Input|Conflicts")
    EPlayerMappableKeySlot Slot = EPlayerMappableKeySlot::Unspecified;

    UPROPERTY(BlueprintReadOnly, Category = "Input|Conflicts")
    FString ProfileId;
};

/**
 * Subsystem that exposes Enhanced Input's player mappable key/profile system to Blueprint.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFInputBindingSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UPROPERTY(BlueprintAssignable, Category = "Input|Enhanced Input")
    FEFInputBindingsChanged OnInputBindingsChanged;

    UFUNCTION(BlueprintPure, Category = "Input|Enhanced Input")
    bool HasEnhancedInputUserSettings() const;

    UFUNCTION(BlueprintPure, Category = "Input|Enhanced Input")
    FString GetActiveKeyProfileId() const;

    UFUNCTION(BlueprintPure, Category = "Input|Enhanced Input")
    TArray<FEFInputKeyProfileInfo> GetAvailableKeyProfiles() const;

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input")
    bool SetActiveKeyProfile(const FString& ProfileId, bool bSaveImmediately = true);

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input")
    bool CreateKeyProfile(const FString& ProfileId, const FText& DisplayName, bool bSetAsActive = true, bool bSaveImmediately = true);

    UFUNCTION(BlueprintPure, Category = "Input|Enhanced Input")
    TArray<FPlayerKeyMapping> GetPlayerKeyMappings(EPlayerMappableKeySlot Slot = EPlayerMappableKeySlot::Unspecified, bool bOnlyValidMappings = true, const FString& ProfileId = TEXT("")) const;

    UFUNCTION(BlueprintPure, Category = "Input|Enhanced Input")
    FPlayerKeyMapping GetPlayerKeyMapping(const FText& KeyName) const;

    UFUNCTION(BlueprintPure, Category = "Input|Enhanced Input")
    FPlayerKeyMapping GetPlayerKeyMappingByName(FName MappingName, EPlayerMappableKeySlot Slot = EPlayerMappableKeySlot::First, const FString& ProfileId = TEXT("")) const;

    UFUNCTION(BlueprintPure, Category = "Input|Enhanced Input")
    FName GetMappingNameForAction(UInputAction* InputAction, UInputMappingContext* MappingContext = nullptr) const;

    UFUNCTION(BlueprintPure, Category = "Input|Enhanced Input")
    TArray<FKey> GetKeysForMappingName(FName MappingName, EPlayerMappableKeySlot Slot = EPlayerMappableKeySlot::Unspecified, const FString& ProfileId = TEXT("")) const;

    UFUNCTION(BlueprintPure, Category = "Input|Enhanced Input")
    TArray<FKey> GetKeysForInputAction(UInputAction* InputAction, EPlayerMappableKeySlot Slot = EPlayerMappableKeySlot::Unspecified, const FString& ProfileId = TEXT("")) const;

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input")
    FKey GetCurrentKeyBinding(UInputAction* InputAction, int32 MappingGroup = 0) const;

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input")
    bool SetKeyBinding(UInputAction* InputAction, FKey NewKey, int32 MappingGroup = 0, UInputMappingContext* MappingContext = nullptr, bool bSaveImmediately = true, bool bCreateSlotIfNeeded = true);

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input")
    bool SetPlayerKeyMapping(FName MappingName, FKey NewKey, EPlayerMappableKeySlot Slot = EPlayerMappableKeySlot::First, const FString& ProfileId = TEXT(""), FName HardwareDeviceId = NAME_None, bool bSaveImmediately = true, bool bCreateSlotIfNeeded = true);

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input", meta = (AutoCreateRefTerm = "FailureReason"))
    bool MapPlayerKey(const FMapPlayerKeyArgs& Args, FGameplayTagContainer& FailureReason, bool bSaveImmediately = true);

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input", meta = (AutoCreateRefTerm = "FailureReason"))
    bool UnmapPlayerKey(const FMapPlayerKeyArgs& Args, FGameplayTagContainer& FailureReason, bool bSaveImmediately = true);

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input", meta = (AutoCreateRefTerm = "FailureReason"))
    bool ResetPlayerKeyMapping(FName MappingName, FGameplayTagContainer& FailureReason, EPlayerMappableKeySlot Slot = EPlayerMappableKeySlot::First, const FString& ProfileId = TEXT(""), bool bSaveImmediately = true);

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input", meta = (AutoCreateRefTerm = "FailureReason"))
    bool ResetActiveKeyProfileToDefault(FGameplayTagContainer& FailureReason, bool bSaveImmediately = true);

    UFUNCTION(BlueprintPure, Category = "Input|Enhanced Input")
    FEFInputKeyConflict FindKeyConflict(FKey Key, UInputAction* ActionToIgnore = nullptr, EPlayerMappableKeySlot Slot = EPlayerMappableKeySlot::Unspecified, const FString& ProfileId = TEXT("")) const;

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input")
    bool IsKeyAlreadyBound(FKey Key, UInputAction*& OutBoundAction, int32& OutMappingGroup) const;

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input")
    bool SwapKeyBindings(UInputAction* ActionA, UInputAction* ActionB, int32 MappingGroupA = 0, int32 MappingGroupB = 0, bool bSaveImmediately = true);

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input")
    bool RegisterMappingContext(UInputMappingContext* MappingContext);

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input")
    bool UnregisterMappingContext(UInputMappingContext* MappingContext);

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input")
    bool IsMappingContextRegistered(UInputMappingContext* MappingContext) const;

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input")
    void AddMappingContext(UInputMappingContext* MappingContext, int32 Priority, bool bRegisterWithUserSettings = true, bool bForceImmediately = false);

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input")
    void RemoveMappingContext(UInputMappingContext* MappingContext, bool bUnregisterFromUserSettings = false, bool bForceImmediately = false);

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input")
    void ApplyKeyBindings(bool bForceImmediately = true);

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input")
    bool SaveKeyBindings();

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input")
    bool AsyncSaveKeyBindings();

    UFUNCTION(BlueprintCallable, Category = "Input|Enhanced Input")
    FString DumpActiveKeyProfileToString() const;

private:
    ULocalPlayer* GetLocalPlayer() const;
    UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;
    UEnhancedInputUserSettings* GetUserSettings() const;
    UEnhancedPlayerMappableKeyProfile* GetKeyProfile(const FString& ProfileId = TEXT("")) const;
    FName ResolveMappingNameForAction(UInputAction* InputAction, UInputMappingContext* MappingContext = nullptr) const;
    FMapPlayerKeyArgs MakeMapPlayerKeyArgs(FName MappingName, FKey NewKey, EPlayerMappableKeySlot Slot, const FString& ProfileId, FName HardwareDeviceId, bool bCreateSlotIfNeeded) const;
    bool ApplyAndSave(UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem, UEnhancedInputUserSettings* UserSettings, bool bSaveImmediately);
    static EPlayerMappableKeySlot MappingGroupToSlot(int32 MappingGroup);
    static int32 SlotToMappingGroup(EPlayerMappableKeySlot Slot);
};
