// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFContentDeliverySubsystem.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFContentUrlReceived, const FEPFResult&, Result, const FString&, DownloadUrl);

/**
 * Content Delivery — download server-hosted files via PlayFab CDN.
 * Useful for patches, DLC manifests, config files, or any server-hosted assets.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFContentDeliverySubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/**
	 * Get a download URL for content hosted on PlayFab CDN.
	 * @param Key              The content key (file path in PlayFab).
	 * @param bThruCDN         If true, returns a CDN URL. If false, returns origin URL.
	 * @param HttpMethod       HTTP method hint for the download (GET by default).
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|ContentDelivery")
	void GetContentDownloadUrl(const FString& Key, bool bThruCDN = true, const FString& HttpMethod = TEXT("GET"));

	/** Download content as a string (uses GetContentDownloadUrl internally, then fetches) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|ContentDelivery")
	void DownloadContentAsString(const FString& Key);

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|ContentDelivery")
	FOnEPFContentUrlReceived OnContentUrlReceived;

	/** Fired when DownloadContentAsString completes; the string is the file content */
	UPROPERTY(BlueprintAssignable, Category = "PlayFab|ContentDelivery")
	FOnEPFContentUrlReceived OnContentDownloaded;
};
