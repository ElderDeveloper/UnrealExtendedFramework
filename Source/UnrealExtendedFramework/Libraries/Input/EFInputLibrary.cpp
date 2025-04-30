// Fill out your copyright notice in the Description page of Project Settings.


#include "EFInputLibrary.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

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



bool UEFInputLibrary::CopyStringToClipboard(const FString& TextToCopy)
{
#if PLATFORM_WINDOWS
	if (!OpenClipboard(nullptr))
		return false;
	    
	EmptyClipboard();
	    
	// Convert FString to wide string and calculate size
	const TCHAR* WideStr = *TextToCopy;
	SIZE_T WideStrLen = TextToCopy.Len() + 1; // +1 for null terminator
	SIZE_T BufferSize = WideStrLen * sizeof(TCHAR);
	    
	// Allocate global memory
	HGLOBAL HGlobal = GlobalAlloc(GMEM_MOVEABLE, BufferSize);
	if (!HGlobal)
	{
		CloseClipboard();
		return false;
	}
	    
	// Lock the memory and copy the text
	void* Buffer = GlobalLock(HGlobal);
	if (!Buffer)
	{
		GlobalFree(HGlobal);
		CloseClipboard();
		return false;
	}
	    
	FMemory::Memcpy(Buffer, WideStr, BufferSize);
	GlobalUnlock(HGlobal);
	    
	// Set the clipboard data (using Unicode for wide strings)
	UINT Format = CF_UNICODETEXT;
	HANDLE Result = SetClipboardData(Format, HGlobal);
	    
	CloseClipboard();
	return Result != nullptr;
#else
	// Not implemented for other platforms
	return false;
#endif
}


FString UEFInputLibrary::GetStringFromClipboard()
{
	FString ClipboardContent;
    
#if PLATFORM_WINDOWS
	if (!OpenClipboard(nullptr))
		return ClipboardContent;
	    
	// Try to get Unicode text from clipboard
	HANDLE HGlobal = GetClipboardData(CF_UNICODETEXT);
	    
	if (HGlobal)
	{
		const WCHAR* Text = static_cast<const WCHAR*>(GlobalLock(HGlobal));
		if (Text)
		{
			ClipboardContent = FString(Text);
			GlobalUnlock(HGlobal);
		}
	}
	    
	CloseClipboard();
#endif
    
	return ClipboardContent;
}
