// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESteamBlueprintLibrary.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamSettings.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"

namespace ESteamBlueprintLibraryPrivate
{
	static FString TryGetAuthToken(FName SubsystemName, int32 LocalUserNum)
	{
		IOnlineSubsystem* Subsystem = SubsystemName.IsNone()
			? IOnlineSubsystem::Get()
			: IOnlineSubsystem::Get(SubsystemName);
		if (!Subsystem)
		{
			return FString();
		}

		const IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
		return Identity.IsValid() ? Identity->GetAuthToken(LocalUserNum) : FString();
	}
}

bool UESteamBlueprintLibrary::IsSteamAvailable()
{
	return FExtendedSteamSharedModule::IsModuleAvailable()
		&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();
}

bool UESteamBlueprintLibrary::IsSteamClientRunning()
{
	return FExtendedSteamSharedModule::IsModuleAvailable()
		&& FExtendedSteamSharedModule::Get().IsSteamClientRunning();
}

int32 UESteamBlueprintLibrary::GetConfiguredAppId()
{
	return UESteamSettings::Get()->SteamAppId;
}

FString UESteamBlueprintLibrary::SteamIdToString(const FESteamId& SteamId)
{
	return SteamId.ToString();
}

FESteamId UESteamBlueprintLibrary::SteamIdFromString(const FString& SteamIdString)
{
	return FESteamId::FromString(SteamIdString);
}

bool UESteamBlueprintLibrary::IsValidSteamId(const FESteamId& SteamId)
{
	return SteamId.IsValid();
}

FString UESteamBlueprintLibrary::GetSteamAuthTicket(int32 LocalUserNum)
{
	// Default OSS first (project DefaultPlatformService), then common Steam providers.
	if (const FString Token = ESteamBlueprintLibraryPrivate::TryGetAuthToken(NAME_None, LocalUserNum); !Token.IsEmpty())
	{
		return Token;
	}
	if (const FString Token = ESteamBlueprintLibraryPrivate::TryGetAuthToken(TEXT("Steam"), LocalUserNum); !Token.IsEmpty())
	{
		return Token;
	}
	if (const FString Token = ESteamBlueprintLibraryPrivate::TryGetAuthToken(TEXT("ExtendedSteam"), LocalUserNum); !Token.IsEmpty())
	{
		return Token;
	}

	return FString();
}
