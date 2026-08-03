// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianUserCache.h"

#include "ExtendedAtlassianLog.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

namespace ExtendedAtlassianUserCachePrivate
{
	const TCHAR* Section = TEXT("VerifiedUser");
	const TCHAR* SiteUrlKey = TEXT("SiteUrl");
	const TCHAR* CredentialEmailKey = TEXT("CredentialEmail");
	const TCHAR* AccountIdKey = TEXT("AccountId");
	const TCHAR* DisplayNameKey = TEXT("DisplayName");
	const TCHAR* EmailAddressKey = TEXT("EmailAddress");
	const TCHAR* AvatarUrlKey = TEXT("AvatarUrl");
	const TCHAR* InitialsKey = TEXT("Initials");
	const TCHAR* AvatarBackgroundKey = TEXT("AvatarBackground");
	const TCHAR* AvatarForegroundKey = TEXT("AvatarForeground");
}

FString FExtendedAtlassianUserCache::GetStorePath()
{
	FString Root = FPlatformProcess::UserSettingsDir();
	if (Root.IsEmpty())
	{
		Root = FPlatformProcess::UserDir();
	}

	return FPaths::Combine(Root, TEXT("UnrealExtendedAtlassian"), TEXT("verified-user.ini"));
}

bool FExtendedAtlassianUserCache::Load(
	const FString& SiteUrl,
	const FString& CredentialEmail,
	FExtendedAtlassianUser& OutUser)
{
	return LoadFrom(GetStorePath(), SiteUrl, CredentialEmail, OutUser);
}

bool FExtendedAtlassianUserCache::Save(
	const FString& SiteUrl,
	const FString& CredentialEmail,
	const FExtendedAtlassianUser& User)
{
	return SaveTo(GetStorePath(), SiteUrl, CredentialEmail, User);
}

bool FExtendedAtlassianUserCache::LoadFrom(
	const FString& Path,
	const FString& SiteUrl,
	const FString& CredentialEmail,
	FExtendedAtlassianUser& OutUser)
{
	using namespace ExtendedAtlassianUserCachePrivate;

	OutUser.Reset();
	if (SiteUrl.IsEmpty()
		|| CredentialEmail.IsEmpty()
		|| !IFileManager::Get().FileExists(*Path))
	{
		return false;
	}

	FConfigFile Config;
	Config.Read(Path);

	FString StoredSiteUrl;
	FString StoredCredentialEmail;
	if (!Config.GetString(Section, SiteUrlKey, StoredSiteUrl)
		|| !Config.GetString(Section, CredentialEmailKey, StoredCredentialEmail)
		|| !StoredSiteUrl.Equals(SiteUrl, ESearchCase::IgnoreCase)
		|| !StoredCredentialEmail.Equals(CredentialEmail, ESearchCase::IgnoreCase))
	{
		return false;
	}

	Config.GetString(Section, AccountIdKey, OutUser.AccountId);
	Config.GetString(Section, DisplayNameKey, OutUser.DisplayName);
	Config.GetString(Section, EmailAddressKey, OutUser.EmailAddress);
	Config.GetString(Section, AvatarUrlKey, OutUser.AvatarUrl);
	Config.GetString(Section, InitialsKey, OutUser.Initials);
	Config.GetString(Section, AvatarBackgroundKey, OutUser.AvatarBackground);
	Config.GetString(Section, AvatarForegroundKey, OutUser.AvatarForeground);

	if (!OutUser.IsValid())
	{
		OutUser.Reset();
		return false;
	}
	return true;
}

bool FExtendedAtlassianUserCache::SaveTo(
	const FString& Path,
	const FString& SiteUrl,
	const FString& CredentialEmail,
	const FExtendedAtlassianUser& User)
{
	using namespace ExtendedAtlassianUserCachePrivate;

	if (SiteUrl.IsEmpty() || CredentialEmail.IsEmpty() || !User.IsValid())
	{
		return false;
	}

	const FString Directory = FPaths::GetPath(Path);
	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		UE_LOG(
			LogExtendedAtlassian,
			Warning,
			TEXT("Could not create verified-user cache directory %s."),
			*Directory);
		return false;
	}

	FConfigFile Config;
	Config.SetString(Section, SiteUrlKey, *SiteUrl);
	Config.SetString(Section, CredentialEmailKey, *CredentialEmail);
	Config.SetString(Section, AccountIdKey, *User.AccountId);
	Config.SetString(Section, DisplayNameKey, *User.DisplayName);
	Config.SetString(Section, EmailAddressKey, *User.EmailAddress);
	Config.SetString(Section, AvatarUrlKey, *User.AvatarUrl);
	Config.SetString(Section, InitialsKey, *User.Initials);
	Config.SetString(Section, AvatarBackgroundKey, *User.AvatarBackground);
	Config.SetString(Section, AvatarForegroundKey, *User.AvatarForeground);
	Config.Dirty = true;
	return Config.Write(Path);
}

void FExtendedAtlassianUserCache::Clear()
{
	const FString Path = GetStorePath();
	if (IFileManager::Get().FileExists(*Path))
	{
		IFileManager::Get().Delete(*Path, false, true);
	}
}
