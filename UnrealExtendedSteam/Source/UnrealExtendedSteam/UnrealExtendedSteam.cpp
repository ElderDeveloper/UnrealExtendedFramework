// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedSteam.h"
#include "Shared/ESteamLog.h"

void FUnrealExtendedSteamModule::StartupModule()
{
	UE_LOG(LogExtendedSteam, Verbose, TEXT("UnrealExtendedSteam module started"));
}

void FUnrealExtendedSteamModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FUnrealExtendedSteamModule, UnrealExtendedSteam)
