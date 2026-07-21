// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Core/OnlineSubsystemExtendedSteam.h"
#include "Core/OnlineSubsystemExtendedSteamPackage.h"

#include "Modules/ModuleManager.h"
#include "OnlineSubsystemModule.h"

/**
 * Factory the OnlineSubsystem module calls to create "EXTENDEDSTEAM" instances.
 *
 * Singleton semantics, like the platform OSS factories: the Steam client is a per-process
 * resource, so only one live subsystem instance is allowed. Repeat creation requests (e.g.
 * additional PIE instances) log a warning and yield null.
 */
class FOnlineFactoryExtendedSteam : public IOnlineFactory
{
public:
	FOnlineFactoryExtendedSteam() = default;

	virtual ~FOnlineFactoryExtendedSteam()
	{
		DestroySubsystem();
	}

	//~ Begin IOnlineFactory
	virtual IOnlineSubsystemPtr CreateSubsystem(FName InstanceName) override
	{
		if (ExtendedSteamSingleton.IsValid())
		{
			UE_LOG(LogExtendedSteam, Warning,
				TEXT("Can't create more than one instance of the Extended Steam online subsystem (existing: %s, requested: %s)"),
				*ExtendedSteamSingleton->GetInstanceName().ToString(), *InstanceName.ToString());
			return nullptr;
		}

		ExtendedSteamSingleton = MakeShared<FOnlineSubsystemExtendedSteam, ESPMode::ThreadSafe>(InstanceName);
		if (ExtendedSteamSingleton->IsEnabled())
		{
			if (!ExtendedSteamSingleton->Init())
			{
				UE_LOG(LogExtendedSteam, Warning, TEXT("Extended Steam online subsystem failed to initialize; instance %s destroyed"),
					*InstanceName.ToString());
				DestroySubsystem();
			}
		}
		else
		{
			UE_LOG(LogExtendedSteam, Log, TEXT("Extended Steam online subsystem is disabled by config; instance %s destroyed"),
				*InstanceName.ToString());
			DestroySubsystem();
		}

		return ExtendedSteamSingleton;
	}
	//~ End IOnlineFactory

	/** Shuts down and releases the singleton (safe when none exists). */
	void DestroySubsystem()
	{
		if (ExtendedSteamSingleton.IsValid())
		{
			ExtendedSteamSingleton->Shutdown();
			ExtendedSteamSingleton.Reset();
		}
	}

private:
	FOnlineSubsystemExtendedSteamPtr ExtendedSteamSingleton;
};

/**
 * Registers the "EXTENDEDSTEAM" platform service with the engine's OnlineSubsystem module.
 * Loads at the Default phase (per .uplugin), safely after OnlineSubsystem (also Default,
 * pulled in as a plugin dependency) and after ExtendedSteamShared (PreDefault).
 */
class FOnlineSubsystemExtendedSteamModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		ExtendedSteamFactory = new FOnlineFactoryExtendedSteam();

		FOnlineSubsystemModule& OSSModule = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
		OSSModule.RegisterPlatformService(ESTEAM_SUBSYSTEM, ExtendedSteamFactory);

		UE_LOG(LogExtendedSteam, Log, TEXT("OnlineSubsystemExtendedSteam registered as platform service %s"), *ESTEAM_SUBSYSTEM.ToString());
	}

	virtual void ShutdownModule() override
	{
		if (FModuleManager::Get().IsModuleLoaded("OnlineSubsystem"))
		{
			FOnlineSubsystemModule& OSSModule = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
			OSSModule.UnregisterPlatformService(ESTEAM_SUBSYSTEM);
		}

		if (ExtendedSteamFactory != nullptr)
		{
			ExtendedSteamFactory->DestroySubsystem();
			delete ExtendedSteamFactory;
			ExtendedSteamFactory = nullptr;
		}
	}

private:
	FOnlineFactoryExtendedSteam* ExtendedSteamFactory = nullptr;
};

IMPLEMENT_MODULE(FOnlineSubsystemExtendedSteamModule, OnlineSubsystemExtendedSteam)
