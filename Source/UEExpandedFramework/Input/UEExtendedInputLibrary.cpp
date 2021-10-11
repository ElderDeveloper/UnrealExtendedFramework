// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedInputLibrary.h"



bool UUEExtendedInputLibrary::DoesKeyExistInActionMapping(UInputSettings* InputSettings,FKey Key ,FInputActionKeyMapping& FoundMapping)
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

bool UUEExtendedInputLibrary::DoesKeyExistInAxisMapping(UInputSettings* InputSettings, FKey Key,FInputAxisKeyMapping& FoundMapping)
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

bool UUEExtendedInputLibrary::DoesKeyBindingExist(UInputSettings* InputSettings, FKey Key)
{
	/*
	auto Action = FInputActionKeyMapping();
	if (DoesKeyExistInActionMapping(InputSettings,Key,Action))
	{
		return true;
	}
	auto Axis = FInputAxisKeyMapping();
	if (DoesKeyExistInAxisMapping(InputSettings,Key,Axis))
	{
		return true;
	}
	*/
	return false;
}

FName UUEExtendedInputLibrary::GetKeyBindingName(UInputSettings* InputSettings, FKey Key)
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
