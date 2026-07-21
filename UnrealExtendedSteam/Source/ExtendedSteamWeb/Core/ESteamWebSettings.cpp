// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Core/ESteamWebSettings.h"

UESteamWebSettings::UESteamWebSettings()
{
	CategoryName = TEXT("Extended Framework");
}

FName UESteamWebSettings::GetCategoryName() const
{
	return TEXT("Extended Framework");
}
