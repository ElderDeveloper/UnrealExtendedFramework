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
 * The file lives under %LOCALAPPDATA%\UnrealExtendedAtlassian\ rather than anywhere in the
 * repository, so no .gitignore mistake can commit a token. On Windows the token is wrapped with
 * DPAPI (CryptProtectData) bound to the current user account, so copying the file to another
 * machine or user yields nothing usable.
 *
 * This is obfuscation-at-rest against casual disclosure, not a secret manager: anything running as
 * this user can unwrap it. An Atlassian API token grants that user's full account access, so treat
 * it accordingly.
 */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianCredentialStore
{
public:
	/** Reads and decrypts stored credentials. Returns false when nothing is stored or decryption fails. */
	static bool Load(FExtendedAtlassianCredentials& OutCredentials);

	/** Encrypts and writes credentials, creating the directory if needed. */
	static bool Save(const FExtendedAtlassianCredentials& InCredentials);

	/** Deletes the credential file. Safe to call when nothing is stored. */
	static void Clear();

	/** True when a credential file exists on disk. Does not attempt to decrypt it. */
	static bool HasStoredCredentials();

	/** Absolute path of the credential file. */
	static FString GetStorePath();

	/**
	 * Path-explicit variants.
	 *
	 * Exposed so tests can exercise the encrypt/decrypt round trip against a temporary file instead
	 * of the real per-user store, which would otherwise be overwritten by running the test suite.
	 */
	static bool LoadFrom(const FString& Path, FExtendedAtlassianCredentials& OutCredentials);
	static bool SaveTo(const FString& Path, const FExtendedAtlassianCredentials& InCredentials);

	/** True when the token is encrypted at rest on this platform. False means it is stored as plain text. */
	static bool IsEncryptionAvailable();
};
