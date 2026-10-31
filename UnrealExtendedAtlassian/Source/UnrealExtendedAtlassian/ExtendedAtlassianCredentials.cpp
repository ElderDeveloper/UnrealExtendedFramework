// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianCredentials.h"

#include "ExtendedAtlassianLog.h"

#include "Credentials/EFCredentialStore.h"
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
	/** FEFCredentialStore service; the account is the Atlassian e-mail address. */
	const TCHAR* ServiceName = TEXT("UnrealExtendedAtlassian");

	/** Account file layout: [Atlassian] Email=... The legacy credentials.ini used the same section. */
	const TCHAR* Section = TEXT("Atlassian");
	const TCHAR* EmailKey = TEXT("Email");

	/** Legacy credentials.ini keys, read only to migrate that file. */
	const TCHAR* LegacyTokenKey = TEXT("Token");
	const TCHAR* LegacyEncryptedKey = TEXT("Encrypted");

	FString& TestDirectory()
	{
		static FString Directory;
		return Directory;
	}

	/** <user settings>/UnrealExtendedAtlassian, which also holds the verified-user cache. */
	FString GetStoreDirectory()
	{
		if (!TestDirectory().IsEmpty())
		{
			return TestDirectory();
		}

		FString Root = FPlatformProcess::UserSettingsDir();
		if (Root.IsEmpty())
		{
			Root = FPlatformProcess::UserDir();
		}

		return FPaths::Combine(Root, TEXT("UnrealExtendedAtlassian"));
	}

	/** The e-mail address recorded in an account file, or empty when there is none. */
	FString ReadAccountEmail(const FString& AccountPath)
	{
		FString Email;
		if (IFileManager::Get().FileExists(*AccountPath))
		{
			FConfigFile Config;
			Config.Read(AccountPath);
			Config.GetString(Section, EmailKey, Email);
		}
		return Email;
	}

	/** True when nothing is left at Path. */
	bool DeleteFileIfPresent(const FString& Path)
	{
		IFileManager& FileManager = IFileManager::Get();
		return !FileManager.FileExists(*Path) || FileManager.Delete(*Path, /*RequireExists*/ false, /*EvenReadOnly*/ true);
	}

#if PLATFORM_WINDOWS
	/** The application-specific DPAPI entropy the legacy credentials.ini was written with. */
	const ANSICHAR LegacyEntropySalt[] = "UnrealExtendedAtlassian.credentials.v1";

	bool DecryptLegacyToken(const FString& InBase64, FString& OutPlain)
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
		Entropy.cbData = static_cast<DWORD>(sizeof(LegacyEntropySalt) - 1);
		Entropy.pbData = reinterpret_cast<BYTE*>(const_cast<ANSICHAR*>(LegacyEntropySalt));

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

	/** The pre-foundation reader, unchanged apart from names: an ini with Email, Token and Encrypted keys. */
	bool LoadLegacyFrom(const FString& Path, FExtendedAtlassianCredentials& OutCredentials)
	{
		OutCredentials.Reset();

		if (!IFileManager::Get().FileExists(*Path))
		{
			return false;
		}

		FConfigFile Config;
		Config.Read(Path);

		FString StoredToken;
		if (!Config.GetString(Section, EmailKey, OutCredentials.Email) ||
			!Config.GetString(Section, LegacyTokenKey, StoredToken))
		{
			UE_LOG(LogExtendedAtlassian, Warning, TEXT("Credential file at %s is missing expected keys."), *Path);
			OutCredentials.Reset();
			return false;
		}

		FString EncryptedFlag;
		Config.GetString(Section, LegacyEncryptedKey, EncryptedFlag);
		const bool bStoredEncrypted = EncryptedFlag.ToBool();

		if (!bStoredEncrypted)
		{
			OutCredentials.ApiToken = StoredToken;
			return OutCredentials.IsValid();
		}

#if PLATFORM_WINDOWS
		if (!DecryptLegacyToken(StoredToken, OutCredentials.ApiToken))
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

	enum class ELegacyMigration : uint8
	{
		/** There is no legacy file. */
		NoLegacyFile,
		/** The legacy credentials are in the store now and the file is gone. */
		Migrated,
		/** The legacy file could not be read. It stays until the user saves or clears credentials. */
		Unreadable,
		/** The legacy file was read but the store refused the write. It stays, and is still usable. */
		NotMoved,
	};

	/** Moves a legacy credentials.ini into the store. OutLegacy holds what was read from it. */
	ELegacyMigration MigrateLegacyFile(const FString& LegacyPath, const FString& AccountPath, FExtendedAtlassianCredentials& OutLegacy)
	{
		OutLegacy.Reset();

		if (!IFileManager::Get().FileExists(*LegacyPath))
		{
			return ELegacyMigration::NoLegacyFile;
		}

		if (!LoadLegacyFrom(LegacyPath, OutLegacy))
		{
			return ELegacyMigration::Unreadable;
		}

		if (!FExtendedAtlassianCredentialStore::SaveTo(AccountPath, OutLegacy))
		{
			UE_LOG(LogExtendedAtlassian, Warning,
				TEXT("Could not move the Atlassian credentials in %s into %s. Still reading them from that file; the move is retried on the next load."),
				*LegacyPath, *FEFCredentialStore::GetStorageDescription());
			return ELegacyMigration::NotMoved;
		}

		if (DeleteFileIfPresent(LegacyPath))
		{
			UE_LOG(LogExtendedAtlassian, Log, TEXT("Moved the stored Atlassian credentials from %s into %s."),
				*LegacyPath, *FEFCredentialStore::GetStorageDescription());
		}
		else
		{
			UE_LOG(LogExtendedAtlassian, Warning,
				TEXT("Moved the stored Atlassian credentials into %s, but could not delete %s. Delete that file by hand."),
				*FEFCredentialStore::GetStorageDescription(), *LegacyPath);
		}

		return ELegacyMigration::Migrated;
	}
}

FString FExtendedAtlassianCredentialStore::GetStorePath()
{
	return FPaths::Combine(ExtendedAtlassianCredentialsPrivate::GetStoreDirectory(), TEXT("account.ini"));
}

FString FExtendedAtlassianCredentialStore::GetLegacyStorePath()
{
	return FPaths::Combine(ExtendedAtlassianCredentialsPrivate::GetStoreDirectory(), TEXT("credentials.ini"));
}

FString FExtendedAtlassianCredentialStore::GetStorageDescription()
{
	return FEFCredentialStore::GetStorageDescription();
}

bool FExtendedAtlassianCredentialStore::IsEncryptionAvailable()
{
	return FEFCredentialStore::IsProtectedAtRest();
}

bool FExtendedAtlassianCredentialStore::HasStoredCredentials()
{
	IFileManager& FileManager = IFileManager::Get();
	return FileManager.FileExists(*GetStorePath()) || FileManager.FileExists(*GetLegacyStorePath());
}

bool FExtendedAtlassianCredentialStore::Load(FExtendedAtlassianCredentials& OutCredentials)
{
	using namespace ExtendedAtlassianCredentialsPrivate;

	FExtendedAtlassianCredentials Legacy;
	if (MigrateLegacyFile(GetLegacyStorePath(), GetStorePath(), Legacy) == ELegacyMigration::NotMoved)
	{
		// Readable but not movable: keep working from the old file rather than signing the user out.
		OutCredentials = MoveTemp(Legacy);
		return OutCredentials.IsValid();
	}

	return LoadFrom(GetStorePath(), OutCredentials);
}

bool FExtendedAtlassianCredentialStore::Save(const FExtendedAtlassianCredentials& InCredentials)
{
	if (!SaveTo(GetStorePath(), InCredentials))
	{
		return false;
	}

	// New credentials supersede a legacy file, including one that could not be read here.
	ExtendedAtlassianCredentialsPrivate::DeleteFileIfPresent(GetLegacyStorePath());
	return true;
}

bool FExtendedAtlassianCredentialStore::LoadFrom(const FString& Path, FExtendedAtlassianCredentials& OutCredentials)
{
	using namespace ExtendedAtlassianCredentialsPrivate;

	OutCredentials.Reset();

	if (!IFileManager::Get().FileExists(*Path))
	{
		return false;
	}

	const FString Email = ReadAccountEmail(Path);
	if (Email.IsEmpty())
	{
		UE_LOG(LogExtendedAtlassian, Warning, TEXT("Credential file at %s is missing expected keys."), *Path);
		return false;
	}

	FString Token;
	if (!FEFCredentialStore::Load(ServiceName, Email, Token) || Token.IsEmpty())
	{
		// Usually a store copied from another user or machine, or a Keychain item that was removed.
		UE_LOG(LogExtendedAtlassian, Warning,
			TEXT("The stored Atlassian API token could not be read from %s. It may have been created by a different user or machine. Re-enter it in Project Settings."),
			*FEFCredentialStore::GetStorageDescription());
		return false;
	}

	OutCredentials.Email = Email;
	OutCredentials.ApiToken = MoveTemp(Token);
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

	// The account whose token is stored now, so a token saved under a different e-mail is not left behind.
	const FString PreviousEmail = ReadAccountEmail(Path);

	if (!FEFCredentialStore::Save(ServiceName, InCredentials.Email, InCredentials.ApiToken))
	{
		UE_LOG(LogExtendedAtlassian, Error, TEXT("Could not store the Atlassian API token in %s."), *FEFCredentialStore::GetStorageDescription());
		return false;
	}

	const bool bSameAccount = PreviousEmail.Equals(InCredentials.Email, ESearchCase::CaseSensitive);

	// The account file holds the e-mail address only; the token never touches it.
	FConfigFile Config;
	Config.SetString(Section, EmailKey, *InCredentials.Email);
	Config.Dirty = true;

	if (!Config.Write(Path))
	{
		UE_LOG(LogExtendedAtlassian, Error, TEXT("Could not write credential file %s."), *Path);

		// Without the account file this token can never be found again, so do not leave it behind.
		// A token stored under the account the existing file names is still reachable; keep it.
		if (!bSameAccount)
		{
			FEFCredentialStore::Delete(ServiceName, InCredentials.Email);
		}
		return false;
	}

	if (!PreviousEmail.IsEmpty() && !bSameAccount)
	{
		FEFCredentialStore::Delete(ServiceName, PreviousEmail);
	}

	UE_LOG(LogExtendedAtlassian, Log, TEXT("Stored Atlassian credentials. The API token is kept in %s (encrypted: %s)."),
		*FEFCredentialStore::GetStorageDescription(), FEFCredentialStore::IsProtectedAtRest() ? TEXT("yes") : TEXT("no"));
	return true;
}

bool FExtendedAtlassianCredentialStore::ClearFrom(const FString& Path)
{
	using namespace ExtendedAtlassianCredentialsPrivate;

	const bool bHadAccountFile = IFileManager::Get().FileExists(*Path);

	const FString Email = ReadAccountEmail(Path);
	if (!Email.IsEmpty())
	{
		FEFCredentialStore::Delete(ServiceName, Email);
	}

	DeleteFileIfPresent(Path);
	return bHadAccountFile;
}

void FExtendedAtlassianCredentialStore::Clear()
{
	const FString LegacyPath = GetLegacyStorePath();
	const bool bHadLegacyFile = IFileManager::Get().FileExists(*LegacyPath);
	ExtendedAtlassianCredentialsPrivate::DeleteFileIfPresent(LegacyPath);

	const bool bHadAccountFile = ClearFrom(GetStorePath());

	if (bHadAccountFile || bHadLegacyFile)
	{
		UE_LOG(LogExtendedAtlassian, Log, TEXT("Cleared stored Atlassian credentials."));
	}
}

bool FExtendedAtlassianCredentialStore::MigrateLegacyStore(const FString& LegacyPath, const FString& AccountPath)
{
	using namespace ExtendedAtlassianCredentialsPrivate;

	FExtendedAtlassianCredentials Legacy;
	return MigrateLegacyFile(LegacyPath, AccountPath, Legacy) == ELegacyMigration::Migrated;
}

void FExtendedAtlassianCredentialStore::SetStoreDirectoryForTests(const FString& Directory)
{
	ExtendedAtlassianCredentialsPrivate::TestDirectory() = Directory;
}
