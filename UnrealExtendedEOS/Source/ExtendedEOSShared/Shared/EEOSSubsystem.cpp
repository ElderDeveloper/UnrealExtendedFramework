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

IOnlineSubsystem* UEEOSSubsystem::GetEOSOnlineSubsystem() const
{
	// Only a successful lookup is cached: the OSS may not be loaded yet when the first caller
	// asks (subsystem Initialize order is undefined), so a null result must be retried, not
	// remembered for the rest of the session.
	if (!CachedEOSSubsystem)
	{
		CachedEOSSubsystem = IOnlineSubsystem::Get(EOS_SUBSYSTEM);
		if (!CachedEOSSubsystem && !bHasTriedCaching)
		{
			bHasTriedCaching = true;
			UE_LOG(LogExtendedEOS, Warning, TEXT("EOS OnlineSubsystem is not available. Make sure OnlineSubsystemEOS is configured."));
		}
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

