// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Publish/ESteamPublishSettings.h"

#include "ExtendedSteamEditor.h"
#include "Publish/ESteamPublishCredentials.h"
#include "Publish/ESteamPublishManager.h"
#include "Misc/Paths.h"

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

		// Mirror the stored credentials (user name file + OS credential store) into the transient UI
		// fields so they survive editor restarts. Load also migrates a password left in the file by
		// older versions of the plugin into the credential store.
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
		FESteamPublishManager::Notify(FString::Printf(TEXT("Steam user name saved to %s; password saved to %s."),
			*FESteamPublishCredentials::GetCredentialsFilePath(), *FESteamPublishCredentials::GetPasswordStorageDescription()), true);
		if (!FESteamPublishCredentials::IsPasswordProtectedAtRest())
		{
			FESteamPublishManager::Notify(TEXT("On this platform the password file is readable only by your user account but is not encrypted."), true);
		}
		FESteamPublishManager::Notify(TEXT("Reminder: run 'Secure Credentials for Version Control' so the Config/SteamPublish folder is never committed."), true);
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
