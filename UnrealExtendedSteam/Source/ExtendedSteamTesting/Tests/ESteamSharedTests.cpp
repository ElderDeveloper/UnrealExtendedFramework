// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamSettings.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "Shared/ESteamBlueprintLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ESteamTestHelpers
{
	bool IsSteamClientInitialized()
	{
		return FExtendedSteamSharedModule::IsModuleAvailable()
			&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamSettingsDefaultsTest,
	"UnrealExtendedSteam.Settings.Defaults.Values",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	const UESteamSettings* Settings = UESteamSettings::Get();
	TestNotNull(TEXT("Settings CDO"), Settings);
	TestEqual(TEXT("Default app id is Spacewar (480)"), Settings->SteamAppId, 480);
	TestTrue(TEXT("Steam initializes on startup by default"), Settings->bInitializeSteamOnStartup);
	TestTrue(TEXT("Editor initialization on by default"), Settings->bInitializeSteamInEditor);
	TestFalse(TEXT("Relaunch in Steam off by default"), Settings->bRelaunchInSteam);
	TestEqual(TEXT("Default async timeout"), Settings->AsyncTaskTimeoutSeconds, 10.0f);
	TestFalse(TEXT("Verbose logging off by default"), Settings->bVerboseLogging);
	TestEqual(TEXT("Settings live under the shared Extended Framework category"),
		Settings->GetCategoryName(), FName(TEXT("Extended Framework")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamIdRoundTripTest,
	"UnrealExtendedSteam.Shared.SteamId.StringRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamIdRoundTripTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Default id is invalid"), FESteamId().IsValid());

	const FESteamId Id(76561197960287930ull);
	TestTrue(TEXT("Non-zero id is valid"), Id.IsValid());
	TestEqual(TEXT("ToString"), Id.ToString(), FString(TEXT("76561197960287930")));

	const FESteamId Parsed = FESteamId::FromString(TEXT("76561197960287930"));
	TestTrue(TEXT("Round trip equality"), Parsed == Id);

	TestFalse(TEXT("Garbage input parses to invalid id"), FESteamId::FromString(TEXT("not-a-steam-id")).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamAvailabilityConsistencyTest,
	"UnrealExtendedSteam.Shared.Availability.Consistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamAvailabilityConsistencyTest::RunTest(const FString& Parameters)
{
	// Steam may or may not be initialized depending on the machine and run mode
	// (auto-init is skipped for commandlets/unattended runs); what must always hold
	// is that every availability surface agrees with the shared module.
	const bool bInitialized = ESteamTestHelpers::IsSteamClientInitialized();

	// The invariant under test is consistency: every availability surface must agree with the
	// shared module, whether Steam is up or not. (We deliberately do NOT assert that unattended
	// runs stay offline: the shared module's *auto*-init skips unattended/commandlet runs, but an
	// on-demand consumer — e.g. the OSS interface's Init requesting the subsystem — can still bring
	// the Steam client up mid-process. That is valid behavior, so availability is not fixed here.)
	const UESteamSubsystem* SubsystemCDO = GetDefault<UESteamSubsystem>();
	TestNotNull(TEXT("Abstract base CDO exists"), SubsystemCDO);
	TestEqual(TEXT("Subsystem availability matches module state"), SubsystemCDO->IsSteamAvailable(), bInitialized);
	TestEqual(TEXT("Library availability matches module state"), UESteamBlueprintLibrary::IsSteamAvailable(), bInitialized);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamLifecycleSafetyTest,
	"UnrealExtendedSteam.Shared.Lifecycle.Safety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamLifecycleSafetyTest::RunTest(const FString& Parameters)
{
	if (!FExtendedSteamSharedModule::IsModuleAvailable())
	{
		AddError(TEXT("ExtendedSteamShared module is not loaded"));
		return false;
	}

	FExtendedSteamSharedModule& Module = FExtendedSteamSharedModule::Get();

	// The game server API is never auto-initialized.
	TestFalse(TEXT("Game server API not initialized by default"), Module.IsSteamGameServerInitialized());

	// Shutdown must be a safe no-op when the corresponding API is not initialized.
	// (Skipped for the client when a live session initialized it — tests must not tear it down.)
	Module.ShutdownSteamGameServer();
	TestFalse(TEXT("Game server shutdown without init is a no-op"), Module.IsSteamGameServerInitialized());

	if (!Module.IsSteamClientInitialized())
	{
		Module.ShutdownSteamClient();
		TestFalse(TEXT("Client shutdown without init is a no-op"), Module.IsSteamClientInitialized());
	}

	// IsSteamClientRunning is a pre-init query and must never fatally fail.
	const bool bClientRunning = Module.IsSteamClientRunning();
	TestEqual(TEXT("Library mirrors client-running state"), UESteamBlueprintLibrary::IsSteamClientRunning(), bClientRunning);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamBlueprintLibraryValuesTest,
	"UnrealExtendedSteam.Library.Values",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamBlueprintLibraryValuesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Library exposes configured app id"), UESteamBlueprintLibrary::GetConfiguredAppId(), 480);

	const FESteamId Id = UESteamBlueprintLibrary::SteamIdFromString(TEXT("76561197960287930"));
	TestTrue(TEXT("Library id parse"), UESteamBlueprintLibrary::IsValidSteamId(Id));
	TestEqual(TEXT("Library id to string"), UESteamBlueprintLibrary::SteamIdToString(Id), FString(TEXT("76561197960287930")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
