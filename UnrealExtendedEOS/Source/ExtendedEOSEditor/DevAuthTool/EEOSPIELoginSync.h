// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Read/write bridge to the engine's PIE credential list (UOnlinePIESettings).
 *
 * That class is declared in OnlineSubsystemUtils' PRIVATE folder and is not exported, so it
 * cannot be linked against — its CDO is reached through reflection instead. The element type
 * (FOnlineAccountStoredCredentials) IS public and exported, so the array contents are read and
 * written as the real struct rather than field-by-field reflection.
 *
 * Why this list and not our own: UE assigns entry N to PIE instance N in BOTH topologies.
 *  - Separate client processes: UEditorEngine::LaunchNewProcess → GetPIELoginCommandLineArgs(N)
 *    emits "-AUTH_TYPE=<Type> -AUTH_LOGIN=<Id> -AUTH_PASSWORD=<Token>", which
 *    FUserManagerEOS::AutoLogin consumes ahead of every other login branch.
 *  - Clients inside the editor process: UEditorEngine::CreateInnerProcessPIEGameInstance calls
 *    UOnlineEngineInterface::LoginPIEInstance(OnlineIdentifier, 0, N) against that PIE world's
 *    own OSS instance (PlayLevel.cpp:1875).
 */
class FEEOSPIELoginSync
{
public:

	/** One entry of the engine's list, as configured. */
	struct FLoginEntry
	{
		/** Index in UOnlinePIESettings::Logins == the PIE instance that receives it. */
		int32 InstanceIndex = INDEX_NONE;
		/** Credentials.Type — "developer" for Dev Auth Tool logins. */
		FString Type;
		/** Credentials.Id — for Developer logins, the Dev Auth Tool address. */
		FString Id;
		/** Credentials.Token — for Developer logins, the tool's credential (account) name. */
		FString Token;
		/**
		 * Mirrors FOnlineAccountStoredCredentials::IsValid() — all three fields non-empty.
		 * This is what the engine counts in GetNumPIELogins(), and an INVALID entry is not
		 * merely skipped: if the valid count drops below the client count, UE turns per-instance
		 * login off for the WHOLE session (PlayLevel.cpp:2776).
		 */
		bool bValid = false;

		bool IsDeveloperType() const;
	};

	/** True when the engine's OnlinePIESettings class could be resolved (editor builds only). */
	static bool IsAvailable();

	/**
	 * Replace the engine's login list with one Developer entry per name, in order, all pointing
	 * at Address, and enable PIE logins. Saves the config.
	 *
	 * Every entry shares the same Id (the tool's address) and differs only by Token. That is
	 * legal specifically for this type: OnlineSubsystemUtils' Editor.ini ships
	 * "+LoginTypesAllowingDuplicates=Developer", which exempts it from the duplicate-Id check in
	 * UOnlinePIESettings::PostEditChangeProperty that would otherwise blank the Ids out.
	 *
	 * @return false with OutError set when the settings class or its properties cannot be found.
	 */
	static bool WriteDeveloperLogins(const TArray<FString>& CredentialNames, const FString& Address, FString& OutError);

	/** Read the engine's list verbatim, plus whether PIE logins are enabled at all. */
	static bool ReadAllLogins(TArray<FLoginEntry>& OutLogins, bool& bOutLoginsEnabled);

	/** Count of entries the engine would consider usable — compare against the client count. */
	static int32 GetValidLoginCount();
};
