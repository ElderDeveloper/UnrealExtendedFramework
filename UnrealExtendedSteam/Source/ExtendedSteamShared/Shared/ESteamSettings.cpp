// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESteamSettings.h"

UESteamSettings::UESteamSettings()
{
	CategoryName = TEXT("Extended Framework");
}

FName UESteamSettings::GetCategoryName() const
{
	return TEXT("Extended Framework");
}
