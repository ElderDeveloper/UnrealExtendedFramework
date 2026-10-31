// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedSteam.h"
#include "EFFoundationVersion.h"
#include "Shared/ESteamLog.h"

// 1.2: the platform layer (UEFPlatformProviderSubsystem) that UESteamPlatformProvider implements.
EF_REQUIRE_FOUNDATION_VERSION(1, 2, "UnrealExtendedSteam");

void FUnrealExtendedSteamModule::StartupModule()
{
	UE_LOG(LogExtendedSteam, Verbose, TEXT("UnrealExtendedSteam module started"));
}

void FUnrealExtendedSteamModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FUnrealExtendedSteamModule, UnrealExtendedSteam)
