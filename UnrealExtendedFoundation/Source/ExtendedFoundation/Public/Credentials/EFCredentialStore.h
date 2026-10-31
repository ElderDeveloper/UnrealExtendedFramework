// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Per-user secret storage for editor tools and developer workflows: API tokens, publishing
 * passwords, developer keys. Nothing here is ever written to the project, so nothing is committed
 * or packaged.
 *
 * - Windows: a file under %LOCALAPPDATA%\UnrealExtended\Credentials, encrypted with DPAPI and bound
 *   to the current Windows user. Copied to another user or machine, it cannot be decrypted.
 * - macOS: the user's login Keychain (generic password items).
 * - Other platforms: a file readable only by the current user (mode 0600) but not encrypted.
 *   IsProtectedAtRest() reports this so tools can tell the user.
 *
 * Software running as the same OS user can read these secrets, as it can for any OS secret store.
 * Do not use this for secrets a shipped game needs at runtime: those belong on a server.
 *
 *     FEFCredentialStore::Save(TEXT("UnrealExtendedAtlassian"), Email, ApiToken);
 *     FString Token;
 *     if (FEFCredentialStore::Load(TEXT("UnrealExtendedAtlassian"), Email, Token)) { ... }
 */
class EXTENDEDFOUNDATION_API FEFCredentialStore
{
public:
	/** Stores Secret for (Service, Account), replacing any existing value. */
	static bool Save(const FString& Service, const FString& Account, const FString& Secret);

	/** Reads the secret for (Service, Account). False when there is none or it cannot be decrypted. */
	static bool Load(const FString& Service, const FString& Account, FString& OutSecret);

	/** Removes the secret for (Service, Account). True when nothing remains stored. */
	static bool Delete(const FString& Service, const FString& Account);

	static bool Has(const FString& Service, const FString& Account);

	/** True on platforms that encrypt secrets at rest (Windows, macOS). */
	static bool IsProtectedAtRest();

	/** Where secrets are kept on this platform, for settings pages and logs. */
	static FString GetStorageDescription();

	/**
	 * The folder that holds the credential files on platforms that use them (Windows, Linux), for
	 * "show in folder" buttons. Empty on macOS, where secrets live in the Keychain.
	 */
	static FString GetStorageDirectory();

	/**
	 * Tests only: keep secrets as files under Root, on every platform, instead of the real store.
	 * Pass an empty string to go back to the real store.
	 */
	static void SetFileStoreRootForTests(const FString& Root);
};
