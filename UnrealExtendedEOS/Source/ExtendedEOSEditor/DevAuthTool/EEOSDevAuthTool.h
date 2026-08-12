// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Helpers for the EOS Developer Authentication Tool that ships in the plugin's Thirdparty tree.
 *
 * WHAT THE TOOL IS (from its own unpacked bundle, resources/app/.webpack/main):
 * two halves that are worth keeping straight.
 *
 *  1. An interactive Electron login window. It opens epicgames.com/id/login with Epic's own
 *     OAuth client, exchanges the authorization code for tokens, and persists the REFRESH
 *     tokens to <AppData Roaming>/EOS_DevAuthTool/credentials_<env>.json as
 *     [{ "name": "...", "authTokenInfo": { "refresh_token": "..." } }]. Anything expiring
 *     within 30 minutes is refreshed on the tool's own schedule.
 *     This half is NOT automatable and must not be reimplemented — doing so would mean
 *     embedding Epic's private OAuth client secret, which they can rotate at any time.
 *
 *  2. A one-route local HTTP server:
 *         GET http://<host>:<port>/<CredentialName>/exchange_code
 *     answering with a short-lived exchange code, or 404 for an unknown credential name.
 *     The EOS SDK calls this ITSELF during an EOS_LCT_Developer login — the game never speaks
 *     the protocol. So there is nothing to "integrate": the only things this editor module
 *     needs are (a) is the server up, and (b) which credential names exist.
 *
 * Both of those are answered WITHOUT touching the HTTP route: liveness by a TCP connect probe,
 * names by reading the JSON file. That is deliberate — hitting /exchange_code would mint a real
 * exchange code against Epic's backend just to throw it away.
 */
class FEEOSDevAuthTool
{
public:

	/** Which credential store the tool is using — it keys the file name and is selected by the
	 *  tool's own "-gamedev" command-line switch. */
	enum class EEnvironment : uint8
	{
		Prod,
		GameDev
	};

	/**
	 * Absolute path to the bundled EOS_DevAuthTool.exe, or empty when it cannot be found.
	 * The Thirdparty folder is version-stamped (EOS_DevAuthTool-win32-x64-1.2.1), so the
	 * versioned directory is discovered by wildcard rather than hardcoded — upgrading the SDK
	 * must not silently break the launcher.
	 */
	static FString FindToolExecutable();

	/** True when something is listening on 127.0.0.1:Port (i.e. the tool's server is up). */
	static bool IsToolListening(int32 Port, int32 TimeoutMs = 250);

	/**
	 * Launch the bundled tool detached, if it is not already listening. Returns false when the
	 * executable could not be found or the process could not be created. Does NOT wait for the
	 * port — use WaitForTool for that.
	 */
	static bool LaunchTool(EEnvironment Environment);

	/**
	 * Poll the port until it accepts a connection or TimeoutSeconds elapses. Returns true once
	 * it is listening. Electron cold-start is a few seconds, so callers on the game thread
	 * should show progress.
	 */
	static bool WaitForTool(int32 Port, float TimeoutSeconds);

	/**
	 * Credential names the user has added in the tool, read from
	 * <AppData Roaming>/EOS_DevAuthTool/credentials_<env>.json. An empty result means either
	 * the tool was never run or no accounts were added yet — the two are distinguished by
	 * GetCredentialsFilePath existing.
	 */
	static TArray<FString> ReadCredentialNames(EEnvironment Environment);

	/** Path of the tool's credential store for the given environment (may not exist). */
	static FString GetCredentialsFilePath(EEnvironment Environment);

	/** "prod" / "gamedev" — matches the tool's own file-name suffix. */
	static const TCHAR* ToString(EEnvironment Environment);
};
