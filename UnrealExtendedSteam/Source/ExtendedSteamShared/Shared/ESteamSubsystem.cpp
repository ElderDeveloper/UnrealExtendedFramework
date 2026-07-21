// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESteamSubsystem.h"
#include "ESteamLog.h"
#include "ESteamSettings.h"
#include "ESteamTypes.h"
#include "ExtendedSteamSharedModule.h"
#include "OnlineSubsystem.h"

void UESteamSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UESteamSettings::Get()->bVerboseLogging)
	{
		UE_LOG(LogExtendedSteam, Verbose, TEXT("%s initialized"), *GetClass()->GetName());
	}

	if (FExtendedSteamSharedModule::IsModuleAvailable())
	{
		FExtendedSteamSharedModule& Shared = FExtendedSteamSharedModule::Get();
		SteamInitializedHandle = Shared.OnSteamClientInitialized.AddUObject(this, &UESteamSubsystem::HandleSteamClientInitialized);
		SteamShutdownHandle = Shared.OnSteamClientShutdown.AddUObject(this, &UESteamSubsystem::HandleSteamClientShutdown);

		if (Shared.IsSteamClientInitialized())
		{
			HandleSteamClientInitialized();
		}
	}
}

void UESteamSubsystem::Deinitialize()
{
	if (FExtendedSteamSharedModule::IsModuleAvailable())
	{
		FExtendedSteamSharedModule& Shared = FExtendedSteamSharedModule::Get();
		Shared.OnSteamClientInitialized.Remove(SteamInitializedHandle);
		Shared.OnSteamClientShutdown.Remove(SteamShutdownHandle);

		if (Shared.IsSteamClientInitialized())
		{
			HandleSteamClientShutdown();
		}
	}

	CachedOnlineSubsystem = nullptr;
	Super::Deinitialize();
}

bool UESteamSubsystem::IsSteamAvailable() const
{
	return FExtendedSteamSharedModule::IsModuleAvailable()
		&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();
}

IOnlineSubsystem* UESteamSubsystem::GetSteamOnlineSubsystem() const
{
	if (!CachedOnlineSubsystem)
	{
		CachedOnlineSubsystem = IOnlineSubsystem::Get(ESTEAM_SUBSYSTEM);
	}
	return CachedOnlineSubsystem;
}

const UESteamSettings* UESteamSubsystem::GetSteamSettings() const
{
	return UESteamSettings::Get();
}

void UESteamSubsystem::LogSteamUnavailable(const TCHAR* Context) const
{
	UE_LOG(LogExtendedSteam, Warning, TEXT("%s: Steam is not available (client not running or SDK not initialized)"), Context);
}
