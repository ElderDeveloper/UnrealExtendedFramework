#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "InputCoreTypes.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "EnhancedInput/Public/UserSettings/EnhancedInputUserSettings.h"
#include "PlayerMappableKeySettings.h"
#include "EFInputSettings.generated.h"



// Forward declarations
class UInputAction;
class UInputMappingContext;

namespace EFInputSettingsHelper
{
	inline UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem()
	{
		if (!GEngine) return nullptr;
		
		APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
		if (!PC) return nullptr;
		
		ULocalPlayer* LP = PC->GetLocalPlayer();
		if (!LP) return nullptr;
		
		return LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	}
}

/**
 * Setting for remapping an Enhanced Input Action.
 * Uses Enhanced Input's PlayerMappableKey system for proper key rebinding.
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFModularSettingsInput : public UEFModularSettingsBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	FKey CurrentKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	FKey DefaultKey;

	UFUNCTION(BlueprintCallable,BlueprintNativeEvent, Category = "Settings")
	void SetKey(FKey NewKey);
	virtual void SetKey_Implementation(FKey NewKey) 
	{ 
		if (CurrentKey != NewKey)
		{
			CurrentKey = NewKey;
			MarkDirty();
		}
	}

	virtual void SaveCurrentValue() override { SavedKey = CurrentKey; Super::SaveCurrentValue(); }
	virtual void RevertToSavedValue() override { CurrentKey = SavedKey; Apply_Implementation(); Super::RevertToSavedValue(); }
	
	virtual void Apply_Implementation() override 
	{
		if (!InputAction || !CurrentKey.IsValid()) return;
		
		if (UEnhancedInputLocalPlayerSubsystem* EIS = EFInputSettingsHelper::GetEnhancedInputSubsystem())
		{
			if (UEnhancedInputUserSettings* UserSettings = EIS->GetUserSettings())
			{
				// Get the mapping name from the input action's player mappable key settings
				FName MappingName = NAME_None;
				if (const UPlayerMappableKeySettings* KeySettings = InputAction->GetPlayerMappableKeySettings())
				{
					MappingName = KeySettings->GetMappingName();
				}
				else
				{
					// Fallback to the input action's FName
					MappingName = InputAction->GetFName();
				}

				FMapPlayerKeyArgs Args;
				Args.MappingName = MappingName;
				Args.NewKey = CurrentKey;
				Args.Slot = EPlayerMappableKeySlot::First;

				FGameplayTagContainer FailureReason;
				UserSettings->MapPlayerKey(Args, FailureReason);
				
				EIS->RequestRebuildControlMappings(FModifyContextOptions());
				
				if (!FailureReason.IsEmpty())
				{
					UE_LOG(LogTemp, Warning, TEXT("[UEFModularSettingsInput] MapPlayerKey failed: %s"), *FailureReason.ToString());
				}
			}
		}
	}
	
	virtual FString GetValueAsString() const override
	{
		return CurrentKey.ToString();
	}
	
	virtual void SetValueFromString(const FString& Value) override
	{
		SetKey(FKey(*Value));
	}

	virtual FString GetSavedValueAsString() const override
	{
		return SavedKey.ToString();
	}
	
	virtual void ResetToDefault() override
	{
		SetKey(DefaultKey);
	}

protected:
	FKey SavedKey;
};





// Mouse Sensitivity Setting
UCLASS(Blueprintable, DisplayName = "Extended Mouse Sensitivity")
class UNREALEXTENDEDFRAMEWORK_API UEFMouseSensitivitySetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFMouseSensitivitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.MouseSensitivity"));
		DisplayName = NSLOCTEXT("Settings", "MouseSensitivity", "Mouse Sensitivity");
		ConfigCategory = TEXT("Controls");
		DefaultValue = 1.0f;
		
		Value = 1.0f;
		Min = 0.1f;
		Max = 5.0f;
	}
	
	virtual void Apply_Implementation() override
	{
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				PC->PlayerInput->SetMouseSensitivity(Value);
				UE_LOG(LogTemp, Log, TEXT("Applied Mouse Sensitivity: %.2f"), Value);
			}
		}
	}
};

// Mouse Sensitivity X Setting
UCLASS(Blueprintable, DisplayName = "Extended Mouse Sensitivity X")
class UNREALEXTENDEDFRAMEWORK_API UEFMouseSensitivityXSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFMouseSensitivityXSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.MouseSensitivityX"));
		DisplayName = NSLOCTEXT("Settings", "MouseSensitivityX", "Mouse Sensitivity X");
		ConfigCategory = TEXT("Controls");
		DefaultValue = 1.0f;
		
		Value = 1.0f;
		Min = 0.1f;
		Max = 5.0f;
	}
	
	virtual void Apply_Implementation() override
	{
		// Per-axis sensitivity is typically handled by a custom look input modifier 
		// on the Enhanced Input action. Store the value for the game to query.
		UE_LOG(LogTemp, Log, TEXT("Applied Mouse Sensitivity X: %.2f"), Value);
	}
};

// Mouse Sensitivity Y Setting
UCLASS(Blueprintable, DisplayName = "Extended Mouse Sensitivity Y")
class UNREALEXTENDEDFRAMEWORK_API UEFMouseSensitivityYSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFMouseSensitivityYSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.MouseSensitivityY"));
		DisplayName = NSLOCTEXT("Settings", "MouseSensitivityY", "Mouse Sensitivity Y");
		ConfigCategory = TEXT("Controls");
		DefaultValue = 1.0f;
		
		Value = 1.0f;
		Min = 0.1f;
		Max = 5.0f;
	}
	
	virtual void Apply_Implementation() override
	{
		// Per-axis sensitivity is typically handled by a custom look input modifier 
		// on the Enhanced Input action. Store the value for the game to query.
		UE_LOG(LogTemp, Log, TEXT("Applied Mouse Sensitivity Y: %.2f"), Value);
	}
};

// Reverse Mouse Y Setting — Enhanced Input approach: negate the Y-axis via input modifier
UCLASS(Blueprintable, DisplayName = "Extended Reverse Mouse Y")
class UNREALEXTENDEDFRAMEWORK_API UEFReverseMouseYSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()
	
public:
	UEFReverseMouseYSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.ReverseMouseY"));
		DisplayName = NSLOCTEXT("Settings", "ReverseMouseY", "Reverse Mouse Y-Axis");
		ConfigCategory = TEXT("Controls");
		DefaultValue = false;
		Value = false;
	}
	
	virtual void Apply_Implementation() override
	{
		// With Enhanced Input, Y-axis inversion is best handled by toggling a Negate modifier
		// on the Look action's Y component. The game's input configuration should query this
		// setting value and apply the negate modifier accordingly.
		// 
		// This can be done by:
		//   1. Having a custom UInputModifier subclass that reads this setting
		//   2. Or querying this value in your PlayerController and applying the sign
		UE_LOG(LogTemp, Log, TEXT("Applied Reverse Mouse Y: %s"), Value ? TEXT("Enabled") : TEXT("Disabled"));
	}
};

// Mouse Smoothing Setting
UCLASS(Blueprintable, DisplayName = "Extended Mouse Smoothing")
class UNREALEXTENDEDFRAMEWORK_API UEFMouseSmoothingSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()
	
public:
	UEFMouseSmoothingSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.MouseSmoothing"));
		DisplayName = NSLOCTEXT("Settings", "MouseSmoothing", "Mouse Smoothing");
		ConfigCategory = TEXT("Controls");
		DefaultValue = true;
		Value = true;
	}
	
	virtual void Apply_Implementation() override
	{
		// Mouse smoothing with Enhanced Input is typically handled by a Smooth modifier
		// on the relevant input action. The game should query this setting to toggle that modifier.
		UE_LOG(LogTemp, Log, TEXT("Applied Mouse Smoothing: %s"), Value ? TEXT("Enabled") : TEXT("Disabled"));
	}
};

// Mouse Acceleration Setting
UCLASS(Blueprintable, DisplayName = "Extended Mouse Acceleration")
class UNREALEXTENDEDFRAMEWORK_API UEFMouseAccelerationSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()
	
public:
	UEFMouseAccelerationSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.MouseAcceleration"));
		DisplayName = NSLOCTEXT("Settings", "MouseAcceleration", "Mouse Acceleration");
		ConfigCategory = TEXT("Controls");
		DefaultValue = false;
		Value = false;
	}
	
	virtual void Apply_Implementation() override
	{
		// Typically handled by OS or by a custom Enhanced Input modifier
		UE_LOG(LogTemp, Log, TEXT("Applied Mouse Acceleration: %s"), Value ? TEXT("Enabled") : TEXT("Disabled"));
	}
};


// Controller Sensitivity Setting — Enhanced Input approach
UCLASS(Blueprintable, DisplayName = "Extended Controller Sensitivity")
class UNREALEXTENDEDFRAMEWORK_API UEFControllerSensitivitySetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFControllerSensitivitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.ControllerSensitivity"));
		DisplayName = NSLOCTEXT("Settings", "ControllerSensitivity", "Controller Sensitivity");
		ConfigCategory = TEXT("Controls");
		DefaultValue = 1.0f;
		
		Value = 1.0f;
		Min = 0.1f;
		Max = 3.0f;
	}
	
	virtual void Apply_Implementation() override
	{
		// With Enhanced Input, controller sensitivity is best applied through a Scalar modifier
		// on the gamepad look input action. The game's input configuration should query this
		// setting value and scale the modifier accordingly.
		UE_LOG(LogTemp, Log, TEXT("Applied Controller Sensitivity: %.2f"), Value);
	}
};

// Controller Deadzone Setting
UCLASS(Blueprintable, DisplayName = "Extended Controller Deadzone")
class UNREALEXTENDEDFRAMEWORK_API UEFControllerDeadzoneSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFControllerDeadzoneSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.ControllerDeadzone"));
		DisplayName = NSLOCTEXT("Settings", "ControllerDeadzone", "Controller Deadzone");
		ConfigCategory = TEXT("Controls");
		DefaultValue = 0.2f;
		
		Value = 0.2f;
		Min = 0.0f;
		Max = 0.5f;
	}
	
	virtual void Apply_Implementation() override
	{
		// With Enhanced Input, deadzone is configured via UInputModifierDeadZone on the action.
		// The game should query this value and apply it to the deadzone modifier.
		UE_LOG(LogTemp, Log, TEXT("Applied Controller Deadzone: %.2f"), Value);
	}
};


// Controller Vibration Setting
UCLASS(Blueprintable, DisplayName = "Extended Controller Vibration")
class UNREALEXTENDEDFRAMEWORK_API UEFControllerVibrationSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()
	
public:
	UEFControllerVibrationSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.ControllerVibration"));
		DisplayName = NSLOCTEXT("Settings", "ControllerVibration", "Controller Vibration");
		ConfigCategory = TEXT("Controls");
		DefaultValue = true;
		Value = true;
	}
	
	virtual void Apply_Implementation() override
	{
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			if (APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
			{
				// Enable/disable force feedback
				PC->SetDisableHaptics(!Value);
				UE_LOG(LogTemp, Log, TEXT("Applied Controller Vibration: %s"), Value ? TEXT("Enabled") : TEXT("Disabled"));
			}
		}
	}
};

// Controller Vibration Strength Setting
UCLASS(Blueprintable, DisplayName = "Extended Controller Vibration Strength")
class UNREALEXTENDEDFRAMEWORK_API UEFControllerVibrationStrengthSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFControllerVibrationStrengthSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.ControllerVibrationStrength"));
		DisplayName = NSLOCTEXT("Settings", "ControllerVibrationStrength", "Vibration Strength");
		ConfigCategory = TEXT("Controls");
		DefaultValue = 1.0f;
		
		Value = 1.0f;
		Min = 0.0f;
		Max = 1.0f;
	}
	
	virtual void Apply_Implementation() override
	{
		// Force feedback intensity scaling
		UE_LOG(LogTemp, Log, TEXT("Applied Controller Vibration Strength: %.2f"), Value);
	}
};


// Mouse Wheel Sensitivity Setting — Enhanced Input approach
UCLASS(Blueprintable, DisplayName = "Extended Mouse Wheel Sensitivity")
class UNREALEXTENDEDFRAMEWORK_API UEFMouseWheelSensitivitySetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFMouseWheelSensitivitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.MouseWheelSensitivity"));
		DisplayName = NSLOCTEXT("Settings", "MouseWheelSensitivity", "Mouse Wheel Sensitivity");
		ConfigCategory = TEXT("Controls");
		DefaultValue = 1.0f;
		
		Value = 1.0f;
		Min = 0.1f;
		Max = 3.0f;
	}
	
	virtual void Apply_Implementation() override
	{
		// With Enhanced Input, mouse wheel sensitivity is handled via a Scalar modifier
		// on the scroll/zoom input action. The game should query this value and apply it.
		UE_LOG(LogTemp, Log, TEXT("Applied Mouse Wheel Sensitivity: %.2f"), Value);
	}
};
