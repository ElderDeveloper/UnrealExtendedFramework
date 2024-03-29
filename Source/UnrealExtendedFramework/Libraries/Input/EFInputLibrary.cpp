﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "EFInputLibrary.h"



bool UEFInputLibrary::DoesKeyExistInActionMapping(UInputSettings* InputSettings,FKey Key ,FInputActionKeyMapping& FoundMapping)
{
	if (InputSettings)
	{
		TArray<FName> ActionNames;
		InputSettings->GetActionNames(ActionNames);

		if (ActionNames.IsValidIndex(0))
		{
			for (const auto i : ActionNames)
			{
				TArray<FInputActionKeyMapping> AllActionMappings;
				InputSettings->GetActionMappingByName(i,AllActionMappings);

				for (const auto j : AllActionMappings)
				{
					if (j.Key == Key)
					{
						FoundMapping = j;
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool UEFInputLibrary::DoesKeyExistInAxisMapping(UInputSettings* InputSettings, FKey Key,FInputAxisKeyMapping& FoundMapping)
{
	if (InputSettings)
	{
		TArray<FName> ActionNames;
		InputSettings->GetActionNames(ActionNames);

		if (ActionNames.IsValidIndex(0))
		{
			for (const auto i : ActionNames)
			{
				TArray<FInputAxisKeyMapping> AxisKeyMappings;
				InputSettings->GetAxisMappingByName(i,AxisKeyMappings);

				for (const auto j : AxisKeyMappings)
				{
					if (j.Key == Key)
					{
						FoundMapping = j;
						return true;
					}
				}
			}
		}
	}
	return false;
}


FName UEFInputLibrary::GetKeyBindingName(UInputSettings* InputSettings, FKey Key)
{
	FInputActionKeyMapping ActionKeyMapping = FInputActionKeyMapping();
	if (DoesKeyExistInActionMapping(InputSettings,Key,ActionKeyMapping))
	{
		return ActionKeyMapping.ActionName;
	}
	FInputAxisKeyMapping AxisKeyMapping = FInputAxisKeyMapping();
	if (DoesKeyExistInAxisMapping(InputSettings,Key,AxisKeyMapping))
	{
		return AxisKeyMapping.AxisName;
	}
	return FName();
}

bool UEFInputLibrary::UpdateKeyBindingAction(FInputActionKeyMapping ChangeActionMapping)
{
	if(const auto Settings = GetMutableDefault<UInputSettings>())
	{
		for(const auto key : Settings->GetActionMappings())
		{
			if (key.ActionName == ChangeActionMapping.ActionName)
			{
				Settings->RemoveActionMapping(key);
				Settings->AddActionMapping(ChangeActionMapping);
				Settings->SaveKeyMappings();
				return true;
			}
		}
	}
	return false;
}

bool UEFInputLibrary::AddKeyBindingAction(FInputActionKeyMapping ChangeActionMapping)
{
	if(const auto Settings = GetMutableDefault<UInputSettings>())
	{
		for(const auto key : Settings->GetActionMappings())
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
	if(const auto Settings = GetMutableDefault<UInputSettings>())
	{
		for(const auto key : Settings->GetActionMappings())
		{
			if (key.ActionName == ChangeActionMapping.ActionName)
			{
				Settings->RemoveActionMapping(key);
				Settings->SaveKeyMappings();
				return true;
			}
		}
	}
	return false;
}




bool UEFInputLibrary::UpdateKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping)
{
	if(const auto Settings = GetMutableDefault<UInputSettings>())
	{
		for(const auto key : Settings->GetAxisMappings())
		{
			if (key.AxisName == ChangeAxisMapping.AxisName)
			{
				Settings->RemoveAxisMapping(key);
				Settings->AddAxisMapping(ChangeAxisMapping);
				Settings->SaveKeyMappings();
				return true;
			}
		}
	}
	return false;
}

bool UEFInputLibrary::AddKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping)
{
	if(const auto Settings = GetMutableDefault<UInputSettings>())
	{
		for(const auto key : Settings->GetAxisMappings())
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
	if(const auto Settings = GetMutableDefault<UInputSettings>())
	{
		for(const auto key : Settings->GetAxisMappings())
		{
			if (key.AxisName == ChangeAxisMapping.AxisName)
			{
				Settings->RemoveAxisMapping(key);
				Settings->SaveKeyMappings();
				return true;
			}
		}
	}
	return false;
}