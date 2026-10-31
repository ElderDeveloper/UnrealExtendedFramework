// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** An Atlassian Cloud API token and the account e-mail it belongs to. */
struct FExtendedAtlassianCredentials
{
	FString Email;
	FString ApiToken;

	bool IsValid() const { return !Email.IsEmpty() && !ApiToken.IsEmpty(); }
	void Reset() { *this = FExtendedAtlassianCredentials(); }
};

/**
 * Per-user credential storage, deliberately outside the project tree.
 *
 * The API token is kept in FEFCredentialStore (service "UnrealExtendedAtlassian", account = the
 * e-mail address): DPAPI-encrypted and bound to the current user on Windows, the login Keychain on
 * macOS, an owner-only file elsewhere. The e-mail address is not secret; it sits in a small account
 * file under the user's settings folder (GetStorePath()) so the token can be found again. Neither is
 * ever written to the repository or to project config.
 *
 * This protects the token at rest against casual disclosure; it is not a secret manager. Anything
 * running as this user can ask the OS to unwrap it, and an Atlassian API token grants that user's
 * full account access, so treat it accordingly.
 *
 * Credentials written by earlier versions to credentials.ini (GetLegacyStorePath()) are moved into
 * the store the first time they are loaded, and that file is then deleted.
 */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianCredentialStore
{
public:
	/** Reads stored credentials, moving a legacy credentials.ini first. False when nothing is stored or the token cannot be read. */
	static bool Load(FExtendedAtlassianCredentials& OutCredentials);

	/** Stores the token and the account file, and removes any legacy credentials.ini they replace. */
	static bool Save(const FExtendedAtlassianCredentials& InCredentials);

	/** Deletes the stored token, the account file and any legacy credentials.ini. Safe to call when nothing is stored. */
	static void Clear();

	/** True when an account file or a legacy credentials.ini exists. Does not attempt to read the token. */
	static bool HasStoredCredentials();

	/** Absolute path of the per-user account file. It holds the e-mail address only, never the token. */
	static FString GetStorePath();

	/** Absolute path of the credentials.ini written by earlier versions. Normally absent. */
	static FString GetLegacyStorePath();

	/** Where the API token is kept on this platform, for settings pages and logs. */
	static FString GetStorageDescription();

	/**
	 * Path-explicit variants. Path is the account file; the token goes to FEFCredentialStore under
	 * the account's e-mail address.
	 *
	 * Exposed so tests can exercise the round trip against a temporary account file instead of the
	 * real per-user one. Tests also redirect the token with FEFCredentialStore::SetFileStoreRootForTests,
	 * so running the suite never overwrites the user's stored token.
	 */
	static bool LoadFrom(const FString& Path, FExtendedAtlassianCredentials& OutCredentials);
	static bool SaveTo(const FString& Path, const FExtendedAtlassianCredentials& InCredentials);

	/** Deletes the token named by the account file at Path, then the file. True when anything was removed. */
	static bool ClearFrom(const FString& Path);

	/**
	 * Moves credentials from a legacy credentials.ini at LegacyPath into the store, with the account
	 * file at AccountPath, then deletes LegacyPath. True when credentials were moved. A legacy file
	 * that cannot be read (usually DPAPI data from another user or machine) is left in place.
	 */
	static bool MigrateLegacyStore(const FString& LegacyPath, const FString& AccountPath);

	/** True when the token is encrypted at rest on this platform. False means it is stored unencrypted in an owner-only file. */
	static bool IsEncryptionAvailable();

	/**
	 * Tests only: keep the account file and the legacy credentials.ini under Directory instead of the
	 * per-user folder. Combine with FEFCredentialStore::SetFileStoreRootForTests so the token is
	 * redirected too. Pass an empty string to go back to the per-user folder.
	 */
	static void SetStoreDirectoryForTests(const FString& Directory);
};
