// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EEOSPIELoginSettings.generated.h"

/**
 * Editor-only behaviour for signing PIE instances into different Epic accounts through the EOS
 * Developer Authentication Tool.
 *
 * WHAT THIS DOES NOT OWN. The account list itself lives in the ENGINE's own
 * "Level Editor → Play Credentials" panel (UOnlinePIESettings::Logins) — that is the list UE
 * reads when it builds each PIE client's command line
 * (UEditorEngine::LaunchNewProcess → GetPIELoginCommandLineArgs → "-AUTH_TYPE/-AUTH_LOGIN/-AUTH_PASSWORD",
 * one entry per instance index). Duplicating it here would only create drift, so this module
 * POPULATES that list from the Dev Auth Tool instead of replacing it.
 *
 * The tool's address is likewise not duplicated: it comes from
 * Extended EOS → Developer → "Dev Auth Tool Address" (UEEOSSettings::DevAuthToolAddress), the
 * same value a packaged/standalone Developer login uses.
 *
 * REQUIRES separate client processes. UE only injects per-instance credentials when each PIE
 * client is its own process — Editor Preferences → Level Editor → Play → Multiplayer Options →
 * uncheck "Run Under One Process". With one shared process every client shares one command
 * line, so they would all sign in as the same account.
 */
UCLASS(config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "Extended EOS - PIE Logins"))
class UEEOSPIELoginSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UEEOSPIELoginSettings();

	/**
	 * Use the tool's non-production account service (its own "-gamedev" switch). Also selects
	 * which credential store is read: credentials_gamedev.json instead of credentials_prod.json.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Dev Auth Tool",
		meta = (DisplayName = "Use GameDev Environment"))
	bool bUseGameDevEnvironment = false;

	/**
	 * Start the bundled EOS_DevAuthTool.exe when a PIE session begins and nothing is listening
	 * on the configured port. Without the tool running, every Developer-type PIE login fails.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Dev Auth Tool",
		meta = (DisplayName = "Auto-Launch Tool on Play"))
	bool bAutoLaunchOnPlay = true;

	/**
	 * How long to wait for the tool's port to open after auto-launching it. This blocks the
	 * editor (with a progress dialog) because PIE cannot be delayed asynchronously — it only
	 * happens on the first Play after the tool is closed. Electron cold-start is a few seconds.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Dev Auth Tool",
		meta = (DisplayName = "Launch Timeout (Seconds)", ClampMin = "0.0", ClampMax = "60.0",
			EditCondition = "bAutoLaunchOnPlay"))
	float LaunchTimeoutSeconds = 20.f;

	/**
	 * Before each PIE session, check every configured Developer login's credential name against
	 * the names actually present in the tool's store, and warn about the ones that are missing.
	 * Cheap (reads a local file) and turns an opaque per-client login failure into a named one.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Dev Auth Tool",
		meta = (DisplayName = "Validate Credential Names on Play"))
	bool bValidateCredentialsOnPlay = true;

	// ── Status (read-only; press Refresh to re-poll) ─────────────────────────
	// Transient and VisibleAnywhere: this is a live view of the Dev Auth Tool and the engine's
	// Play Credentials, not stored configuration. A details panel has no "on shown" hook, so it
	// is refreshed on editor start, on any edit here, after Open/Sync, before every PIE session,
	// and on demand via Refresh Status.

	/** Whether the tool is currently listening, and on which address. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Status",
		meta = (DisplayName = "Dev Auth Tool"))
	FString ToolStatus;

	/** Accounts saved in the tool — these are the names you can assign to clients. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Status",
		meta = (DisplayName = "Accounts In Tool"))
	TArray<FString> AccountsInTool;

	/** Exactly which account each PIE client will sign in as on the next Play. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Status",
		meta = (DisplayName = "Sign-In Order"))
	TArray<FString> SignInOrder;

	/** Anything that will stop the above from happening. Empty means you are good to go. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Status",
		meta = (DisplayName = "Problems"))
	TArray<FString> Problems;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Re-poll the tool and the engine's Play Credentials, and rebuild the status above. */
	UFUNCTION(CallInEditor, Category = "Status", meta = (DisplayName = "Refresh Status"))
	void RefreshStatus();

	/** Launch the bundled Dev Auth Tool (no-op if it is already listening). */
	UFUNCTION(CallInEditor, Category = "Actions", meta = (DisplayName = "Open Dev Auth Tool"))
	void OpenDevAuthTool();

	/**
	 * Replace the engine's Play Credentials with one Developer login per account in the tool,
	 * in the order the tool stores them. Client N then signs in as account N.
	 */
	UFUNCTION(CallInEditor, Category = "Actions", meta = (DisplayName = "Sync PIE Logins From Dev Auth Tool"))
	void SyncPIELoginsFromDevAuthTool();

	/** Resolved from UEEOSSettings::DevAuthToolAddress; falls back to localhost:6547. */
	static FString GetDevAuthToolAddress();

	/** Port half of GetDevAuthToolAddress(), or 6547 when it cannot be parsed. */
	static int32 GetDevAuthToolPort();

	/** Rebuild the status fields on the CDO. Safe to call from anywhere on the game thread. */
	static void RefreshStatusOnDefault();

	virtual FName GetCategoryName() const override { return FName(TEXT("Extended Framework")); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
