// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianTypes.h"

/**
 * Per-user cache of the last account returned by Jira's /myself endpoint.
 *
 * The cache is presentation data only. It is tied to both the normalized site URL and credential
 * e-mail so switching projects or accounts cannot briefly display the wrong identity.
 */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianUserCache
{
public:
	static bool Load(
		const FString& SiteUrl,
		const FString& CredentialEmail,
		FExtendedAtlassianUser& OutUser);

	static bool Save(
		const FString& SiteUrl,
		const FString& CredentialEmail,
		const FExtendedAtlassianUser& User);

	static void Clear();
	static FString GetStorePath();

	/** Path-explicit variants used by automation tests. */
	static bool LoadFrom(
		const FString& Path,
		const FString& SiteUrl,
		const FString& CredentialEmail,
		FExtendedAtlassianUser& OutUser);
	static bool SaveTo(
		const FString& Path,
		const FString& SiteUrl,
		const FString& CredentialEmail,
		const FExtendedAtlassianUser& User);
};
