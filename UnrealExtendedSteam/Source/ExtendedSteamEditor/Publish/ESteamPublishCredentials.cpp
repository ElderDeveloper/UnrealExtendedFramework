// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Publish/ESteamPublishCredentials.h"

#include "Credentials/EFCredentialStore.h"
#include "ExtendedSteamEditor.h"
#include "HAL/FileManager.h"
#include "Misc/AES.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	static const TCHAR* GSectionHeader = TEXT("[SteamPublish]");
	static const TCHAR* GUsernameKey   = TEXT("Username=");

	// Legacy: older versions wrote the AES-encrypted password on this line. Load() migrates it to the
	// credential store and rewrites the file without it; nothing writes this line any more.
	static const TCHAR* GLegacyPasswordKey = TEXT("Password=");

	// FEFCredentialStore service holding the upload password. The account is the Steam user name.
	static const TCHAR* GCredentialService = TEXT("UnrealExtendedSteam.Publish");

	// AES block size (bytes). Buffers handed to FAES must be a multiple of this.
	static constexpr int32 GAesBlockSize = 16;

	// The 32-byte AES-256 key older versions embedded in the plugin to encrypt the password in the
	// credentials file. Kept ONLY to decrypt such a file once, for migration; nothing is encrypted
	// with it any more. Must be exactly 32 characters.
	void MakeLegacyAesKey(FAES::FAESKey& OutKey)
	{
		static const ANSICHAR KeyMaterial[] = "ExtSteamPublish-AES256-v1-Key!!!";
		static_assert(sizeof(KeyMaterial) == 32 + 1, "AES-256 key material must be exactly 32 bytes.");
		FMemory::Memcpy(OutKey.Key, KeyMaterial, 32);
	}

	/** What the credentials file holds. */
	struct FCredentialsFileContents
	{
		FString Username;

		/** True while the file still has a legacy Password= line (even an empty one). */
		bool bHasLegacyPassword = false;
		FString LegacyEncryptedPassword;
	};

	bool ReadCredentialsFile(const FString& FilePath, FCredentialsFileContents& OutContents)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *FilePath))
		{
			return false;
		}

		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		for (const FString& Raw : Lines)
		{
			const FString Line = Raw.TrimStartAndEnd();
			if (Line.StartsWith(GUsernameKey))
			{
				OutContents.Username = Line.RightChop(FCString::Strlen(GUsernameKey)).TrimStartAndEnd();
			}
			else if (Line.StartsWith(GLegacyPasswordKey))
			{
				OutContents.bHasLegacyPassword = true;
				OutContents.LegacyEncryptedPassword = Line.RightChop(FCString::Strlen(GLegacyPasswordKey)).TrimStartAndEnd();
			}
		}
		return true;
	}

	/** Writes the credentials file with the user name only; the password never goes into it. */
	bool WriteCredentialsFile(const FString& Directory, const FString& FilePath, const FString& UserName, FString& OutError)
	{
		if (!IFileManager::Get().MakeDirectory(*Directory, /*Tree*/ true))
		{
			OutError = FString::Printf(TEXT("Could not create secrets folder: %s"), *Directory);
			return false;
		}

		FString Text;
		Text += FString::Printf(TEXT("; Extended Steam upload credentials — DO NOT COMMIT.%s"), LINE_TERMINATOR);
		Text += FString::Printf(TEXT("; Only the Steam user name is kept here. The password is in %s, never in project files.%s"),
			*FEFCredentialStore::GetStorageDescription(), LINE_TERMINATOR);
		Text += FString::Printf(TEXT("%s%s"), GSectionHeader, LINE_TERMINATOR);
		Text += FString::Printf(TEXT("%s%s%s"), GUsernameKey, *UserName, LINE_TERMINATOR);

		if (!FFileHelper::SaveStringToFile(Text, *FilePath))
		{
			OutError = FString::Printf(TEXT("Could not write credentials file: %s"), *FilePath);
			return false;
		}
		return true;
	}
}

FString FESteamPublishCredentials::GetSecretsDirectory()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Config"), TEXT("SteamPublish")));
}

FString FESteamPublishCredentials::GetCredentialsFilePath()
{
	return FPaths::Combine(GetSecretsDirectory(), TEXT("SteamPublishCredentials.ini"));
}

bool FESteamPublishCredentials::Exists()
{
	return FPaths::FileExists(GetCredentialsFilePath());
}

const TCHAR* FESteamPublishCredentials::GetCredentialStoreService()
{
	return GCredentialService;
}

FString FESteamPublishCredentials::GetPasswordStorageDescription()
{
	return FEFCredentialStore::GetStorageDescription();
}

bool FESteamPublishCredentials::IsPasswordProtectedAtRest()
{
	return FEFCredentialStore::IsProtectedAtRest();
}

FString FESteamPublishCredentials::DecryptLegacyPassword(const FString& Encoded)
{
	if (Encoded.IsEmpty())
	{
		return FString();
	}

	TArray<uint8> Buffer;
	if (!FBase64::Decode(Encoded, Buffer) || Buffer.Num() < static_cast<int32>(sizeof(uint32)) || (Buffer.Num() % GAesBlockSize) != 0)
	{
		UE_LOG(LogExtendedSteamEditor, Warning, TEXT("Credentials: stored password is malformed; ignoring it."));
		return FString();
	}

	FAES::FAESKey Key;
	MakeLegacyAesKey(Key);
	FAES::DecryptData(Buffer.GetData(), Buffer.Num(), Key);

	// Layout written by the old code: [uint32 LE original length][UTF-8 bytes], zero-padded to the block size.
	uint32 PlainLen = 0;
	FMemory::Memcpy(&PlainLen, Buffer.GetData(), sizeof(uint32));
	if (PlainLen > static_cast<uint32>(Buffer.Num()) - sizeof(uint32))
	{
		// Wrong key or corrupt data — decrypted length is impossible.
		UE_LOG(LogExtendedSteamEditor, Warning, TEXT("Credentials: could not decrypt stored password (wrong key or corrupt file)."));
		return FString();
	}

	TArray<uint8> PlainBytes;
	PlainBytes.Append(Buffer.GetData() + sizeof(uint32), PlainLen);
	PlainBytes.Add(0); // null-terminate for UTF8 conversion

	const FString Plain = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(PlainBytes.GetData())));
	FMemory::Memzero(Buffer.GetData(), Buffer.Num());
	FMemory::Memzero(PlainBytes.GetData(), PlainBytes.Num());
	return Plain;
}

bool FESteamPublishCredentials::Load(FESteamPublishCredentials& Out)
{
	const FString FilePath = GetCredentialsFilePath();
	FCredentialsFileContents File;
	if (!ReadCredentialsFile(FilePath, File))
	{
		return false;
	}

	Out.Username = File.Username;

	FString LoadedPassword;

	// One-time migration from older versions, which kept the password AES-encrypted in this file:
	// decrypt it with the old code, save it to the credential store, then rewrite the file without it.
	if (File.bHasLegacyPassword)
	{
		LoadedPassword = DecryptLegacyPassword(File.LegacyEncryptedPassword);

		const bool bStored = LoadedPassword.IsEmpty() || FEFCredentialStore::Save(GCredentialService, File.Username, LoadedPassword);
		if (!bStored)
		{
			// Keep the old line so the password is not lost; the migration is retried on the next load.
			UE_LOG(LogExtendedSteamEditor, Warning,
				TEXT("Credentials: could not move the Steam publish password into %s; it stays in %s for now."),
				*FEFCredentialStore::GetStorageDescription(), *FilePath);
		}
		else
		{
			FString Error;
			if (!WriteCredentialsFile(GetSecretsDirectory(), FilePath, File.Username, Error))
			{
				UE_LOG(LogExtendedSteamEditor, Warning,
					TEXT("Credentials: the old password entry could not be removed from the credentials file (%s). Delete its Password= line by hand."),
					*Error);
			}
			else if (!LoadedPassword.IsEmpty())
			{
				UE_LOG(LogExtendedSteamEditor, Display,
					TEXT("Credentials: moved the Steam publish password from %s into %s."),
					*FilePath, *FEFCredentialStore::GetStorageDescription());
			}
			else if (!File.LegacyEncryptedPassword.IsEmpty())
			{
				UE_LOG(LogExtendedSteamEditor, Warning,
					TEXT("Credentials: removed an unreadable password from %s. Enter it again and click 'Save Credentials'."),
					*FilePath);
			}
		}
	}

	if (LoadedPassword.IsEmpty())
	{
		FString StoredPassword;
		if (FEFCredentialStore::Load(GCredentialService, File.Username, StoredPassword))
		{
			LoadedPassword = MoveTemp(StoredPassword);
		}
	}

	if (!LoadedPassword.IsEmpty())
	{
		Out.Password = MoveTemp(LoadedPassword);
	}
	return true;
}

bool FESteamPublishCredentials::Save(const FESteamPublishCredentials& In, FString& OutError)
{
	if (In.Password.IsEmpty())
	{
		FEFCredentialStore::Delete(GCredentialService, In.Username);
	}
	else if (!FEFCredentialStore::Save(GCredentialService, In.Username, In.Password))
	{
		OutError = FString::Printf(TEXT("Could not save the password to %s."), *FEFCredentialStore::GetStorageDescription());
		return false;
	}

	// Switching to another account must not leave the previous account's password behind.
	FCredentialsFileContents Previous;
	if (ReadCredentialsFile(GetCredentialsFilePath(), Previous) && !Previous.Username.Equals(In.Username, ESearchCase::IgnoreCase))
	{
		FEFCredentialStore::Delete(GCredentialService, Previous.Username);
	}

	return WriteCredentialsFile(GetSecretsDirectory(), GetCredentialsFilePath(), In.Username, OutError);
}
