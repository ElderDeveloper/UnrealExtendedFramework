// Fill out your copyright notice in the Description page of Project Settings.


#include "ESInputSubsystem.h"
#include "GameFramework/InputSettings.h"


bool UESInputSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return true;
}

void UESInputSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}



bool UESInputSubsystem::DoesKeyExistInActionMapping(UInputSettings* InputSettings,FKey Key ,FInputActionKeyMapping& FoundMapping)
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



bool UESInputSubsystem::DoesKeyExistInAxisMapping(UInputSettings* InputSettings, FKey Key,FInputAxisKeyMapping& FoundMapping)
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



bool UESInputSubsystem::UpdateKeyBindingAction(FInputActionKeyMapping ChangeActionMapping)
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



bool UESInputSubsystem::AddKeyBindingAction(FInputActionKeyMapping ChangeActionMapping)
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



bool UESInputSubsystem::RemoveKeyBindingAction(FInputActionKeyMapping ChangeActionMapping)
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



bool UESInputSubsystem::UpdateKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping)
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



bool UESInputSubsystem::AddKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping)
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



bool UESInputSubsystem::RemoveKeyBindingAxis(FInputAxisKeyMapping ChangeAxisMapping)
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



bool UESInputSubsystem::UpdateKeyBindingActionForAll(FInputActionKeyMapping ChangeActionMapping , FKey OldKey)
{
	if(const auto Settings = GetMutableDefault<UInputSettings>())
	{

		for(const auto key : Settings->GetActionMappings())
		{
			if (key.Key ==  ChangeActionMapping.Key)
			{
				FInputActionKeyMapping Save = key;
				Save.Key = OldKey;
				Settings->RemoveActionMapping(key);
				Settings->AddActionMapping(Save);
				Settings->SaveKeyMappings();
				return true;
			}
		}
		
		for(const auto key : Settings->GetActionMappings())
		{
			if (key.ActionName == ChangeActionMapping.ActionName)
			{
				OldKey = key.Key;
				Settings->RemoveActionMapping(key);
				Settings->AddActionMapping(ChangeActionMapping);
				Settings->SaveKeyMappings();
				break;
			}
		}



		
	}
	return false;
}



bool UESInputSubsystem::UpdateKeyBindingAxisForAll(FInputAxisKeyMapping ChangeAxisMapping , FKey OldKey)
{
	if(const auto Settings = GetMutableDefault<UInputSettings>())
	{
		for(const auto key : Settings->GetAxisMappings())
		{
			if (key.Key ==  ChangeAxisMapping.Key)
			{
				FInputAxisKeyMapping Save = key;
				Save.Key = OldKey;
				Settings->RemoveAxisMapping(key);
				Settings->AddAxisMapping(Save);
				Settings->SaveKeyMappings();
				break;
			}
		}
		
		for(const auto key : Settings->GetAxisMappings())
		{
			if (key.AxisName == ChangeAxisMapping.AxisName)
			{
				OldKey = key.Key;
				Settings->RemoveAxisMapping(key);
				Settings->AddAxisMapping(ChangeAxisMapping);
				Settings->SaveKeyMappings();
				return true;
			}
		}
	}
	return false;
}



bool UESInputSubsystem::UpdateKeyBindingForAll(FKey NewKey, FKey OldKey, FName BindingName)
{
	UpdateKeyBindingActionForAll(FInputActionKeyMapping(BindingName,NewKey) , OldKey);
	UpdateKeyBindingAxisForAll(FInputAxisKeyMapping(BindingName,NewKey) , OldKey);
	return true;
}
