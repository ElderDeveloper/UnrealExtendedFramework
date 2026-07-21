// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "ESteamPublishSettings.generated.h"

/**
 * One depot mapping for a SteamPipe build. Mirrors a depot_build_<id>.vdf FileMapping block.
 */
USTRUCT(BlueprintType)
struct FESteamPublishDepot
{
	GENERATED_BODY()

	/** Depot ID as assigned in your Steamworks app configuration. */
	UPROPERTY(EditAnywhere, Category = "Depot")
	int32 DepotId = 0;

	/** Source path relative to ContentRoot. "*" includes everything. */
	UPROPERTY(EditAnywhere, Category = "Depot")
	FString LocalPath = TEXT("*");

	/** Install path inside the game folder on the client. "." is the install root. */
	UPROPERTY(EditAnywhere, Category = "Depot")
	FString DepotPath = TEXT(".");

	/** Include matching files in subdirectories of LocalPath. */
	UPROPERTY(EditAnywhere, Category = "Depot")
	bool bRecursive = true;
};

/**
 * Project Settings -> Extended Framework -> Extended Steam Publish.
 *
 * Non-secret build configuration is stored here (committed to DefaultEditor.ini). Credentials are NOT
 * stored here — the Username/Password fields are transient UI mirrors of the version-control-ignored
 * credentials file (see FESteamPublishCredentials). Use the action buttons to prepare tools, secure
 * credentials for version control, and upload a build via steamcmd.
 */
UCLASS(Config = Editor, DefaultConfig, meta = (DisplayName = "Extended Steam Publish"))
class EXTENDEDSTEAMEDITOR_API UESteamPublishSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UESteamPublishSettings();

	//~ UDeveloperSettings
	virtual FName GetCategoryName() const override;
	virtual void PostInitProperties() override;

	static UESteamPublishSettings* GetMutable() { return GetMutableDefault<UESteamPublishSettings>(); }
	static const UESteamPublishSettings* Get() { return GetDefault<UESteamPublishSettings>(); }

	// ---------------------------------------------------------------------------------------------
	// Build configuration
	// ---------------------------------------------------------------------------------------------

	/** Steamworks App ID for the game you are uploading. 480 is Valve's public test app (Spacewar). */
	UPROPERTY(Config, EditAnywhere, Category = "Steam Publish|Build", meta = (DisplayName = "Steam App ID", ClampMin = "1"))
	int32 AppId = 480;

	/** Internal description recorded on the Steamworks build for this upload. */
	UPROPERTY(Config, EditAnywhere, Category = "Steam Publish|Build")
	FString BuildDescription = TEXT("Editor build");

	/** Folder containing the packaged build to upload (the ContentRoot). Absolute, or relative to the project. */
	UPROPERTY(Config, EditAnywhere, Category = "Steam Publish|Build")
	FDirectoryPath ContentRoot;

	/** Depot mappings. Most games ship a single depot with LocalPath "*". */
	UPROPERTY(Config, EditAnywhere, Category = "Steam Publish|Build")
	TArray<FESteamPublishDepot> Depots;

	/** Beta branch to set live automatically after upload. Leave empty to upload without setting a branch live. */
	UPROPERTY(Config, EditAnywhere, Category = "Steam Publish|Build", meta = (DisplayName = "Set Live Branch (optional)"))
	FString SetLiveBranch;

	/** Preview only: validates and reports what WOULD upload without pushing anything to Steam. Safe default. */
	UPROPERTY(Config, EditAnywhere, Category = "Steam Publish|Build", meta = (DisplayName = "Preview Build (nothing is uploaded)"))
	bool bPreviewBuild = true;

	// ---------------------------------------------------------------------------------------------
	// Tooling
	// ---------------------------------------------------------------------------------------------

	/** Working folder where ContentBuilder (steamcmd + generated scripts + logs) is prepared. */
	UPROPERTY(Config, EditAnywhere, Category = "Steam Publish|Tooling", meta = (DisplayName = "ContentBuilder Directory"))
	FDirectoryPath ContentBuilderDirectory;

	// ---------------------------------------------------------------------------------------------
	// Credentials (transient — mirrored to the ignored credentials file, never to committed config)
	// ---------------------------------------------------------------------------------------------

	/** Steam account used for uploads. Prefer a dedicated Steamworks builder account over a personal one. */
	UPROPERTY(EditAnywhere, Transient, Category = "Steam Publish|Credentials")
	FString SteamUsername;

	/** Password for the upload account. Stored AES-256 encrypted in the ignored credentials file, never in committed config. */
	UPROPERTY(EditAnywhere, Transient, Category = "Steam Publish|Credentials", meta = (PasswordField = true))
	FString SteamPassword;

	// ---------------------------------------------------------------------------------------------
	// Actions
	// ---------------------------------------------------------------------------------------------

	/** Copy the bundled ContentBuilder (steamcmd + templates) out of the plugin SDK into the working folder. */
	UFUNCTION(CallInEditor, Category = "Steam Publish|Actions", meta = (DisplayName = "1. Extract ContentBuilder Tools"))
	void ExtractContentBuilderTools();

	/** Add the credentials/tools folder to the project's .gitignore / .svnignore / .p4ignore (project check). */
	UFUNCTION(CallInEditor, Category = "Steam Publish|Actions", meta = (DisplayName = "2. Secure Credentials for Version Control"))
	void SecureCredentialsForVersionControl();

	/** Write the Username/Password above to the version-control-ignored credentials file. */
	UFUNCTION(CallInEditor, Category = "Steam Publish|Actions", meta = (DisplayName = "3. Save Credentials"))
	void SaveCredentials();

	/** One-time: launch steamcmd in a console so you can enter your Steam Guard code and authorize this machine. */
	UFUNCTION(CallInEditor, Category = "Steam Publish|Actions", meta = (DisplayName = "Login & Authorize (one-time Steam Guard)"))
	void LoginAndAuthorize();

	/** Generate the VDF scripts from these settings and run steamcmd to upload the build. */
	UFUNCTION(CallInEditor, Category = "Steam Publish|Actions", meta = (DisplayName = "4. Publish Build"))
	void PublishBuild();
};
