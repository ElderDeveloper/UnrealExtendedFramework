// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ExtendedAtlassianSettings.generated.h"

/** A named JQL query offered in the issue browser's preset dropdown. */
USTRUCT()
struct FExtendedAtlassianJqlPreset
{
	GENERATED_BODY()

	/** Label shown in the dropdown. */
	UPROPERTY(EditAnywhere, Config, Category = "Jira")
	FString Name;

	/** The query itself, e.g. assignee = currentUser() AND resolution = Unresolved ORDER BY updated DESC */
	UPROPERTY(EditAnywhere, Config, Category = "Jira")
	FString Jql;
};

/**
 * Project-level Atlassian configuration, shared by the whole team through Config/DefaultGame.ini.
 *
 * Credentials are deliberately absent — the API token is per-user and lives outside the repository.
 * See FExtendedAtlassianCredentialStore.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Extended Atlassian"))
class UNREALEXTENDEDATLASSIAN_API UExtendedAtlassianSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UExtendedAtlassianSettings();

	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;
	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif

	static const UExtendedAtlassianSettings* Get();
	static UExtendedAtlassianSettings* GetMutable() { return GetMutableDefault<UExtendedAtlassianSettings>(); }

	/** Atlassian Cloud site, e.g. https://yourcompany.atlassian.net */
	UPROPERTY(EditAnywhere, Config, Category = "Connection", meta = (DisplayName = "Atlassian Site URL"))
	FString SiteUrl;

	// ---------------------------------------------------------------------------------------------
	// Credentials (transient — mirrored to the per-user store, never to committed config)
	// ---------------------------------------------------------------------------------------------

	/** E-mail address of your Atlassian account. Used as the HTTP Basic username. */
	UPROPERTY(EditAnywhere, Transient, Category = "Credentials")
	FString AccountEmail;

	/** Atlassian Cloud API token. Stored DPAPI-encrypted per-user outside the repository, never in committed config. */
	UPROPERTY(EditAnywhere, Transient, Category = "Credentials", meta = (PasswordField = true))
	FString ApiToken;

	/** Result of the last save or connection test. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Credentials")
	FString ConnectionStatus;

#if WITH_EDITOR
	// ---------------------------------------------------------------------------------------------
	// Actions, driven by FExtendedAtlassianSettingsCustomization.
	//
	// These are deliberately NOT UFUNCTION(CallInEditor): those buttons do not render or fire in the
	// Project Settings viewer in this engine build — verified against both this plugin and
	// UESteamPublishSettings, where they are equally dead. The Slate customization is the mechanism
	// that actually works.
	// ---------------------------------------------------------------------------------------------

	/** Write the e-mail and token to the per-user credential file. */
	void SaveCredentials();

	/** Verify the stored credentials against /rest/api/3/myself, then fill the dropdown lists. */
	void TestConnection();

	/** Open id.atlassian.com, where API tokens are created. */
	void OpenApiTokenPage();

	/** Forget and delete the stored credentials on this machine. */
	void ClearStoredCredentials();

	/** Re-read projects, issue types, priorities and spaces from Atlassian to fill the dropdowns. */
	void RefreshAtlassianLists();

	/** Reveal the credential file in the file browser. */
	void ShowCredentialFileLocation();
#endif

	// ---------------------------------------------------------------------------------------------
	// Dropdown sources
	//
	// GetOptions requires a synchronous answer, so these serve a cache filled by
	// RefreshAtlassianLists. Each one always includes the currently configured value, so an
	// unreachable site or a project you can no longer see never silently erases your setting.
	// ---------------------------------------------------------------------------------------------

	UFUNCTION()
	TArray<FString> GetProjectKeyOptions() const;

	UFUNCTION()
	TArray<FString> GetIssueTypeOptions() const;

	UFUNCTION()
	TArray<FString> GetPriorityOptions() const;

	UFUNCTION()
	TArray<FString> GetSpaceKeyOptions() const;

	/** Jira project used for issue browsing and as the default for new issues. Picked from your projects after connecting. */
	UPROPERTY(EditAnywhere, Config, Category = "Jira", meta = (GetOptions = "GetProjectKeyOptions"))
	FString ProjectKey;

	/** Project that bug reports are filed into. Leave empty to use ProjectKey. */
	UPROPERTY(EditAnywhere, Config, Category = "Jira", meta = (GetOptions = "GetProjectKeyOptions"))
	FString BugProjectKey;

	/** Issue type pre-selected in the bug report dialog. Listed from the bug project after connecting. */
	UPROPERTY(EditAnywhere, Config, Category = "Jira", meta = (GetOptions = "GetIssueTypeOptions"))
	FString DefaultIssueTypeName;

	/** Priority pre-selected in the bug report dialog. Leave empty to use the project default. */
	UPROPERTY(EditAnywhere, Config, Category = "Jira", meta = (GetOptions = "GetPriorityOptions"))
	FString DefaultPriorityName;

	/** Queries offered in the issue browser. The first entry is selected on open. */
	UPROPERTY(EditAnywhere, Config, Category = "Jira")
	TArray<FExtendedAtlassianJqlPreset> JqlPresets;

	/** Confluence space keys shown in the documentation browser. Empty means all spaces you can read. */
	UPROPERTY(EditAnywhere, Config, Category = "Confluence", meta = (GetOptions = "GetSpaceKeyOptions"))
	TArray<FString> SpaceKeys;

	/**
	 * Include personal spaces (the ~accountid one each user gets) in the documentation browser.
	 *
	 * Off by default: on a team of any size they swamp the real spaces and are nearly always empty.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Confluence")
	bool bIncludePersonalSpaces;

	/**
	 * Periodically refresh the issue browser.
	 *
	 * Off by default on purpose: Atlassian Cloud rate limits are cost-based, and a team of editors
	 * polling in parallel is a reliable way to get throttled.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Polling")
	bool bEnablePolling;

	/** Seconds between automatic refreshes. Values below 60 are clamped; Atlassian throttles aggressively. */
	UPROPERTY(EditAnywhere, Config, Category = "Polling", meta = (ClampMin = "60", UIMin = "60", EditCondition = "bEnablePolling"))
	int32 PollIntervalSeconds;

	/** Capture the viewport when the bug report dialog opens. The dialog can still opt out per report. */
	UPROPERTY(EditAnywhere, Config, Category = "Bug Report")
	bool bCaptureScreenshot;

	/** Attach the tail of the editor log to new bug reports. */
	UPROPERTY(EditAnywhere, Config, Category = "Bug Report")
	bool bCaptureLogTail;

	/** How much of the tail of the editor log to attach. */
	UPROPERTY(EditAnywhere, Config, Category = "Bug Report", meta = (ClampMin = "4", ClampMax = "1024", EditCondition = "bCaptureLogTail"))
	int32 LogTailKilobytes;

	/** List the selected actors, with class and location, in the captured context. Capped at 20. */
	UPROPERTY(EditAnywhere, Config, Category = "Bug Report")
	bool bCaptureSelectedActors;

	/** Record the camera transform — the player camera during play, the viewport camera otherwise. */
	UPROPERTY(EditAnywhere, Config, Category = "Bug Report")
	bool bCaptureCameraTransform;

	/** Include the git branch and short SHA of the project repository. */
	UPROPERTY(EditAnywhere, Config, Category = "Bug Report")
	bool bCaptureSourceControlRevision;

	/** Per-request timeout. Raise it if large Confluence pages or attachment uploads time out. */
	UPROPERTY(EditAnywhere, Config, Category = "Network", meta = (ClampMin = "5", ClampMax = "120"))
	int32 RequestTimeoutSeconds;

	/** Retry attempts for rate-limited and transient server failures. Non-retryable errors never retry. */
	UPROPERTY(EditAnywhere, Config, Category = "Network", meta = (ClampMin = "0", ClampMax = "5"))
	int32 MaxRetries;

	/** SiteUrl with whitespace and any trailing slash removed, and https:// added when no scheme is present. */
	FString GetNormalizedSiteUrl() const;

	/** <site>/rest/api/3 — empty when SiteUrl is not set. */
	FString GetJiraApiBaseUrl() const;

	/** <site>/wiki/api/v2 — empty when SiteUrl is not set. */
	FString GetConfluenceApiBaseUrl() const;

	/** <site>/wiki/rest/api — Confluence search is still only available on v1. */
	FString GetConfluenceV1ApiBaseUrl() const;

	/** BugProjectKey when set, otherwise ProjectKey. */
	FString GetEffectiveBugProjectKey() const;

	/** True once a site URL is present. Credentials are checked separately by the client. */
	bool IsConfigured() const;

private:
	UPROPERTY(Transient)
	TArray<FString> CachedProjectKeys;

	UPROPERTY(Transient)
	TArray<FString> CachedIssueTypeNames;

	UPROPERTY(Transient)
	TArray<FString> CachedPriorityNames;

	UPROPERTY(Transient)
	TArray<FString> CachedSpaceKeys;
};
