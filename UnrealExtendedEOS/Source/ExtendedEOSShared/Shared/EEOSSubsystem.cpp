// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSSubsystem.h"
#include "EEOSSettings.h"
#include "OnlineSubsystem.h"
#include "Shared/EEOSLog.h"
#include "IEOSSDKManager.h"

void UEEOSSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogExtendedEOS, Verbose, TEXT("%s initialized"), *GetClass()->GetName());
}

void UEEOSSubsystem::Deinitialize()
{
	UE_LOG(LogExtendedEOS, Verbose, TEXT("%s deinitialized"), *GetClass()->GetName());
	CachedEOSSubsystem = nullptr;
	bHasTriedCaching = false;
	Super::Deinitialize();
}

bool UEEOSSubsystem::IsEOSAvailable() const
{
	return GetEOSOnlineSubsystem() != nullptr;
}

// Process-wide budget for EOS OSS CREATION attempts. Every IOnlineSubsystem::Get() call after a
// failed creation re-attempts the full EOS SDK platform boot (which fails again if the
// [/Script/OnlineSubsystemEOS.EOSSettings] credentials are invalid) — with 25 subsystems and
// retry tickers, an unbounded retry floods the log with SDK boot errors every second. A few real
// attempts cover late module load; after that the failure is config, not timing, and we latch.
static int32 GEOSCreateAttemptsRemaining = 3;
static bool GEOSCreateGiveUpLogged = false;

bool UEEOSSubsystem::IsEOSCreationExhausted()
{
	return GEOSCreateAttemptsRemaining <= 0 && !IOnlineSubsystem::DoesInstanceExist(EOS_SUBSYSTEM);
}

IOnlineSubsystem* UEEOSSubsystem::GetEOSOnlineSubsystem() const
{
	if (CachedEOSSubsystem)
	{
		return CachedEOSSubsystem;
	}

	// Cheap path: an instance that already exists can be fetched without risking a new SDK boot.
	if (IOnlineSubsystem::DoesInstanceExist(EOS_SUBSYSTEM))
	{
		CachedEOSSubsystem = IOnlineSubsystem::Get(EOS_SUBSYSTEM);
		return CachedEOSSubsystem;
	}

	if (GEOSCreateAttemptsRemaining <= 0)
	{
		if (!GEOSCreateGiveUpLogged)
		{
			GEOSCreateGiveUpLogged = true;
			UE_LOG(LogExtendedEOS, Error,
				TEXT("EOS platform failed to initialize after repeated attempts — giving up for this session. ")
				TEXT("Check the [/Script/OnlineSubsystemEOS.EOSSettings] Artifacts entry in DefaultEngine.ini ")
				TEXT("(ProductId/SandboxId/DeploymentId/ClientId/ClientSecret must be real values and the ")
				TEXT("EncryptionKey must be 64 hex digits — placeholders make EOS_Platform_Create reject the options)."));
		}
		return nullptr;
	}

	--GEOSCreateAttemptsRemaining;
	CachedEOSSubsystem = IOnlineSubsystem::Get(EOS_SUBSYSTEM);
	if (CachedEOSSubsystem)
	{
		GEOSCreateAttemptsRemaining = 3;	// healthy again — restore the budget for OSS restarts
	}
	else if (!bHasTriedCaching)
	{
		bHasTriedCaching = true;
		UE_LOG(LogExtendedEOS, Warning, TEXT("EOS OnlineSubsystem is not available. Make sure OnlineSubsystemEOS is configured."));
	}
	return CachedEOSSubsystem;
}

EOS_HPlatform UEEOSSubsystem::GetPlatformHandle() const
{
	// LIMITATION: returns the first active platform. Under multi-instance PIE the SDK manager
	// holds one platform per OSS instance and this may pick another instance's platform.
	// Raw-SDK calls are therefore only reliable in standalone / single-instance runs.
	if (IEOSSDKManager* SDKManager = IEOSSDKManager::Get())
	{
		TArray<IEOSPlatformHandlePtr> ActivePlatforms = SDKManager->GetActivePlatforms();
		if (ActivePlatforms.Num() > 0 && ActivePlatforms[0].IsValid())
		{
			return *ActivePlatforms[0];
		}
	}

	UE_LOG(LogExtendedEOS, Warning, TEXT("%s::GetPlatformHandle — EOS SDK platform handle not available"), *GetClass()->GetName());
	return nullptr;
}

const UEEOSSettings* UEEOSSubsystem::GetEOSSettings() const
{
	return UEEOSSettings::Get();
}

void UEEOSSubsystem::LogEOSUnavailable(const FString& FunctionName) const
{
	UE_LOG(LogExtendedEOS, Warning, TEXT("%s::%s — EOS is not available. Operation skipped."), *GetClass()->GetName(), *FunctionName);
}

