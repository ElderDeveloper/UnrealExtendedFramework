// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ESteamSubsystem.generated.h"

class IOnlineSubsystem;
class UESteamSettings;

/**
 * Abstract base for all Extended Steam feature subsystems.
 * Mirrors the UEEOSSubsystem pattern from UnrealExtendedEOS.
 */
UCLASS(Abstract)
class EXTENDEDSTEAMSHARED_API UESteamSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** True when the Steam client API is initialized and usable. */
	UFUNCTION(BlueprintPure, Category = "Steam")
	bool IsSteamAvailable() const;

protected:
	/**
	 * Called when the Steam client API becomes available — either immediately during Initialize
	 * (when Steam is already up) or later when the shared module finishes initialization.
	 * Register Steam callback listeners here.
	 */
	virtual void HandleSteamClientInitialized() {}

	/** Called when the Steam client API shuts down (and during Deinitialize while Steam is up). */
	virtual void HandleSteamClientShutdown() {}

	/**
	 * The Extended Steam online subsystem ("EXTENDEDSTEAM"), or null when it is not enabled.
	 * Not memoized on failure: the OSS may load after this subsystem initializes.
	 */
	IOnlineSubsystem* GetSteamOnlineSubsystem() const;

	const UESteamSettings* GetSteamSettings() const;

	/** Logs a standard "Steam unavailable" warning for the given call site. */
	void LogSteamUnavailable(const TCHAR* Context) const;

private:
	mutable IOnlineSubsystem* CachedOnlineSubsystem = nullptr;
	FDelegateHandle SteamInitializedHandle;
	FDelegateHandle SteamShutdownHandle;
};
