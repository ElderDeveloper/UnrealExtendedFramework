// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "SocketSubsystem.h"
#include "UObject/UObjectGlobals.h"
#include "NetDriver/ExtendedSteamSocketsNetDriver.h"

#if WITH_DEV_AUTOMATION_TESTS

// Registration is opt-in ([OnlineSubsystemExtendedSteam] bUseSteamNetworking, default false) and also
// requires the Steam client to be up, so in a normal test environment the subsystem is NOT registered.
// These tests assert the safe/default posture and that all UClasses resolve; they never open a real
// connection, so they stay non-crashing in every environment (SDK present or not, Steam up or not).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamSocketsRegistrationTest,
	"UnrealExtendedSteam.Sockets.Registration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamSocketsRegistrationTest::RunTest(const FString& Parameters)
{
	// The module must be loadable without crashing.
	const bool bModuleLoaded = FModuleManager::Get().IsModuleLoaded(TEXT("ExtendedSteamSockets"))
		|| FModuleManager::Get().LoadModule(TEXT("ExtendedSteamSockets")) != nullptr;
	TestTrue(TEXT("ExtendedSteamSockets module is loaded"), bModuleLoaded);

	// The net driver UClass must exist (its header is UHT-processed by this Runtime module).
	TestNotNull(TEXT("UExtendedSteamSocketsNetDriver UClass exists"), UExtendedSteamSocketsNetDriver::StaticClass());

	// Read the opt-in flag; only when it is on AND Steam is up would the subsystem be registered.
	bool bUseSteamNetworking = false;
	if (GConfig != nullptr)
	{
		GConfig->GetBool(TEXT("OnlineSubsystemExtendedSteam"), TEXT("bUseSteamNetworking"), bUseSteamNetworking, GEngineIni);
	}

	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(FName(TEXT("ExtendedSteamSockets")));

	if (!bUseSteamNetworking)
	{
		// Default configuration: the subsystem must not be registered.
		TestNull(TEXT("ExtendedSteamSockets subsystem is not registered when opt-in flag is off"), Subsystem);
	}
	else if (Subsystem != nullptr)
	{
		// If it did register (flag on + Steam up), sanity-check its identity.
		TestEqual(TEXT("Registered subsystem reports its API name"),
			FString(Subsystem->GetSocketAPIName()), FString(TEXT("ExtendedSteamSockets")));
	}

	return true;
}

// The net driver + net connection UClasses must resolve and produce valid CDOs. This exercises the
// UHT-generated reflection for both transport classes without instantiating a live transport.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamSocketsClassesTest,
	"UnrealExtendedSteam.Sockets.Classes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamSocketsClassesTest::RunTest(const FString& Parameters)
{
	// Both transport UClasses resolve.
	UClass* NetDriverClass = UExtendedSteamSocketsNetDriver::StaticClass();
	UClass* NetConnectionClass = UExtendedSteamSocketsNetConnection::StaticClass();
	TestNotNull(TEXT("UExtendedSteamSocketsNetDriver UClass resolves"), NetDriverClass);
	TestNotNull(TEXT("UExtendedSteamSocketsNetConnection UClass resolves"), NetConnectionClass);

	// Their default objects construct without crashing.
	if (NetDriverClass != nullptr)
	{
		TestNotNull(TEXT("Net driver CDO exists"), NetDriverClass->GetDefaultObject());
	}
	if (NetConnectionClass != nullptr)
	{
		TestNotNull(TEXT("Net connection CDO exists"), NetConnectionClass->GetDefaultObject());
	}

	return true;
}

// When the opt-in flag is OFF (the default), the subsystem must not be registered under either its
// FName or its net driver's lookup. This locks in the safe default so the experimental transport can
// never become live simply by loading the module.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamSocketsDisabledByDefaultTest,
	"UnrealExtendedSteam.Sockets.DisabledByDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamSocketsDisabledByDefaultTest::RunTest(const FString& Parameters)
{
	bool bUseSteamNetworking = false;
	if (GConfig != nullptr)
	{
		GConfig->GetBool(TEXT("OnlineSubsystemExtendedSteam"), TEXT("bUseSteamNetworking"), bUseSteamNetworking, GEngineIni);
	}

	// This test only asserts the negative case; when the flag is on the Registration test covers it.
	if (!bUseSteamNetworking)
	{
		TestNull(TEXT("Subsystem is not registered while bUseSteamNetworking is off"),
			ISocketSubsystem::Get(FName(TEXT("ExtendedSteamSockets"))));

		// The net driver reports itself unavailable without a registered subsystem, so it can never be
		// silently selected as a live GameNetDriver in the default configuration.
		if (UExtendedSteamSocketsNetDriver* NetDriverCDO = GetMutableDefault<UExtendedSteamSocketsNetDriver>())
		{
			TestFalse(TEXT("Net driver is unavailable when the subsystem is not registered"), NetDriverCDO->IsAvailable());
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
