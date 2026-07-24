// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Shared/EEOSTypes.h"
#include "EEOSSubsystem.generated.h"

// Forward declare EOS SDK platform handle
typedef struct EOS_PlatformHandle* EOS_HPlatform;

class UEEOSSettings;
class IOnlineSubsystem;

/**
 * Base class for all Extended EOS subsystems.
 * Provides common access to the EOS Online Subsystem, settings, and the raw EOS SDK platform handle.
 */
UCLASS(Abstract)
class EXTENDEDEOSSHARED_API UEEOSSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Check if the EOS subsystem is available and ready */
	UFUNCTION(BlueprintPure, Category = "EOS")
	bool IsEOSAvailable() const;

	/**
	 * True when the process-wide EOS OSS creation budget is exhausted and no instance exists —
	 * the platform cannot be created this session (invalid credentials/config). Retry loops
	 * should stop polling when this returns true.
	 */
	static bool IsEOSCreationExhausted();

protected:

	/** Get the EOS Online Subsystem (may return nullptr if not configured) */
	IOnlineSubsystem* GetEOSOnlineSubsystem() const;

	/** Get the raw EOS SDK platform handle for direct SDK calls (may return nullptr) */
	EOS_HPlatform GetPlatformHandle() const;

	/** Get the EOS Settings */
	const UEEOSSettings* GetEOSSettings() const;

	/** Log a warning if EOS is not available */
	void LogEOSUnavailable(const FString& FunctionName) const;

private:

	/** Cached pointer to the EOS Online Subsystem (only set on a successful lookup) */
	mutable IOnlineSubsystem* CachedEOSSubsystem = nullptr;
	/** Whether the "EOS unavailable" warning has been logged (once per session) */
	mutable bool bHasTriedCaching = false;
};
