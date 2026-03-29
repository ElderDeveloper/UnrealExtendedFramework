// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EEOSTypes.h"
#include "EEOSSettings.generated.h"


/**
 * Developer settings for Epic Online Services configuration.
 * These settings appear in Project Settings → Plugins → Extended EOS.
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Extended EOS"))
class UNREALEXTENDEDEOS_API UEEOSSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UEEOSSettings();

	// ── Application Credentials ──────────────────────────────────────────────

	/** The Product ID from the EOS Developer Portal */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Credentials",
		meta = (DisplayName = "Product ID"))
	FString ProductId;

	/** The Sandbox ID (environment) from the EOS Developer Portal */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Credentials",
		meta = (DisplayName = "Sandbox ID"))
	FString SandboxId;

	/** The Deployment ID from the EOS Developer Portal */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Credentials",
		meta = (DisplayName = "Deployment ID"))
	FString DeploymentId;

	/** The Client ID for the application from the EOS Developer Portal */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Credentials",
		meta = (DisplayName = "Client ID"))
	FString ClientId;

	/** The Client Secret for the application from the EOS Developer Portal */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Credentials",
		meta = (DisplayName = "Client Secret"))
	FString ClientSecret;

	/** Encryption key used for player data storage (64 hex characters) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Credentials",
		meta = (DisplayName = "Encryption Key"))
	FString EncryptionKey;


	// ── Auth Settings ────────────────────────────────────────────────────────

	/** Default login type to use when calling Login without specifying a type */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Auth",
		meta = (DisplayName = "Default Login Type"))
	EEOSLoginType DefaultLoginType = EEOSLoginType::AccountPortal;

	/** If true, automatically attempt persistent auth login on subsystem initialization */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Auth",
		meta = (DisplayName = "Auto Login on Start"))
	bool bAutoLoginOnStart = false;

	/** If true, automatically do EOS Connect login after successful Auth login (or on start for platform logins) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Auth",
		meta = (DisplayName = "Auto Connect Login on Start"))
	bool bAutoConnectLoginOnStart = false;

	/** Default platform for EOS Connect login (Steam, PSN, Xbox, DeviceId, etc.) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Auth",
		meta = (DisplayName = "Default Connect Login Type"))
	EEOSConnectLoginType DefaultConnectLoginType = EEOSConnectLoginType::DeviceId;

	/** If true, automatically create a new ProductUser when Connect login returns EOS_InvalidUser (first-time player) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Auth",
		meta = (DisplayName = "Auto Create Product User"))
	bool bAutoCreateProductUser = true;


	// ── Session Settings ─────────────────────────────────────────────────────

	/** Default maximum players per session */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Sessions",
		meta = (DisplayName = "Default Max Players", ClampMin = 2, ClampMax = 64))
	int32 DefaultMaxPlayers = 4;

	/** Default session name for quick-create */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Sessions",
		meta = (DisplayName = "Default Session Name"))
	FString DefaultSessionName = TEXT("DefaultGame");

	/** If true, sessions are advertised publicly by default */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Sessions",
		meta = (DisplayName = "Public Sessions by Default"))
	bool bPublicSessionsByDefault = true;

	/** If true, use lobbies instead of sessions by default */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Sessions",
		meta = (DisplayName = "Use Lobbies by Default"))
	bool bUseLobbiesByDefault = false;

	/** Preferred deployment region (empty = auto) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Sessions",
		meta = (DisplayName = "Preferred Region"))
	EEOSRegionInfo PreferredRegion = EEOSRegionInfo::NoSelection;


	// ── Voice Settings ───────────────────────────────────────────────────────

	/** Default output (speaker) volume (0.0 to 1.0) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Voice",
		meta = (DisplayName = "Default Output Volume", ClampMin = 0.0, ClampMax = 1.0))
	float DefaultOutputVolume = 1.0f;

	/** Default input (microphone) volume (0.0 to 1.0) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Voice",
		meta = (DisplayName = "Default Input Volume", ClampMin = 0.0, ClampMax = 1.0))
	float DefaultInputVolume = 1.0f;

	/** If true, microphone starts muted */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Voice",
		meta = (DisplayName = "Start Muted"))
	bool bStartMuted = false;


	// ── P2P Settings ─────────────────────────────────────────────────────────

	/** Default relay control mode for P2P: NoRelays, AllowRelays, ForceRelays */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "P2P",
		meta = (DisplayName = "Default Relay Mode", EditCondition = "bEnableP2P"))
	FString DefaultRelayMode = TEXT("AllowRelays");


	// ── Developer / Debug ────────────────────────────────────────────────────

	/** Host address for the Developer Auth Tool (e.g. localhost:6547) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Developer",
		meta = (DisplayName = "Dev Auth Tool Address", EditCondition = "DefaultLoginType == EEOSLoginType::Developer"))
	FString DevAuthToolAddress = TEXT("localhost:6547");

	/** Credential name configured in the Developer Auth Tool */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Developer",
		meta = (DisplayName = "Dev Auth Credential Name", EditCondition = "DefaultLoginType == EEOSLoginType::Developer"))
	FString DevAuthCredentialName;

	/** If true, log verbose EOS SDK output to the output log */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Developer",
		meta = (DisplayName = "Enable Verbose Logging"))
	bool bEnableVerboseLogging = false;


	// ── Feature Toggles ──────────────────────────────────────────────────────

	/** Enable Anti-Cheat functionality */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Features",
		meta = (DisplayName = "Enable Anti-Cheat"))
	bool bEnableAntiCheat = false;

	/** Enable Voice Chat functionality */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Features",
		meta = (DisplayName = "Enable Voice Chat"))
	bool bEnableVoiceChat = false;

	/** Enable P2P Networking functionality */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Features",
		meta = (DisplayName = "Enable P2P"))
	bool bEnableP2P = false;

	/** Enable Leaderboard features */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Features",
		meta = (DisplayName = "Enable Leaderboards"))
	bool bEnableLeaderboards = true;

	/** Enable Achievement tracking */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Features",
		meta = (DisplayName = "Enable Achievements"))
	bool bEnableAchievements = true;


	// ── Helpers ──────────────────────────────────────────────────────────────

	/** Get the singleton settings instance */
	static const UEEOSSettings* Get()
	{
		return GetDefault<UEEOSSettings>();
	}

	/** Category path for Project Settings UI */
	virtual FName GetCategoryName() const override { return FName(TEXT("Extended Framework")); }
};
