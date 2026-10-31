// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Steam upload credentials.
 *
 * - The user name is kept in <Project>/Config/SteamPublish/SteamPublishCredentials.ini, a folder the
 *   publish tools add to the project's version-control ignore files (see FESteamPublishManager).
 * - The password is kept in the per-user OS credential store (FEFCredentialStore, service
 *   "UnrealExtendedSteam.Publish", account = the Steam user name): DPAPI-encrypted on Windows, the
 *   login Keychain on macOS, a file only the user can read elsewhere. It is never written to project
 *   config or anywhere inside the project folder.
 *
 * This is intentionally NOT a UObject/config property: keeping credentials out of the committed
 * DefaultEditor.ini is the whole point.
 *
 * Older versions of the plugin stored the password in the credentials file, AES-encrypted with a key
 * embedded in the plugin. Load() migrates such a value once: it decrypts it, saves it to the
 * credential store and rewrites the file without it.
 */
struct FESteamPublishCredentials
{
	FString Username;
	FString Password;

	/** <Project>/Config/SteamPublish — the folder that holds the user name file + prepared tools + build logs. */
	static FString GetSecretsDirectory();

	/** Full path to the credentials ini (user name only) inside GetSecretsDirectory(). */
	static FString GetCredentialsFilePath();

	/** True if a credentials file exists on disk. */
	static bool Exists();

	/**
	 * Load credentials: the user name from the credentials file, the password from the credential
	 * store (migrating a legacy password left in the file first). Returns false (and leaves Out
	 * untouched) if no file is present. Out.Password is left untouched when no password is stored.
	 */
	static bool Load(FESteamPublishCredentials& Out);

	/**
	 * Save the password to the credential store (an empty password removes it) and the user name to
	 * the credentials file, creating the folder if needed. Returns false and fills OutError on failure.
	 */
	static bool Save(const FESteamPublishCredentials& In, FString& OutError);

	/** The FEFCredentialStore service the password is saved under; the account is the Steam user name. */
	static const TCHAR* GetCredentialStoreService();

	/** Where the password is kept on this machine, for messages shown to the user. */
	static FString GetPasswordStorageDescription();

	/** False on platforms where the credential store keeps the password unencrypted (owner-only file). */
	static bool IsPasswordProtectedAtRest();

private:
	/**
	 * Base64 decode + AES-256 decrypt with the key older versions embedded in the plugin. Used only
	 * to migrate a legacy value; nothing is encrypted this way any more. Empty/garbage in -> empty out.
	 */
	static FString DecryptLegacyPassword(const FString& Encoded);
};
