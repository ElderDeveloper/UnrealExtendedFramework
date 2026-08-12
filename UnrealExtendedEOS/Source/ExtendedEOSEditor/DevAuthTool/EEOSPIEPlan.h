// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Resolves, ahead of time, exactly which Epic account each PIE client will sign in as — and
 * whether UE is going to honour that at all.
 *
 * This exists because the failure mode is silent. UE decides per-instance login is on with
 * (PlayLevel.cpp:2776):
 *
 *     bHasRequiredLogins = DesiredNumberOfClients <= GetNumPIELogins();
 *
 * If that is false it does not sign in the clients it CAN — it disables online PIE for the
 * whole session and only says so at Verbose. Every client then falls through to the game's own
 * auto-login (for this project: persistent auth), so they all end up on the same cached Epic
 * account and nothing in the log obviously explains why. Surfacing the plan before Play is the
 * whole point of this type.
 */
struct FEEOSPIEPlan
{
	/** UOnlinePIESettings::bOnlinePIEEnabled — the master switch in "Play Credentials". */
	bool bLoginsEnabled = false;

	/** Whether the engine's PIE settings class could be read at all. */
	bool bSettingsAvailable = false;

	/** ULevelEditorPlaySettings::PlayNumberOfClients. */
	int32 NumClients = 0;

	/** Entries the engine will count (FOnlineAccountStoredCredentials::IsValid). */
	int32 NumValidLogins = 0;

	/** Total entries configured, valid or not. */
	int32 NumConfiguredLogins = 0;

	/** ULevelEditorPlaySettings::RunUnderOneProcess — reported for context, not a blocker. */
	bool bRunUnderOneProcess = false;

	/** True when UE will actually apply per-instance logins (mirrors bUseOnlineSubsystemForLogin). */
	bool bWillApplyPerInstanceLogins = false;

	/** Human-readable sign-in order, one line per client: "Client 0  ->  Alice". */
	TArray<FString> SignInOrder;

	/** Everything wrong or worth knowing, in the order it should be read. Empty == all good. */
	TArray<FString> Problems;

	/** Accounts currently saved in the Dev Auth Tool. */
	TArray<FString> AccountsInTool;

	/** True when something is listening on the tool's port. */
	bool bToolRunning = false;

	/** The tool address the plan was resolved against. */
	FString ToolAddress;

	/**
	 * Gather everything: engine play settings, the engine's PIE credential list, the Dev Auth
	 * Tool's account store and its liveness.
	 *
	 * @param bProbeTool  Whether to TCP-probe the tool's port. Skip it on paths that must not
	 *                    do I/O (e.g. a details-panel repaint); the probe is bounded but not free.
	 */
	static FEEOSPIEPlan Build(bool bProbeTool = true);

	/** The whole plan as a multi-line block, suitable for the output log. */
	FString ToLogString() const;
};
