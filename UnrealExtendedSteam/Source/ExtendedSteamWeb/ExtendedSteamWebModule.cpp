// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Core/ESteamWebLog.h"

/** Steam Web API client, settings, and per-interface subsystems. */
class FExtendedSteamWebModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogExtendedSteamWeb, Verbose, TEXT("ExtendedSteamWeb module started"));
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FExtendedSteamWebModule, ExtendedSteamWeb)
