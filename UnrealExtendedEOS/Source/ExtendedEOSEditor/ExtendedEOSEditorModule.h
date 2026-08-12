// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

struct FEEOSPIEPlan;

/**
 * Editor glue for multi-account PIE through the EOS Developer Authentication Tool.
 *
 * Deliberately small: UE already assigns Play Credentials entry N to PIE instance N, and the
 * EOS SDK already speaks the tool's protocol during an EOS_LCT_Developer login. Neither is
 * reimplemented here. What was missing is the operator experience around them:
 *
 *   - Tools ▸ Extended EOS ▸ Open Dev Auth Tool / Sync PIE Logins / Show PIE Sign-In Plan
 *   - The same actions, plus a live read-only status block, in
 *     Project Settings ▸ Extended Framework ▸ Extended EOS - PIE Logins
 *   - On every Play: the tool is started if it is down, and the resolved sign-in order is
 *     printed with anything that will stop it from happening.
 *
 * The last one is the point. UE only applies per-instance logins when
 * Clients <= valid credentials; below that it silently disables them for EVERY client and says
 * so at Verbose only, leaving all clients to fall back to the game's own auto-login and share
 * one account (see FEEOSPIEPlan).
 */
class FExtendedEOSEditorModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	/** Register the Tools ▸ Extended EOS menu section. */
	void RegisterMenus();

	/** Menu action: launch the bundled tool (no-op with a notice if already listening). */
	void ExecuteOpenDevAuthTool();

	/** Menu action: read the tool's account list and write it to the engine's PIE logins. */
	void ExecuteSyncPIELogins();

	/** Menu action: print the sign-in plan without starting a session. */
	void ExecuteShowPIEPlan();

	/**
	 * PIE is starting. Ensure the tool is up when credentials are configured, then report the
	 * resolved sign-in order. Never blocks PIE — a problem here is reported, not fatal, because
	 * the unauthenticated path is a legitimate thing to test.
	 */
	void HandlePreBeginPIE(const bool bIsSimulating);

	/** Full breakdown to the log, one-line summary (or first problem) as a toast. */
	static void ReportPlan(const FEEOSPIEPlan& Plan);

	/** Toast + log. bSuccess picks the notification icon. */
	static void Notify(const FText& Message, bool bSuccess);

	FDelegateHandle PreBeginPIEHandle;
	FDelegateHandle PostEngineInitHandle;
};
