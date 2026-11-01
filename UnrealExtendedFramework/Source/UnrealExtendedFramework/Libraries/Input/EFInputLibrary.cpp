// Copyright Moon Punch Games. All Rights Reserved.


#include "EFInputLibrary.h"
#include "HAL/PlatformApplicationMisc.h"


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
