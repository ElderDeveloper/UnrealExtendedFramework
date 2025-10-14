#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "EFInputSettings.generated.h"

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
	
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	float GetCurrentControllerSensitivity() const
	{
		return Value;
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
	
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	bool IsControllerVibrationEnabled() const
	{
		return Value;
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
	
	UFUNCTION(BlueprintCallable, Category = "Input Settings")
	float GetCurrentMouseWheelSensitivity() const
	{
		return Value;
	}
};
