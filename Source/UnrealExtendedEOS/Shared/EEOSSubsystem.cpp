// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSSubsystem.h"
#include "EEOSSettings.h"
#include "OnlineSubsystem.h"
#include "UnrealExtendedEOS.h"
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
	if (!bHasTriedCaching)
	{
		bHasTriedCaching = true;
		CachedEOSSubsystem = IOnlineSubsystem::Get(EOS_SUBSYSTEM);
		if (!CachedEOSSubsystem)
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EOS OnlineSubsystem is not available. Make sure OnlineSubsystemEOS is configured."));
		}
	}
	return CachedEOSSubsystem;
}

EOS_HPlatform UEEOSSubsystem::GetPlatformHandle() const
{
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

