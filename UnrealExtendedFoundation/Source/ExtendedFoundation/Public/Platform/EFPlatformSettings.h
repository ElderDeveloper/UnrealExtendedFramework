// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Platform/EFPlatformCatalog.h"
#include "Platform/EFPlatformTypes.h"
#include "Settings/EFDeveloperSettings.h"
#include "EFPlatformSettings.generated.h"

/**
 * Project Settings > Extended Framework > Extended Platform: which store serves achievements,
 * stats, leaderboards, cloud storage and presence.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Extended Platform"))
class EXTENDEDFOUNDATION_API UEFPlatformSettings : public UEFDeveloperSettings
{
	GENERATED_BODY()

public:
	UEFPlatformSettings();

	static const UEFPlatformSettings* Get() { return GetDefault<UEFPlatformSettings>(); }

	/**
	 * Providers to try, first to last. A feature goes to the first provider that is registered (its
	 * plugin is enabled) and can serve it now (SDK running, user signed in). Registered providers
	 * that are not listed come after these, in the order they registered.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Providers")
	TArray<FName> ProviderOrder;

	/**
	 * A provider to use for a feature before ProviderOrder is consulted, e.g. PlayFab for
	 * Leaderboards while Steam serves the rest. When it cannot serve the feature now, ProviderOrder
	 * applies.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Providers")
	TMap<EEFPlatformFeature, FName> FeatureProviders;

	/** Maps the game's achievement, stat and leaderboard ids to each store's ids. Optional. */
	UPROPERTY(config, EditAnywhere, Category = "Catalog")
	TSoftObjectPtr<UEFPlatformCatalog> Catalog;

	/**
	 * Creates the built-in Local provider, which keeps achievements, stats and leaderboards in a
	 * file under Saved/ExtendedPlatform. It is last in ProviderOrder by default, so it serves only
	 * when no store can: in the editor, in builds without a store, or before a store has started.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Local Provider")
	bool bEnableLocalProvider = true;

	/**
	 * Lets the Local provider serve cloud storage by writing under Saved/ExtendedPlatform. Off by
	 * default, so a missing store never pretends to be a cloud; turn it on to test cloud saves offline.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Local Provider")
	bool bLocalProviderCloudStorage = false;

	/**
	 * In the editor, lets the Local provider serve a feature whose store is enabled but not running
	 * (Steam client closed, no EOS login), instead of queueing writes for that store. Packaged builds
	 * always wait for the store.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Local Provider")
	bool bLocalFallbackInEditor = true;

	/**
	 * Holds achievement, stat and leaderboard writes made while no provider can take them (for
	 * example before the user signs in) and sends them once one can. Each write's callback runs when
	 * it is finally sent.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Writes")
	bool bQueueWritesWhileUnavailable = true;

	/** Writes held at most; the oldest fails with QueueFull when a new one would exceed it. */
	UPROPERTY(config, EditAnywhere, Category = "Writes", meta = (ClampMin = "1", EditCondition = "bQueueWritesWhileUnavailable"))
	int32 MaxQueuedWrites = 256;

	/** Seconds between attempts to send queued writes. */
	UPROPERTY(config, EditAnywhere, Category = "Writes", meta = (ClampMin = "0.5", Units = "Seconds", EditCondition = "bQueueWritesWhileUnavailable"))
	float QueueRetryIntervalSeconds = 5.f;

	/** Broadcasts Extended.Platform.* messages (achievement unlocked, score submitted) on the message bus. */
	UPROPERTY(config, EditAnywhere, Category = "Messages")
	bool bBroadcastMessages = true;
};
