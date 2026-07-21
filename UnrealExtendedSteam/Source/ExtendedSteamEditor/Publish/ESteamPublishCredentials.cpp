// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Publish/ESteamPublishCredentials.h"

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
	static const TCHAR* GPasswordKey   = TEXT("Password=");

	// AES block size (bytes). Buffers handed to FAES must be a multiple of this.
	static constexpr int32 GAesBlockSize = 16;

	// Embedded 32-byte AES-256 key. This is deliberately baked into the binary: it hardens casual reads
	// of the credentials file but is NOT secrecy against anyone who has the plugin source (the app must
	// decrypt unaided to pass the password to steamcmd). Must be exactly 32 characters.
	void MakeAesKey(FAES::FAESKey& OutKey)
	{
		static const ANSICHAR KeyMaterial[] = "ExtSteamPublish-AES256-v1-Key!!!";
		static_assert(sizeof(KeyMaterial) == 32 + 1, "AES-256 key material must be exactly 32 bytes.");
		FMemory::Memcpy(OutKey.Key, KeyMaterial, 32);
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

FString FESteamPublishCredentials::Encrypt(const FString& Plain)
{
	if (Plain.IsEmpty())
	{
		return FString();
	}

	FTCHARToUTF8 Utf8(*Plain);
	const int32 PlainLen = Utf8.Length();

	// Layout: [uint32 LE original length][UTF-8 bytes], zero-padded up to a multiple of the block size.
	const int32 HeaderLen = sizeof(uint32);
	int32 TotalLen = HeaderLen + PlainLen;
	if (const int32 Remainder = TotalLen % GAesBlockSize)
	{
		TotalLen += GAesBlockSize - Remainder;
	}

	TArray<uint8> Buffer;
	Buffer.SetNumZeroed(TotalLen);
	const uint32 LenLE = static_cast<uint32>(PlainLen);
	FMemory::Memcpy(Buffer.GetData(), &LenLE, HeaderLen);
	FMemory::Memcpy(Buffer.GetData() + HeaderLen, Utf8.Get(), PlainLen);

	FAES::FAESKey Key;
	MakeAesKey(Key);
	FAES::EncryptData(Buffer.GetData(), Buffer.Num(), Key);

	return FBase64::Encode(Buffer);
}

FString FESteamPublishCredentials::Decrypt(const FString& Encoded)
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
	MakeAesKey(Key);
	FAES::DecryptData(Buffer.GetData(), Buffer.Num(), Key);

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

	return FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(PlainBytes.GetData())));
}

bool FESteamPublishCredentials::Load(FESteamPublishCredentials& Out)
{
	const FString FilePath = GetCredentialsFilePath();
	FString Contents;
	if (!FFileHelper::LoadFileToString(Contents, *FilePath))
	{
		return false;
	}

	TArray<FString> Lines;
	Contents.ParseIntoArrayLines(Lines);
	for (const FString& Raw : Lines)
	{
		const FString Line = Raw.TrimStartAndEnd();
		if (Line.StartsWith(GUsernameKey))
		{
			Out.Username = Line.RightChop(FCString::Strlen(GUsernameKey)).TrimStartAndEnd();
		}
		else if (Line.StartsWith(GPasswordKey))
		{
			const FString Encoded = Line.RightChop(FCString::Strlen(GPasswordKey)).TrimStartAndEnd();
			Out.Password = Decrypt(Encoded);
		}
	}
	return true;
}

bool FESteamPublishCredentials::Save(const FESteamPublishCredentials& In, FString& OutError)
{
	const FString Directory = GetSecretsDirectory();
	if (!IFileManager::Get().MakeDirectory(*Directory, /*Tree*/ true))
	{
		OutError = FString::Printf(TEXT("Could not create secrets folder: %s"), *Directory);
		return false;
	}

	FString Contents;
	Contents += FString::Printf(TEXT("; Extended Steam upload credentials — DO NOT COMMIT.%s"), LINE_TERMINATOR);
	Contents += FString::Printf(TEXT("; Password is AES-256 encrypted with a key embedded in the plugin. Keep this folder in your VCS ignore list.%s"), LINE_TERMINATOR);
	Contents += FString::Printf(TEXT("%s%s"), GSectionHeader, LINE_TERMINATOR);
	Contents += FString::Printf(TEXT("%s%s%s"), GUsernameKey, *In.Username, LINE_TERMINATOR);
	Contents += FString::Printf(TEXT("%s%s%s"), GPasswordKey, *Encrypt(In.Password), LINE_TERMINATOR);

	if (!FFileHelper::SaveStringToFile(Contents, *GetCredentialsFilePath()))
	{
		OutError = FString::Printf(TEXT("Could not write credentials file: %s"), *GetCredentialsFilePath());
		return false;
	}
	return true;
}
