// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianCredentials.h"

#include "ExtendedAtlassianLog.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Base64.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <dpapi.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace ExtendedAtlassianCredentialsPrivate
{
	const TCHAR* Section = TEXT("Atlassian");
	const TCHAR* EmailKey = TEXT("Email");
	const TCHAR* TokenKey = TEXT("Token");
	const TCHAR* EncryptedKey = TEXT("Encrypted");

#if PLATFORM_WINDOWS
	/**
	 * Application-specific entropy. Mixing this in means a DPAPI blob produced by some other
	 * application on this account cannot be dropped into our file and unwrapped by us.
	 */
	const ANSICHAR EntropySalt[] = "UnrealExtendedAtlassian.credentials.v1";

	bool EncryptToBase64(const FString& Plain, FString& OutBase64)
	{
		const FTCHARToUTF8 Utf8(*Plain);

		DATA_BLOB In;
		In.cbData = static_cast<DWORD>(Utf8.Length());
		In.pbData = reinterpret_cast<BYTE*>(const_cast<ANSICHAR*>(Utf8.Get()));

		DATA_BLOB Entropy;
		Entropy.cbData = static_cast<DWORD>(sizeof(EntropySalt) - 1);
		Entropy.pbData = reinterpret_cast<BYTE*>(const_cast<ANSICHAR*>(EntropySalt));

		DATA_BLOB Out;
		Out.cbData = 0;
		Out.pbData = nullptr;

		if (!::CryptProtectData(&In, nullptr, &Entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &Out))
		{
			UE_LOG(LogExtendedAtlassian, Warning, TEXT("CryptProtectData failed (error %u)."), ::GetLastError());
			return false;
		}

		OutBase64 = FBase64::Encode(Out.pbData, Out.cbData);
		::LocalFree(Out.pbData);
		return true;
	}

	bool DecryptFromBase64(const FString& InBase64, FString& OutPlain)
	{
		TArray<uint8> Blob;
		if (!FBase64::Decode(InBase64, Blob) || Blob.Num() == 0)
		{
			return false;
		}

		DATA_BLOB In;
		In.cbData = static_cast<DWORD>(Blob.Num());
		In.pbData = Blob.GetData();

		DATA_BLOB Entropy;
		Entropy.cbData = static_cast<DWORD>(sizeof(EntropySalt) - 1);
		Entropy.pbData = reinterpret_cast<BYTE*>(const_cast<ANSICHAR*>(EntropySalt));

		DATA_BLOB Out;
		Out.cbData = 0;
		Out.pbData = nullptr;

		if (!::CryptUnprotectData(&In, nullptr, &Entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &Out))
		{
			UE_LOG(LogExtendedAtlassian, Warning, TEXT("CryptUnprotectData failed (error %u)."), ::GetLastError());
			return false;
		}

		FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Out.pbData), static_cast<int32>(Out.cbData));
		OutPlain = FString(Converted.Length(), Converted.Get());

		// The decrypted token was briefly in process memory; do not leave a copy behind.
		FMemory::Memzero(Out.pbData, Out.cbData);
		::LocalFree(Out.pbData);
		return true;
	}
#endif // PLATFORM_WINDOWS
}

FString FExtendedAtlassianCredentialStore::GetStorePath()
{
	FString Root = FPlatformProcess::UserSettingsDir();
	if (Root.IsEmpty())
	{
		Root = FPlatformProcess::UserDir();
	}

	return FPaths::Combine(Root, TEXT("UnrealExtendedAtlassian"), TEXT("credentials.ini"));
}

bool FExtendedAtlassianCredentialStore::IsEncryptionAvailable()
{
#if PLATFORM_WINDOWS
	return true;
#else
	return false;
#endif
}

bool FExtendedAtlassianCredentialStore::HasStoredCredentials()
{
	return IFileManager::Get().FileExists(*GetStorePath());
}

bool FExtendedAtlassianCredentialStore::Load(FExtendedAtlassianCredentials& OutCredentials)
{
	return LoadFrom(GetStorePath(), OutCredentials);
}

bool FExtendedAtlassianCredentialStore::Save(const FExtendedAtlassianCredentials& InCredentials)
{
	return SaveTo(GetStorePath(), InCredentials);
}

bool FExtendedAtlassianCredentialStore::LoadFrom(const FString& Path, FExtendedAtlassianCredentials& OutCredentials)
{
	using namespace ExtendedAtlassianCredentialsPrivate;

	OutCredentials.Reset();

	if (!IFileManager::Get().FileExists(*Path))
	{
		return false;
	}

	FConfigFile Config;
	Config.Read(Path);

	FString StoredToken;
	if (!Config.GetString(Section, EmailKey, OutCredentials.Email) ||
		!Config.GetString(Section, TokenKey, StoredToken))
	{
		UE_LOG(LogExtendedAtlassian, Warning, TEXT("Credential file at %s is missing expected keys."), *Path);
		OutCredentials.Reset();
		return false;
	}

	FString EncryptedFlag;
	Config.GetString(Section, EncryptedKey, EncryptedFlag);
	const bool bStoredEncrypted = EncryptedFlag.ToBool();

	if (!bStoredEncrypted)
	{
		OutCredentials.ApiToken = StoredToken;
		return OutCredentials.IsValid();
	}

#if PLATFORM_WINDOWS
	if (!DecryptFromBase64(StoredToken, OutCredentials.ApiToken))
	{
		// Most likely the file was copied from another machine or user account, where the DPAPI key
		// does not apply. Leave it in place and let the user re-enter rather than deleting silently.
		UE_LOG(LogExtendedAtlassian, Warning,
			TEXT("Stored Atlassian token could not be decrypted. It was likely created by a different user or machine. Re-enter it in Project Settings."));
		OutCredentials.Reset();
		return false;
	}
#else
	UE_LOG(LogExtendedAtlassian, Warning, TEXT("Credential file is marked encrypted but this platform cannot decrypt it."));
	OutCredentials.Reset();
	return false;
#endif

	return OutCredentials.IsValid();
}

bool FExtendedAtlassianCredentialStore::SaveTo(const FString& Path, const FExtendedAtlassianCredentials& InCredentials)
{
	using namespace ExtendedAtlassianCredentialsPrivate;

	if (!InCredentials.IsValid())
	{
		UE_LOG(LogExtendedAtlassian, Warning, TEXT("Refusing to store incomplete Atlassian credentials."));
		return false;
	}

	const FString Directory = FPaths::GetPath(Path);
	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		UE_LOG(LogExtendedAtlassian, Error, TEXT("Could not create credential directory %s."), *Directory);
		return false;
	}

	FString TokenToWrite;
	bool bEncrypted = false;

#if PLATFORM_WINDOWS
	bEncrypted = EncryptToBase64(InCredentials.ApiToken, TokenToWrite);
#endif

	if (!bEncrypted)
	{
		TokenToWrite = InCredentials.ApiToken;
		UE_LOG(LogExtendedAtlassian, Warning,
			TEXT("Storing the Atlassian API token unencrypted at %s. This platform has no DPAPI equivalent wired up."), *Path);
	}

	FConfigFile Config;
	Config.SetString(Section, EmailKey, *InCredentials.Email);
	Config.SetString(Section, TokenKey, *TokenToWrite);
	Config.SetString(Section, EncryptedKey, bEncrypted ? TEXT("True") : TEXT("False"));
	Config.Dirty = true;

	if (!Config.Write(Path))
	{
		UE_LOG(LogExtendedAtlassian, Error, TEXT("Could not write credential file %s."), *Path);
		return false;
	}

	UE_LOG(LogExtendedAtlassian, Log, TEXT("Stored Atlassian credentials for %s (encrypted: %s)."),
		*InCredentials.Email, bEncrypted ? TEXT("yes") : TEXT("no"));
	return true;
}

void FExtendedAtlassianCredentialStore::Clear()
{
	const FString Path = GetStorePath();
	if (IFileManager::Get().FileExists(*Path))
	{
		IFileManager::Get().Delete(*Path, false, true);
		UE_LOG(LogExtendedAtlassian, Log, TEXT("Cleared stored Atlassian credentials."));
	}
}
