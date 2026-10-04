// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Shared/EEOSTypes.h"
#include "Shared/EEOSOperationTypes.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Containers/Ticker.h"
#include "EEOSSubsystem.generated.h"

// Forward declare EOS SDK platform handle
typedef struct EOS_PlatformHandle* EOS_HPlatform;

class UEEOSSettings;
class IOnlineSubsystem;
class IEOSPlatformHandle;
class UEEOSSubsystem;

/** Retained native ownership for an async request. Never retain a GameInstance in SDK contexts. */
struct EXTENDEDEOSSHARED_API FEEOSRequestContext
{
	TSharedPtr<IEOSPlatformHandle, ESPMode::ThreadSafe> Platform;
	IOnlineIdentityPtr Identity;
	FName Instance;
	FString LocalId;
	int64 Generation = 0;
	int64 PlatformGeneration = 0;
};

/** Keeps a terminal result stable throughout nested legacy/detailed delegate dispatch. */
class EXTENDEDEOSSHARED_API FEEOSOutcomeDispatchScope
{
public:
	FEEOSOutcomeDispatchScope(UEEOSSubsystem* Owner, const FEEOSOperationOutcome& Outcome);
	~FEEOSOutcomeDispatchScope();
	FEEOSOutcomeDispatchScope(const FEEOSOutcomeDispatchScope&) = delete;
	FEEOSOutcomeDispatchScope& operator=(const FEEOSOutcomeDispatchScope&) = delete;
private:
	UEEOSSubsystem* Owner;
};

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
	/** Existing-instance observation only; never initializes an OSS or prompts for login. */
	UFUNCTION(BlueprintPure, Category = "EOS|Diagnostics")
	FEEOSReadinessSnapshot GetEOSReadiness() const;
	FEEOSRequestContext CaptureEOSContext() const;
	/** Identity-changing login calls use false; ordinary SDK work must use the default. */
	bool IsEOSContextCurrent(const FEEOSRequestContext& Context, bool bRequireSameIdentity = true) const;
	void LogCallbackDisposition(FName Operation, int64 RequestId, const TCHAR* Disposition, int64 Generation = 0) const;

	/** Compatibility query for the default EOS instance's bounded creation budget.
	 * Subsystem retry loops use the owning-instance query so PIE instances stay independent.
	 * Exhaustion indicates repeated creation failure; it does not identify a credential error. */
	static bool IsEOSCreationExhausted();
	/** Terminal result, or the result being dispatched to the current callback. */
	UFUNCTION(BlueprintPure, Category = "EOS|Diagnostics")
	FEEOSOperationOutcome GetLastOperationOutcome(FName Operation) const;
	UFUNCTION(BlueprintPure, Category = "EOS|Diagnostics")
	FEEOSOperationOutcome GetActiveOperationOutcome(FName Operation) const;
	UFUNCTION(BlueprintPure, Category = "EOS|Diagnostics")
	FEEOSOperationOutcome GetLastOperationRejection() const { return LastRejection; }
	UPROPERTY(BlueprintAssignable, Category = "EOS|Diagnostics")
	FOnEEOSOperationOutcome OnOperationCompleted;
	UPROPERTY(BlueprintAssignable, Category = "EOS|Diagnostics")
	FOnEEOSOperationOutcome OnOperationRejected;

protected:

	/** Get the EOS Online Subsystem (may return nullptr if not configured) */
	IOnlineSubsystem* GetEOSOnlineSubsystem() const;
	IOnlineSubsystem* GetExistingEOSOnlineSubsystem() const;
	FName GetOwningEOSInstanceName() const;
	bool IsOwningEOSCreationExhausted() const;
	void BeginEOSShutdown() { bEOSShuttingDown = true; }
	TSharedPtr<IEOSPlatformHandle, ESPMode::ThreadSafe> GetOwningEOSPlatform() const;
	int64 BeginOperation(FName Operation, const FString& TargetId = FString(), int64 RequestId = 0);
	void SetOperationPhase(FName Operation, FName Phase);
	void TagOperationContext(FName Operation, const FString& OriginalId, int64 MembershipGeneration = 0,
		int64 SearchGeneration = 0, int64 ParentRequestId = 0);
	FEEOSOperationOutcome CompleteOperation(FName Operation, bool bSuccess, EEOSOperationCode Code, const FString& Message,
		const FString& CurrentId = FString(), const FString& NativeResult = FString(), EEOSResultSource Source = EEOSResultSource::Plugin,
		const FString& SDKResult = FString());
	void RejectOperation(FName Operation, EEOSOperationCode Code, const FString& Message, const FString& TargetId = FString());

	/** Get the raw EOS SDK platform handle for direct SDK calls (may return nullptr) */
	EOS_HPlatform GetPlatformHandle() const;

	/** Get the EOS Settings */
	const UEEOSSettings* GetEOSSettings() const;

	/** Log a warning if EOS is not available */
	void LogEOSUnavailable(const FString& FunctionName) const;

private:

	/** Cached pointer to the EOS Online Subsystem (only set on a successful lookup) */
	mutable IOnlineSubsystem* CachedEOSSubsystem = nullptr;
	mutable FName ResolvedEOSInstance;
	mutable FEEOSRequestContext ObservedContext;
	mutable ELoginStatus::Type ObservedLoginStatus = ELoginStatus::NotLoggedIn;
	mutable FDelegateHandle ContextStatusHandle;
	mutable uint32 ObservedCapabilities = 0;
	FTSTicker::FDelegateHandle ContextTicker;
	bool TickEOSContext(float DeltaTime);
	bool bEOSShuttingDown = false;
	TMap<FName, FEEOSOperationOutcome> OperationOutcomes;
	TMap<FName, FEEOSOperationOutcome> TerminalOutcomes;
	TArray<FEEOSOperationOutcome> DispatchOutcomes;
	friend class FEEOSOutcomeDispatchScope;
	TMap<FName, double> OperationStartTimes;
	FEEOSOperationOutcome LastRejection;
	/** Whether the "EOS unavailable" warning has been logged (once per session) */
	mutable bool bHasTriedCaching = false;
};
