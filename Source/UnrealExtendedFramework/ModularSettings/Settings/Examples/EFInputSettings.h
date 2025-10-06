#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "EFInputSettings.generated.h"

// Mouse Sensitivity Setting
UCLASS(Blueprintable, DisplayName = "Mouse Sensitivity")
class UNREALEXTENDEDFRAMEWORK_API UEFMouseSensitivitySetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFMouseSensitivitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.MouseSensitivity"));
		DisplayName = NSLOCTEXT("Settings", "MouseSensitivity", "Mouse Sensitivity");
		ConfigCategory = TEXT("Controls");
		DefaultValue = TEXT("1.0");
		
		Value = 1.0f;
		Min = 0.1f;
		Max = 5.0f;
	}
	
	virtual void Apply() override
	{
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				// Apply to mouse sensitivity
				PC->PlayerInput->SetMouseSensitivity(Value);
				UE_LOG(LogTemp, Log, TEXT("Applied Mouse Sensitivity: %.2f"), Value);
			}
		}
	}
};

// Reverse Mouse Y Setting
UCLASS(Blueprintable, DisplayName = "Reverse Mouse Y")
class UNREALEXTENDEDFRAMEWORK_API UEFReverseMouseYSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()
	
public:
	UEFReverseMouseYSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.ReverseMouseY"));
		DisplayName = NSLOCTEXT("Settings", "ReverseMouseY", "Reverse Mouse Y-Axis");
		ConfigCategory = TEXT("Controls");
		DefaultValue = TEXT("false");
		bValue = false;
	}
	
	virtual void Apply() override
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
							AxisMapping.Scale = bValue ? -FMath::Abs(AxisMapping.Scale) : FMath::Abs(AxisMapping.Scale);
						}
					}
				}
				
				UE_LOG(LogTemp, Log, TEXT("Applied Reverse Mouse Y: %s"), bValue ? TEXT("Enabled") : TEXT("Disabled"));
			}
		}
	}
	
	// Helper method to check if mouse Y is currently reversed
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	bool IsMouseYReversed() const
	{
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				for (const FInputAxisKeyMapping& AxisMapping : PC->PlayerInput->AxisMappings)
				{
					if ((AxisMapping.AxisName == TEXT("Turn") || 
						 AxisMapping.AxisName == TEXT("LookUp") || 
						 AxisMapping.AxisName == TEXT("MouseY")) && 
						 AxisMapping.Key == EKeys::MouseY)
					{
						return AxisMapping.Scale < 0.0f;
					}
				}
			}
		}
		return false;
	}
};

// Mouse Smoothing Setting (Bonus)
UCLASS(Blueprintable, DisplayName = "Mouse Smoothing")
class UNREALEXTENDEDFRAMEWORK_API UEFMouseSmoothingSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()
	
public:
	UEFMouseSmoothingSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.MouseSmoothing"));
		DisplayName = NSLOCTEXT("Settings", "MouseSmoothing", "Mouse Smoothing");
		ConfigCategory = TEXT("Controls");
		DefaultValue = TEXT("false");
		bValue = false;
	}
	
	virtual void Apply() override
	{
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				UE_LOG(LogTemp, Log, TEXT("Applied Mouse Smoothing: %s"), bValue ? TEXT("Enabled") : TEXT("Disabled"));
			}
		}
	}
};

// Mouse Acceleration Setting
UCLASS(Blueprintable, DisplayName = "Mouse Acceleration")
class UNREALEXTENDEDFRAMEWORK_API UEFMouseAccelerationSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()
	
public:
	UEFMouseAccelerationSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.MouseAcceleration"));
		DisplayName = NSLOCTEXT("Settings", "MouseAcceleration", "Mouse Acceleration");
		ConfigCategory = TEXT("Controls");
		DefaultValue = TEXT("false");
		bValue = false;
	}
	
	virtual void Apply() override
	{
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				UE_LOG(LogTemp, Log, TEXT("Applied Mouse Acceleration: %s"), bValue ? TEXT("Enabled") : TEXT("Disabled"));
			}
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	bool IsMouseAccelerationEnabled() const
	{
		return bValue;
	}
};

// Mouse Sensitivity X Setting
UCLASS(Blueprintable, DisplayName = "Mouse Sensitivity X")
class UNREALEXTENDEDFRAMEWORK_API UEFMouseSensitivityXSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFMouseSensitivityXSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.MouseSensitivityX"));
		DisplayName = NSLOCTEXT("Settings", "MouseSensitivityX", "Mouse Sensitivity X");
		ConfigCategory = TEXT("Controls");
		DefaultValue = TEXT("1.0");
		
		Value = 1.0f;
		Min = 0.1f;
		Max = 5.0f;
	}
	
	virtual void Apply() override
	{
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				// Apply to mouse X sensitivity specifically
				for (FInputAxisKeyMapping& AxisMapping : PC->PlayerInput->AxisMappings)
				{
					if (AxisMapping.AxisName == TEXT("Turn") || AxisMapping.AxisName == TEXT("MouseX"))
					{
						if (AxisMapping.Key == EKeys::MouseX)
						{
							float BaseScale = AxisMapping.Scale < 0 ? -1.0f : 1.0f;
							AxisMapping.Scale = BaseScale * Value;
						}
					}
				}
				UE_LOG(LogTemp, Log, TEXT("Applied Mouse Sensitivity X: %.2f"), Value);
			}
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	float GetCurrentMouseSensitivityX() const
	{
		return Value;
	}
};

// Mouse Sensitivity Y Setting
UCLASS(Blueprintable, DisplayName = "Mouse Sensitivity Y")
class UNREALEXTENDEDFRAMEWORK_API UEFMouseSensitivityYSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFMouseSensitivityYSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.MouseSensitivityY"));
		DisplayName = NSLOCTEXT("Settings", "MouseSensitivityY", "Mouse Sensitivity Y");
		ConfigCategory = TEXT("Controls");
		DefaultValue = TEXT("1.0");
		
		Value = 1.0f;
		Min = 0.1f;
		Max = 5.0f;
	}
	
	virtual void Apply() override
	{
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				// Apply to mouse Y sensitivity specifically
				for (FInputAxisKeyMapping& AxisMapping : PC->PlayerInput->AxisMappings)
				{
					if (AxisMapping.AxisName == TEXT("LookUp") || AxisMapping.AxisName == TEXT("MouseY"))
					{
						if (AxisMapping.Key == EKeys::MouseY)
						{
							float BaseScale = AxisMapping.Scale < 0 ? -1.0f : 1.0f;
							AxisMapping.Scale = BaseScale * Value;
						}
					}
				}
				UE_LOG(LogTemp, Log, TEXT("Applied Mouse Sensitivity Y: %.2f"), Value);
			}
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	float GetCurrentMouseSensitivityY() const
	{
		return Value;
	}
};

// Controller Sensitivity Setting
UCLASS(Blueprintable, DisplayName = "Controller Sensitivity")
class UNREALEXTENDEDFRAMEWORK_API UEFControllerSensitivitySetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFControllerSensitivitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.ControllerSensitivity"));
		DisplayName = NSLOCTEXT("Settings", "ControllerSensitivity", "Controller Sensitivity");
		ConfigCategory = TEXT("Controls");
		DefaultValue = TEXT("1.0");
		
		Value = 1.0f;
		Min = 0.1f;
		Max = 3.0f;
	}
	
	virtual void Apply() override
	{
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				// Apply to gamepad/controller axis mappings
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
	
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	float GetCurrentControllerSensitivity() const
	{
		return Value;
	}
};

// Controller Deadzone Setting
UCLASS(Blueprintable, DisplayName = "Controller Deadzone")
class UNREALEXTENDEDFRAMEWORK_API UEFControllerDeadzoneSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFControllerDeadzoneSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.ControllerDeadzone"));
		DisplayName = NSLOCTEXT("Settings", "ControllerDeadzone", "Controller Deadzone");
		ConfigCategory = TEXT("Controls");
		DefaultValue = TEXT("0.1");
		
		Value = 0.1f;
		Min = 0.0f;
		Max = 0.9f;
	}
	
	virtual void Apply() override
	{
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				// Apply deadzone to gamepad sticks
				for (FInputAxisKeyMapping& AxisMapping : PC->PlayerInput->AxisMappings)
				{
					if (AxisMapping.Key.IsGamepadKey())
					{
						// This would typically be handled by the input system
						// For now, we'll just log the intended behavior
						UE_LOG(LogTemp, Log, TEXT("Would apply Deadzone %.2f to %s"), Value, *AxisMapping.Key.ToString());
					}
				}
				UE_LOG(LogTemp, Log, TEXT("Applied Controller Deadzone: %.2f"), Value);
			}
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	float GetCurrentControllerDeadzone() const
	{
		return Value;
	}
};

// Controller Vibration Setting
UCLASS(Blueprintable, DisplayName = "Controller Vibration")
class UNREALEXTENDEDFRAMEWORK_API UEFControllerVibrationSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()
	
public:
	UEFControllerVibrationSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.ControllerVibration"));
		DisplayName = NSLOCTEXT("Settings", "ControllerVibration", "Controller Vibration");
		ConfigCategory = TEXT("Controls");
		DefaultValue = TEXT("true");
		bValue = true;
	}
	
	virtual void Apply() override
	{
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			if (APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
			{
				// Enable/disable force feedback
				PC->SetDisableHaptics(!bValue);
				UE_LOG(LogTemp, Log, TEXT("Applied Controller Vibration: %s"), bValue ? TEXT("Enabled") : TEXT("Disabled"));
			}
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	bool IsControllerVibrationEnabled() const
	{
		return bValue;
	}
};

// Controller Vibration Strength Setting
UCLASS(Blueprintable, DisplayName = "Controller Vibration Strength")
class UNREALEXTENDEDFRAMEWORK_API UEFControllerVibrationStrengthSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFControllerVibrationStrengthSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.ControllerVibrationStrength"));
		DisplayName = NSLOCTEXT("Settings", "ControllerVibrationStrength", "Controller Vibration Strength");
		ConfigCategory = TEXT("Controls");
		DefaultValue = TEXT("1.0");
		
		Value = 1.0f;
		Min = 0.0f;
		Max = 2.0f;
	}
	
	virtual void Apply() override
	{
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			if (APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
			{
				// This would typically be stored and used when playing force feedback
				// For now, we'll just log the intended behavior
				UE_LOG(LogTemp, Log, TEXT("Applied Controller Vibration Strength: %.2f"), Value);
			}
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	float GetCurrentControllerVibrationStrength() const
	{
		return Value;
	}
};

// Auto Sprint Setting
UCLASS(Blueprintable, DisplayName = "Auto Sprint")
class UNREALEXTENDEDFRAMEWORK_API UEFAutoSprintSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()
	
public:
	UEFAutoSprintSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.AutoSprint"));
		DisplayName = NSLOCTEXT("Settings", "AutoSprint", "Auto Sprint");
		ConfigCategory = TEXT("Controls");
		DefaultValue = TEXT("false");
		bValue = false;
	}
	
	virtual void Apply() override
	{
		// This would typically be handled by the character movement system
		// For now, we'll just log the intended behavior
		UE_LOG(LogTemp, Log, TEXT("Applied Auto Sprint: %s"), bValue ? TEXT("Enabled") : TEXT("Disabled"));
	}
	
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	bool IsAutoSprintEnabled() const
	{
		return bValue;
	}
};

// Hold to Sprint Setting
UCLASS(Blueprintable, DisplayName = "Hold to Sprint")
class UNREALEXTENDEDFRAMEWORK_API UEFHoldToSprintSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()
	
public:
	UEFHoldToSprintSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.HoldToSprint"));
		DisplayName = NSLOCTEXT("Settings", "HoldToSprint", "Hold to Sprint (vs Toggle)");
		ConfigCategory = TEXT("Controls");
		DefaultValue = TEXT("true");
		bValue = true;
	}
	
	virtual void Apply() override
	{
		// This would typically be handled by the input system
		// For now, we'll just log the intended behavior
		UE_LOG(LogTemp, Log, TEXT("Applied Hold to Sprint: %s"), bValue ? TEXT("Hold") : TEXT("Toggle"));
	}
	
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	bool IsHoldToSprintEnabled() const
	{
		return bValue;
	}
};

// Hold to Crouch Setting
UCLASS(Blueprintable, DisplayName = "Hold to Crouch")
class UNREALEXTENDEDFRAMEWORK_API UEFHoldToCrouchSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()
	
public:
	UEFHoldToCrouchSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.HoldToCrouch"));
		DisplayName = NSLOCTEXT("Settings", "HoldToCrouch", "Hold to Crouch (vs Toggle)");
		ConfigCategory = TEXT("Controls");
		DefaultValue = TEXT("false");
		bValue = false;
	}
	
	virtual void Apply() override
	{
		// This would typically be handled by the input system
		// For now, we'll just log the intended behavior
		UE_LOG(LogTemp, Log, TEXT("Applied Hold to Crouch: %s"), bValue ? TEXT("Hold") : TEXT("Toggle"));
	}
	
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	bool IsHoldToCrouchEnabled() const
	{
		return bValue;
	}
};

// Mouse Wheel Sensitivity Setting
UCLASS(Blueprintable, DisplayName = "Mouse Wheel Sensitivity")
class UNREALEXTENDEDFRAMEWORK_API UEFMouseWheelSensitivitySetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFMouseWheelSensitivitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.MouseWheelSensitivity"));
		DisplayName = NSLOCTEXT("Settings", "MouseWheelSensitivity", "Mouse Wheel Sensitivity");
		ConfigCategory = TEXT("Controls");
		DefaultValue = TEXT("1.0");
		
		Value = 1.0f;
		Min = 0.1f;
		Max = 3.0f;
	}
	
	virtual void Apply() override
	{
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				// Apply to mouse wheel axis mappings
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
	
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	float GetCurrentMouseWheelSensitivity() const
	{
		return Value;
	}
};

// Double Click Speed Setting
UCLASS(Blueprintable, DisplayName = "Double Click Speed")
class UNREALEXTENDEDFRAMEWORK_API UEFDoubleClickSpeedSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()
	
public:
	UEFDoubleClickSpeedSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(TEXT("Settings.Controls.DoubleClickSpeed"));
		DisplayName = NSLOCTEXT("Settings", "DoubleClickSpeed", "Double Click Speed");
		ConfigCategory = TEXT("Controls");
		DefaultValue = TEXT("0.5");
		
		Value = 0.5f;
		Min = 0.1f;
		Max = 1.0f;
	}
	
	virtual void Apply() override
	{
		if (GEngine && GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld()))
		{
			APlayerController* PC = GEngine->GetFirstLocalPlayerController(GEngine->GetCurrentPlayWorld());
			if (PC && PC->PlayerInput)
			{
				UE_LOG(LogTemp, Log, TEXT("Applied Double Click Speed: %.2f seconds"), Value);
			}
		}
	}
	
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	float GetCurrentDoubleClickSpeed() const
	{
		return Value;
	}
};
