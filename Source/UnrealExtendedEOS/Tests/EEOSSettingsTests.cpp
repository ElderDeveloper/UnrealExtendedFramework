// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Shared/EEOSSettings.h"

// ── Settings Singleton ──────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSSettingsGetTest,
	"UnrealExtendedEOS.Settings.Get",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSSettingsGetTest::RunTest(const FString& Parameters)
{
	const UEEOSSettings* Settings = UEEOSSettings::Get();
	TestNotNull(TEXT("UEEOSSettings::Get() should return a valid pointer"), Settings);
	return true;
}


// ── Auth Defaults ───────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSSettingsAuthDefaultsTest,
	"UnrealExtendedEOS.Settings.AuthDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSSettingsAuthDefaultsTest::RunTest(const FString& Parameters)
{
	const UEEOSSettings* Settings = UEEOSSettings::Get();
	if (!TestNotNull(TEXT("Settings must be valid"), Settings)) return false;

	// Note: DefaultLoginType is config-driven and may differ per project, so we only validate it's accessible
	TestFalse(TEXT("bAutoLoginOnStart"), Settings->bAutoLoginOnStart);
	TestFalse(TEXT("bAutoConnectLoginOnStart"), Settings->bAutoConnectLoginOnStart);
	TestEqual(TEXT("DefaultConnectLoginType"), Settings->DefaultConnectLoginType, EEOSConnectLoginType::DeviceId);
	TestTrue(TEXT("bAutoCreateProductUser"), Settings->bAutoCreateProductUser);

	return true;
}


// ── Session Defaults ────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSSettingsSessionDefaultsTest,
	"UnrealExtendedEOS.Settings.SessionDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSSettingsSessionDefaultsTest::RunTest(const FString& Parameters)
{
	const UEEOSSettings* Settings = UEEOSSettings::Get();
	if (!TestNotNull(TEXT("Settings must be valid"), Settings)) return false;

	TestEqual(TEXT("DefaultMaxPlayers"), Settings->DefaultMaxPlayers, 4);
	TestEqual(TEXT("DefaultSessionName"), Settings->DefaultSessionName, FString(TEXT("DefaultGame")));
	TestTrue(TEXT("bPublicSessionsByDefault"), Settings->bPublicSessionsByDefault);
	TestFalse(TEXT("bUseLobbiesByDefault"), Settings->bUseLobbiesByDefault);
	TestEqual(TEXT("PreferredRegion"), Settings->PreferredRegion, EEOSRegionInfo::NoSelection);

	return true;
}


// ── Voice Defaults ──────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSSettingsVoiceDefaultsTest,
	"UnrealExtendedEOS.Settings.VoiceDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSSettingsVoiceDefaultsTest::RunTest(const FString& Parameters)
{
	const UEEOSSettings* Settings = UEEOSSettings::Get();
	if (!TestNotNull(TEXT("Settings must be valid"), Settings)) return false;

	TestEqual(TEXT("DefaultOutputVolume"), Settings->DefaultOutputVolume, 1.0f);
	TestEqual(TEXT("DefaultInputVolume"), Settings->DefaultInputVolume, 1.0f);
	TestFalse(TEXT("bStartMuted"), Settings->bStartMuted);

	return true;
}


// ── Feature Toggle Defaults ─────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSSettingsFeatureDefaultsTest,
	"UnrealExtendedEOS.Settings.FeatureDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSSettingsFeatureDefaultsTest::RunTest(const FString& Parameters)
{
	const UEEOSSettings* Settings = UEEOSSettings::Get();
	if (!TestNotNull(TEXT("Settings must be valid"), Settings)) return false;

	TestFalse(TEXT("bEnableAntiCheat"), Settings->bEnableAntiCheat);
	TestFalse(TEXT("bEnableVoiceChat"), Settings->bEnableVoiceChat);
	TestFalse(TEXT("bEnableP2P"), Settings->bEnableP2P);
	TestTrue(TEXT("bEnableLeaderboards"), Settings->bEnableLeaderboards);
	TestTrue(TEXT("bEnableAchievements"), Settings->bEnableAchievements);

	return true;
}


// ── Developer Defaults ──────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSSettingsDeveloperDefaultsTest,
	"UnrealExtendedEOS.Settings.DeveloperDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSSettingsDeveloperDefaultsTest::RunTest(const FString& Parameters)
{
	const UEEOSSettings* Settings = UEEOSSettings::Get();
	if (!TestNotNull(TEXT("Settings must be valid"), Settings)) return false;

	TestEqual(TEXT("DevAuthToolAddress"), Settings->DevAuthToolAddress, FString(TEXT("localhost:6547")));
	TestFalse(TEXT("bEnableVerboseLogging"), Settings->bEnableVerboseLogging);
	TestEqual(TEXT("DefaultRelayMode"), Settings->DefaultRelayMode, FString(TEXT("AllowRelays")));

	return true;
}


// ── Category Name ───────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSSettingsCategoryNameTest,
	"UnrealExtendedEOS.Settings.CategoryName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSSettingsCategoryNameTest::RunTest(const FString& Parameters)
{
	const UEEOSSettings* Settings = UEEOSSettings::Get();
	if (!TestNotNull(TEXT("Settings must be valid"), Settings)) return false;

	TestEqual(TEXT("Category should be Extended Framework"), Settings->GetCategoryName(), FName(TEXT("Extended Framework")));

	return true;
}
