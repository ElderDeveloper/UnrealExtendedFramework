// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFAnalyticsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFEventLogged, const FEPFResult&, Result);

/**
 * Logs PlayStream analytics events — replaces GameAnalytics.
 * Events appear in the PlayFab dashboard and feed into segmentation, A/B testing, and reports.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFAnalyticsSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Log a single player event */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void LogEvent(const FString& EventName, const TMap<FString, FString>& Body);

	/** Log an event with no custom body (just the event name) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void LogSimpleEvent(const FString& EventName);

	/** Log a batch of events */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void LogEvents(const TArray<FEPFAnalyticsEvent>& Events);

	/** Log a character-scoped event */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void LogCharacterEvent(const FString& CharacterId, const FString& EventName, const TMap<FString, FString>& Body);

	/** Log a title-scoped event (global, not player-specific) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void LogTitleEvent(const FString& EventName, const TMap<FString, FString>& Body);

	/** Report device information to PlayFab (OS, device model, etc.) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void ReportDeviceInfo();

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the total number of events logged this session */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Analytics")
	int32 GetEventsLoggedCount() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Analytics")
	FOnEPFEventLogged OnEventLogged;

private:

	int32 EventsLoggedCount = 0;
};
