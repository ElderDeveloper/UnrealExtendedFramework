// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Steam upload credentials, persisted to <Project>/Config/SteamPublish/SteamPublishCredentials.ini.
 *
 * This file is intentionally NOT a UObject/config property: keeping it out of the committed
 * DefaultEditor.ini is the whole point. The password is encrypted at rest with AES-256 using a key
 * embedded in the plugin. That hardens casual reads (the file is real ciphertext, not grep-able text),
 * but it is NOT secrecy against anyone who has the plugin source — the app must decrypt unaided to hand
 * the password to steamcmd, so the key necessarily ships with it. The real protection remains that the
 * containing folder is added to the project's version-control ignore files (see FESteamPublishManager).
 */
struct FESteamPublishCredentials
{
	FString Username;
	FString Password;

	/** <Project>/Config/SteamPublish — the folder that holds credentials + prepared tools + build logs. */
	static FString GetSecretsDirectory();

	/** Full path to the credentials ini inside GetSecretsDirectory(). */
	static FString GetCredentialsFilePath();

	/** True if a credentials file exists on disk. */
	static bool Exists();

	/** Load credentials from disk. Returns false (and leaves Out untouched) if no file is present. */
	static bool Load(FESteamPublishCredentials& Out);

	/** Write credentials to disk, creating the folder if needed. Returns false and fills OutError on failure. */
	static bool Save(const FESteamPublishCredentials& In, FString& OutError);

private:
	/** AES-256 encrypt + Base64 encode (embedded key). Empty in -> empty out. */
	static FString Encrypt(const FString& Plain);
	/** Base64 decode + AES-256 decrypt (embedded key). Empty/garbage in -> empty out. */
	static FString Decrypt(const FString& Encoded);
};
