// Fill out your copyright notice in the Description page of Project Settings.


#include "EFInputLibrary.h"
#include "HAL/PlatformApplicationMisc.h"


bool UEFInputLibrary::DoesKeyExistInActionMapping(UInputSettings* InputSettings, FKey Key, FInputActionKeyMapping& FoundMapping)
{
	if (InputSettings)
	{
		TArray<FName> ActionNames;
		InputSettings->GetActionNames(ActionNames);

		for (const auto& ActionName : ActionNames)
		{
			TArray<FInputActionKeyMapping> AllActionMappings;
			InputSettings->GetActionMappingByName(ActionName, AllActionMappings);

			for (const auto& Mapping : AllActionMappings)
			{
				if (Mapping.Key == Key)
				{
					FoundMapping = Mapping;
					return true;
				}
			}
		}
	}
	return false;
}

bool UEFInputLibrary::DoesKeyExistInAxisMapping(UInputSettings* InputSettings, FKey Key, FInputAxisKeyMapping& FoundMapping)
{
	if (InputSettings)
	{
		// BUG FIX: Previously called GetActionNames() instead of GetAxisNames(),
		// which meant axis-only bindings were never found.
		TArray<FName> AxisNames;
		InputSettings->GetAxisNames(AxisNames);

		for (const auto& AxisName : AxisNames)
		{
			TArray<FInputAxisKeyMapping> AxisKeyMappings;
			InputSettings->GetAxisMappingByName(AxisName, AxisKeyMappings);

			for (const auto& Mapping : AxisKeyMappings)
			{
				if (Mapping.Key == Key)
				{
					FoundMapping = Mapping;
					return true;
				}
			}
		}
	}
	return false;
}


FName UEFInputLibrary::GetKeyBindingName(UInputSettings* InputSettings, FKey Key)
{
	FInputActionKeyMapping ActionKeyMapping;
	if (DoesKeyExistInActionMapping(InputSettings, Key, ActionKeyMapping))
	{
		return ActionKeyMapping.ActionName;
	}
	FInputAxisKeyMapping AxisKeyMapping;
	if (DoesKeyExistInAxisMapping(InputSettings, Key, AxisKeyMapping))
	{
		return AxisKeyMapping.AxisName;
	}
	return FName();
}

bool UEFInputLibrary::UpdateKeyBindingAction(FInputActionKeyMapping ChangeActionMapping)
{
	if (const auto Settings = GetMutableDefault<UInputSettings>())
	{
		// BUG FIX: Copy the matching mapping before modifying the array to avoid
		// undefined behavior from modifying during iteration.
		FInputActionKeyMapping OldMapping;
		bool bFound = false;
		for (const auto& key : Settings->GetActionMappings())
		{
			if (key.ActionName == ChangeActionMapping.ActionName)
			{
				OldMapping = key;
				bFound = true;
				break;
			}
		}

		if (bFound)
		{
			Settings->RemoveActionMapping(OldMapping);
			Settings->AddActionMapping(ChangeActionMapping);
			Settings->SaveKeyMappings();
			return true;
		}
	}
	return false;
}

bool UEFInputLibrary::AddKeyBindingAction(FInputActionKeyMapping ChangeActionMapping)
{
	if (const auto Settings = GetMutableDefault<UInputSettings>())
	{
		for (const auto& key : Settings->GetActionMappings())
		{
			if (key.ActionName == ChangeActionMapping.ActionName)
				return false;
		}
		Settings->AddActionMapping(ChangeActionMapping);
		Settings->SaveKeyMappings();
		return true;
	}
	return false;
}

bool UEFInputLibrary::RemoveKeyBindingAction(FInputActionKeyMapping ChangeActionMapping)
{
	if (const auto Settings = GetMutableDefault<UInputSettings>())
	{
		// BUG FIX: Same iteration-while-modifying fix as UpdateKeyBindingAction
		FInputActionKeyMapping OldMapping;
		bool bFound = false;
		for (const auto& key : Settings->GetActionMappings())
		{
			if (key.ActionName == ChangeActionMapping.ActionName)
			{
				OldMapping = key;
				bFound = true;
				break;
			}
		}

		if (bFound)
		{
			Settings->RemoveActionMapping(OldMapping);
			Settings->SaveKeyMappings();
			return true;
		}
	}
	return false;
}




bool UEFInputLibrary::UpdateKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping)
{
	if (const auto Settings = GetMutableDefault<UInputSettings>())
	{
		FInputAxisKeyMapping OldMapping;
		bool bFound = false;
		for (const auto& key : Settings->GetAxisMappings())
		{
			if (key.AxisName == ChangeAxisMapping.AxisName)
			{
				OldMapping = key;
				bFound = true;
				break;
			}
		}

		if (bFound)
		{
			Settings->RemoveAxisMapping(OldMapping);
			Settings->AddAxisMapping(ChangeAxisMapping);
			Settings->SaveKeyMappings();
			return true;
		}
	}
	return false;
}

bool UEFInputLibrary::AddKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping)
{
	if (const auto Settings = GetMutableDefault<UInputSettings>())
	{
		for (const auto& key : Settings->GetAxisMappings())
		{
			if (key.AxisName == ChangeAxisMapping.AxisName)
				return false;
		}
		Settings->AddAxisMapping(ChangeAxisMapping);
		Settings->SaveKeyMappings();
		return true;
	}
	return false;
}

bool UEFInputLibrary::RemoveKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping)
{
	if (const auto Settings = GetMutableDefault<UInputSettings>())
	{
		FInputAxisKeyMapping OldMapping;
		bool bFound = false;
		for (const auto& key : Settings->GetAxisMappings())
		{
			if (key.AxisName == ChangeAxisMapping.AxisName)
			{
				OldMapping = key;
				bFound = true;
				break;
			}
		}

		if (bFound)
		{
			Settings->RemoveAxisMapping(OldMapping);
			Settings->SaveKeyMappings();
			return true;
		}
	}
	return false;
}


TArray<FName> UEFInputLibrary::GetAllActionMappingNames()
{
	TArray<FName> Names;
	if (const auto Settings = GetDefault<UInputSettings>())
	{
		Settings->GetActionNames(Names);
	}
	return Names;
}


TArray<FName> UEFInputLibrary::GetAllAxisMappingNames()
{
	TArray<FName> Names;
	if (const auto Settings = GetDefault<UInputSettings>())
	{
		Settings->GetAxisNames(Names);
	}
	return Names;
}


// BUG FIX: Replaced raw Win32 API (OpenClipboard, GlobalAlloc, etc.) with
// FPlatformApplicationMisc which is cross-platform (Windows, Mac, Linux, etc.)
bool UEFInputLibrary::CopyStringToClipboard(const FString& TextToCopy)
{
	FPlatformApplicationMisc::ClipboardCopy(*TextToCopy);
	return true;
}


FString UEFInputLibrary::GetStringFromClipboard()
{
	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);
	return ClipboardContent;
}
