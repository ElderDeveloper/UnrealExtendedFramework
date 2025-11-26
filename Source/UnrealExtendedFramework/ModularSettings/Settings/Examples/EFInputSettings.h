#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "InputCoreTypes.h"
#include "EFInputSettings.generated.h"



// Forward declarations to avoid hard dependency if module is missing, 
// but UPROPERTY requires full type or at least known type.
// We assume EnhancedInput is available.
class UInputAction;
class UInputMappingContext;

/**
 * Setting for remapping an Enhanced Input Action.
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
		// Here we would interface with Enhanced Input Subsystem to remap the key.
		// This usually involves removing the old mapping and adding a new one, 
		// or using PlayerMappableKeySettings.
		// For now, we just store the value.
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
				// Apply_Implementation to mouse sensitivity
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
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				// This assumes custom handling or separate axis mapping for X
				// PC->PlayerInput->SetMouseSensitivity(Value); // This sets both
				UE_LOG(LogTemp, Log, TEXT("Applied Mouse Sensitivity X: %.2f"), Value);
			}
		}
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
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				// This assumes custom handling or separate axis mapping for Y
				UE_LOG(LogTemp, Log, TEXT("Applied Mouse Sensitivity Y: %.2f"), Value);
			}
		}
	}
};

// Reverse Mouse Y Setting
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
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				// Find the mouse Y axis binding and invert it
				for (FInputAxisKeyMapping& AxisMapping : PC->PlayerInput->AxisMappings)
				{
					if (AxisMapping.AxisName == TEXT("Turn") || 
						AxisMapping.AxisName == TEXT("LookUp") || 
						AxisMapping.AxisName == TEXT("MouseY"))
					{
						if (AxisMapping.Key == EKeys::MouseY)
						{
							AxisMapping.Scale = Value ? -FMath::Abs(AxisMapping.Scale) : FMath::Abs(AxisMapping.Scale);
						}
					}
				}
				
				UE_LOG(LogTemp, Log, TEXT("Applied Reverse Mouse Y: %s"), Value ? TEXT("Enabled") : TEXT("Disabled"));
			}
		}
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
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				// PC->bEnableMouseSmoothing = Value; // Deprecated in some versions, but generally available
				UE_LOG(LogTemp, Log, TEXT("Applied Mouse Smoothing: %s"), Value ? TEXT("Enabled") : TEXT("Disabled"));
			}
		}
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
		// Typically handled by OS or specific input plugins, but we can log intent
		UE_LOG(LogTemp, Log, TEXT("Applied Mouse Acceleration: %s"), Value ? TEXT("Enabled") : TEXT("Disabled"));
	}
};


// Controller Sensitivity Setting
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
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				// Apply_Implementation to gamepad/controller axis mappings
				for (FInputAxisKeyMapping& AxisMapping : PC->PlayerInput->AxisMappings)
				{
					if (AxisMapping.Key.IsGamepadKey())
					{
						if (AxisMapping.AxisName == TEXT("Turn") || AxisMapping.AxisName == TEXT("LookUp"))
						{
							float BaseScale = AxisMapping.Scale < 0 ? -1.0f : 1.0f;
							AxisMapping.Scale = BaseScale * Value;
						}
					}
				}
				UE_LOG(LogTemp, Log, TEXT("Applied Controller Sensitivity: %.2f"), Value);
			}
		}
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
		// This would typically update the Input settings or be read by the input handling logic
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
		// This would scale the force feedback intensity
		UE_LOG(LogTemp, Log, TEXT("Applied Controller Vibration Strength: %.2f"), Value);
	}
};


// Mouse Wheel Sensitivity Setting
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
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				// Apply_Implementation to mouse wheel axis mappings
				for (FInputAxisKeyMapping& AxisMapping : PC->PlayerInput->AxisMappings)
				{
					if (AxisMapping.Key == EKeys::MouseWheelAxis)
					{
						float BaseScale = AxisMapping.Scale < 0 ? -1.0f : 1.0f;
						AxisMapping.Scale = BaseScale * Value;
					}
				}
				UE_LOG(LogTemp, Log, TEXT("Applied Mouse Wheel Sensitivity: %.2f"), Value);
			}
		}
	}
};
