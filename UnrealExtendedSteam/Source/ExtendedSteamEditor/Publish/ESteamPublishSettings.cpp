// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Publish/ESteamPublishSettings.h"

#include "ExtendedSteamEditor.h"
#include "Publish/ESteamPublishCredentials.h"
#include "Publish/ESteamPublishManager.h"
#include "Misc/Paths.h"

UESteamPublishSettings::UESteamPublishSettings()
{
	CategoryName = TEXT("Extended Framework");
}

FName UESteamPublishSettings::GetCategoryName() const
{
	return TEXT("Extended Framework");
}

void UESteamPublishSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// Only touch defaults / disk on the CDO the settings editor actually shows.
	if (HasAnyFlags(RF_ClassDefaultObject) && !HasAnyFlags(RF_NeedLoad))
	{
		if (ContentBuilderDirectory.Path.IsEmpty())
		{
			ContentBuilderDirectory.Path = FESteamPublishManager::GetDefaultContentBuilderDirectory();
		}

		// Mirror the ignored credentials file into the transient UI fields so they survive editor restarts.
		FESteamPublishCredentials Creds;
		if (FESteamPublishCredentials::Load(Creds))
		{
			SteamUsername = Creds.Username;
			SteamPassword = Creds.Password;
		}
	}
}

void UESteamPublishSettings::ExtractContentBuilderTools()
{
	FESteamPublishManager::ExtractContentBuilderTools(this);
}

void UESteamPublishSettings::SecureCredentialsForVersionControl()
{
	FESteamPublishManager::SecureForVersionControl();
}

void UESteamPublishSettings::SaveCredentials()
{
	FESteamPublishCredentials Creds;
	Creds.Username = SteamUsername;
	Creds.Password = SteamPassword;

	FString Error;
	if (FESteamPublishCredentials::Save(Creds, Error))
	{
		FESteamPublishManager::Notify(FString::Printf(TEXT("Credentials saved to %s"), *FESteamPublishCredentials::GetCredentialsFilePath()), true);
		FESteamPublishManager::Notify(TEXT("Reminder: run 'Secure Credentials for Version Control' so this file is never committed."), true);
	}
	else
	{
		FESteamPublishManager::Notify(Error, false);
	}
}

void UESteamPublishSettings::LoginAndAuthorize()
{
	FESteamPublishManager::LaunchInteractiveLogin(this);
}

void UESteamPublishSettings::PublishBuild()
{
	FESteamPublishManager::PublishBuildAsync(this);
}
