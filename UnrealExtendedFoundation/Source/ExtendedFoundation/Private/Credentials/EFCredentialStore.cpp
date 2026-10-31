// Copyright Moon Punch Games. All Rights Reserved.

#include "Credentials/EFCredentialStore.h"

#include "ExtendedFoundationModule.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <dpapi.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#if PLATFORM_MAC
THIRD_PARTY_INCLUDES_START
#include <Security/Security.h>
THIRD_PARTY_INCLUDES_END
#endif

#if PLATFORM_UNIX || PLATFORM_MAC
#include <sys/stat.h>
#endif

namespace EFCredentialStorePrivate
{
	const TCHAR* FileHeaderProtected = TEXT("EFCredential v1 dpapi");
	const TCHAR* FileHeaderPlain = TEXT("EFCredential v1 plain");

	FString& TestRoot()
	{
		static FString Root;
		return Root;
	}

	bool UseFileStore()
	{
#if PLATFORM_MAC
		return !TestRoot().IsEmpty();
#else
		return true;
#endif
	}

	FString SanitizeServiceName(const FString& Service)
	{
		FString Out;
		Out.Reserve(Service.Len());
		for (const TCHAR Char : Service)
		{
			Out.AppendChar(FChar::IsAlnum(Char) || Char == TEXT('.') || Char == TEXT('-') || Char == TEXT('_') ? Char : TEXT('_'));
		}
		return Out.IsEmpty() ? FString(TEXT("Default")) : Out;
	}

	FString GetFileStoreRoot()
	{
		if (!TestRoot().IsEmpty())
		{
			return TestRoot();
		}

		FString Root = FPlatformProcess::UserSettingsDir();
		if (Root.IsEmpty())
		{
			Root = FPlatformProcess::UserDir();
		}
		return FPaths::Combine(Root, TEXT("UnrealExtended"), TEXT("Credentials"));
	}

	/** One file per (service, account). The account is hashed so file names do not reveal it. */
	FString GetSecretPath(const FString& Service, const FString& Account)
	{
		const FString AccountHash = FMD5::HashAnsiString(*Account);
		return FPaths::Combine(GetFileStoreRoot(), SanitizeServiceName(Service), AccountHash + TEXT(".secret"));
	}

	void RestrictToOwner(const FString& Path, bool bDirectory)
	{
#if PLATFORM_UNIX || PLATFORM_MAC
		::chmod(TCHAR_TO_UTF8(*Path), bDirectory ? (S_IRUSR | S_IWUSR | S_IXUSR) : (S_IRUSR | S_IWUSR));
#endif
	}

#if PLATFORM_WINDOWS
	/** Binds each blob to its service and account, and to this application. */
	FString MakeEntropySource(const FString& Service, const FString& Account)
	{
		return FString::Printf(TEXT("UnrealExtended.credentials.v1|%s|%s"), *Service, *Account);
	}

	bool Protect(const FString& Service, const FString& Account, const FString& Plain, FString& OutBase64)
	{
		const FTCHARToUTF8 Utf8(*Plain);
		const FString EntropySource = MakeEntropySource(Service, Account);
		const FTCHARToUTF8 EntropyText(*EntropySource);

		DATA_BLOB In;
		In.cbData = static_cast<DWORD>(Utf8.Length());
		In.pbData = reinterpret_cast<BYTE*>(const_cast<ANSICHAR*>(Utf8.Get()));

		DATA_BLOB Entropy;
		Entropy.cbData = static_cast<DWORD>(EntropyText.Length());
		Entropy.pbData = reinterpret_cast<BYTE*>(const_cast<ANSICHAR*>(EntropyText.Get()));

		DATA_BLOB Out;
		Out.cbData = 0;
		Out.pbData = nullptr;

		if (!::CryptProtectData(&In, nullptr, &Entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &Out))
		{
			UE_LOG(LogExtendedFoundation, Warning, TEXT("CryptProtectData failed (error %u)."), ::GetLastError());
			return false;
		}

		OutBase64 = FBase64::Encode(Out.pbData, Out.cbData);
		::LocalFree(Out.pbData);
		return true;
	}

	bool Unprotect(const FString& Service, const FString& Account, const FString& InBase64, FString& OutPlain)
	{
		TArray<uint8> Blob;
		if (!FBase64::Decode(InBase64, Blob) || Blob.Num() == 0)
		{
			return false;
		}

		const FString EntropySource = MakeEntropySource(Service, Account);
		const FTCHARToUTF8 EntropyText(*EntropySource);

		DATA_BLOB In;
		In.cbData = static_cast<DWORD>(Blob.Num());
		In.pbData = Blob.GetData();

		DATA_BLOB Entropy;
		Entropy.cbData = static_cast<DWORD>(EntropyText.Length());
		Entropy.pbData = reinterpret_cast<BYTE*>(const_cast<ANSICHAR*>(EntropyText.Get()));

		DATA_BLOB Out;
		Out.cbData = 0;
		Out.pbData = nullptr;

		if (!::CryptUnprotectData(&In, nullptr, &Entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &Out))
		{
			// Usually a file copied from another user or machine, where this user's DPAPI key does not apply.
			UE_LOG(LogExtendedFoundation, Warning, TEXT("A stored credential could not be decrypted (error %u). It was likely created by another user or machine."), ::GetLastError());
			return false;
		}

		const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Out.pbData), static_cast<int32>(Out.cbData));
		OutPlain = FString(Converted.Length(), Converted.Get());

		FMemory::Memzero(Out.pbData, Out.cbData);
		::LocalFree(Out.pbData);
		return true;
	}
#endif // PLATFORM_WINDOWS

	FString EncodePlain(const FString& Plain)
	{
		const FTCHARToUTF8 Utf8(*Plain);
		return FBase64::Encode(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}

	bool DecodePlain(const FString& Encoded, FString& OutPlain)
	{
		TArray<uint8> Bytes;
		if (!FBase64::Decode(Encoded, Bytes))
		{
			return false;
		}
		const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
		OutPlain = FString(Converted.Length(), Converted.Get());
		return true;
	}

	bool FileSave(const FString& Service, const FString& Account, const FString& Secret)
	{
		const FString Path = GetSecretPath(Service, Account);
		const FString Directory = FPaths::GetPath(Path);
		if (!IFileManager::Get().MakeDirectory(*Directory, /*Tree*/ true))
		{
			UE_LOG(LogExtendedFoundation, Error, TEXT("Could not create the credential folder %s."), *Directory);
			return false;
		}
		RestrictToOwner(GetFileStoreRoot(), /*bDirectory*/ true);
		RestrictToOwner(Directory, /*bDirectory*/ true);

		FString Header = FileHeaderPlain;
		FString Payload;
#if PLATFORM_WINDOWS
		if (Protect(Service, Account, Secret, Payload))
		{
			Header = FileHeaderProtected;
		}
		else
#endif
		{
			Payload = EncodePlain(Secret);
		}

		if (!FFileHelper::SaveStringToFile(Header + TEXT("\n") + Payload + TEXT("\n"), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogExtendedFoundation, Error, TEXT("Could not write the credential file %s."), *Path);
			return false;
		}
		RestrictToOwner(Path, /*bDirectory*/ false);
		return true;
	}

	bool FileLoad(const FString& Service, const FString& Account, FString& OutSecret)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *GetSecretPath(Service, Account)))
		{
			return false;
		}

		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		if (Lines.Num() < 2)
		{
			return false;
		}

		if (Lines[0] == FileHeaderPlain)
		{
			return DecodePlain(Lines[1], OutSecret);
		}
#if PLATFORM_WINDOWS
		if (Lines[0] == FileHeaderProtected)
		{
			return Unprotect(Service, Account, Lines[1], OutSecret);
		}
#endif
		UE_LOG(LogExtendedFoundation, Warning, TEXT("A stored credential uses a format this platform cannot read."));
		return false;
	}

	bool FileDelete(const FString& Service, const FString& Account)
	{
		const FString Path = GetSecretPath(Service, Account);
		return !IFileManager::Get().FileExists(*Path) || IFileManager::Get().Delete(*Path, /*RequireExists*/ false, /*EvenReadOnly*/ true);
	}

#if PLATFORM_MAC
	CFStringRef MakeCFString(const FString& Value)
	{
		return CFStringCreateWithCString(kCFAllocatorDefault, TCHAR_TO_UTF8(*Value), kCFStringEncodingUTF8);
	}

	/** Query identifying one generic-password item. The caller releases it. */
	CFMutableDictionaryRef MakeKeychainQuery(const FString& Service, const FString& Account)
	{
		CFMutableDictionaryRef Query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFStringRef CFService = MakeCFString(TEXT("UnrealExtended.") + Service);
		CFStringRef CFAccount = MakeCFString(Account);
		CFDictionarySetValue(Query, kSecClass, kSecClassGenericPassword);
		CFDictionarySetValue(Query, kSecAttrService, CFService);
		CFDictionarySetValue(Query, kSecAttrAccount, CFAccount);
		CFRelease(CFService);
		CFRelease(CFAccount);
		return Query;
	}

	bool KeychainSave(const FString& Service, const FString& Account, const FString& Secret)
	{
		const FTCHARToUTF8 Utf8(*Secret);
		CFDataRef Data = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(Utf8.Get()), Utf8.Length());

		CFMutableDictionaryRef Query = MakeKeychainQuery(Service, Account);
		CFMutableDictionaryRef Update = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(Update, kSecValueData, Data);

		OSStatus Status = SecItemUpdate(Query, Update);
		if (Status == errSecItemNotFound)
		{
			CFDictionarySetValue(Query, kSecValueData, Data);
			Status = SecItemAdd(Query, nullptr);
		}

		CFRelease(Update);
		CFRelease(Query);
		CFRelease(Data);

		if (Status != errSecSuccess)
		{
			UE_LOG(LogExtendedFoundation, Warning, TEXT("Keychain write failed (OSStatus %d)."), static_cast<int32>(Status));
		}
		return Status == errSecSuccess;
	}

	bool KeychainLoad(const FString& Service, const FString& Account, FString& OutSecret)
	{
		CFMutableDictionaryRef Query = MakeKeychainQuery(Service, Account);
		CFDictionarySetValue(Query, kSecReturnData, kCFBooleanTrue);
		CFDictionarySetValue(Query, kSecMatchLimit, kSecMatchLimitOne);

		CFTypeRef Result = nullptr;
		const OSStatus Status = SecItemCopyMatching(Query, &Result);
		CFRelease(Query);

		if (Status != errSecSuccess || !Result)
		{
			return false;
		}

		const CFDataRef Data = static_cast<CFDataRef>(Result);
		const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(CFDataGetBytePtr(Data)), static_cast<int32>(CFDataGetLength(Data)));
		OutSecret = FString(Converted.Length(), Converted.Get());
		CFRelease(Result);
		return true;
	}

	bool KeychainDelete(const FString& Service, const FString& Account)
	{
		CFMutableDictionaryRef Query = MakeKeychainQuery(Service, Account);
		const OSStatus Status = SecItemDelete(Query);
		CFRelease(Query);
		return Status == errSecSuccess || Status == errSecItemNotFound;
	}
#endif // PLATFORM_MAC
}

bool FEFCredentialStore::Save(const FString& Service, const FString& Account, const FString& Secret)
{
	using namespace EFCredentialStorePrivate;

	if (Service.IsEmpty())
	{
		return false;
	}

#if PLATFORM_MAC
	if (!UseFileStore())
	{
		return KeychainSave(Service, Account, Secret);
	}
#endif

	static bool bWarnedUnprotected = false;
	if (!IsProtectedAtRest() && !bWarnedUnprotected)
	{
		bWarnedUnprotected = true;
		UE_LOG(LogExtendedFoundation, Warning, TEXT("Credentials on this platform are stored in owner-only files but are not encrypted: %s"), *GetStorageDescription());
	}
	return FileSave(Service, Account, Secret);
}

bool FEFCredentialStore::Load(const FString& Service, const FString& Account, FString& OutSecret)
{
	using namespace EFCredentialStorePrivate;

	OutSecret.Reset();
	if (Service.IsEmpty())
	{
		return false;
	}

#if PLATFORM_MAC
	if (!UseFileStore())
	{
		return KeychainLoad(Service, Account, OutSecret);
	}
#endif
	return FileLoad(Service, Account, OutSecret);
}

bool FEFCredentialStore::Delete(const FString& Service, const FString& Account)
{
	using namespace EFCredentialStorePrivate;

	if (Service.IsEmpty())
	{
		return true;
	}

#if PLATFORM_MAC
	if (!UseFileStore())
	{
		return KeychainDelete(Service, Account);
	}
#endif
	return FileDelete(Service, Account);
}

bool FEFCredentialStore::Has(const FString& Service, const FString& Account)
{
	FString Ignored;
	const bool bFound = Load(Service, Account, Ignored);
	Ignored.Reset();
	return bFound;
}

bool FEFCredentialStore::IsProtectedAtRest()
{
#if PLATFORM_WINDOWS
	return true;
#elif PLATFORM_MAC
	return !EFCredentialStorePrivate::UseFileStore();
#else
	return false;
#endif
}

FString FEFCredentialStore::GetStorageDescription()
{
#if PLATFORM_MAC
	if (!EFCredentialStorePrivate::UseFileStore())
	{
		return TEXT("the login Keychain");
	}
#endif
#if PLATFORM_WINDOWS
	return FString::Printf(TEXT("DPAPI-encrypted files in %s"), *EFCredentialStorePrivate::GetFileStoreRoot());
#else
	return FString::Printf(TEXT("owner-only files in %s"), *EFCredentialStorePrivate::GetFileStoreRoot());
#endif
}

void FEFCredentialStore::SetFileStoreRootForTests(const FString& Root)
{
	EFCredentialStorePrivate::TestRoot() = Root;
}
