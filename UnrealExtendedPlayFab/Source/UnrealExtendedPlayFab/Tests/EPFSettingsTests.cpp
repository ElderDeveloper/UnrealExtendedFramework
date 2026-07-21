// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Shared/EPFSettings.h"


// ═══════════════════════════════════════════════════════════════════════════════
// UEPFSettings — Singleton Access
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFSettingsSingletonTest,
	"UnrealExtendedPlayFab.Settings.Singleton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFSettingsSingletonTest::RunTest(const FString& Parameters)
{
	const UEPFSettings* Settings = UEPFSettings::Get();

	TestNotNull(TEXT("Settings singleton should not be null"), Settings);

	// Two calls should return the same pointer
	const UEPFSettings* Settings2 = UEPFSettings::Get();
	TestEqual(TEXT("Two Get() calls should return same pointer"), Settings, Settings2);

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════════
// UEPFSettings — Default Values
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFSettingsDefaultsTest,
	"UnrealExtendedPlayFab.Settings.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	const UEPFSettings* Settings = UEPFSettings::Get();
	if (!TestNotNull(TEXT("Settings singleton should exist"), Settings)) return false;

	TestTrue(TEXT("bCreateAccountOnFirstLogin should default to true"), Settings->bCreateAccountOnFirstLogin);
	TestFalse(TEXT("bEnableVerboseLogging should default to false"), Settings->bEnableVerboseLogging);
	TestTrue(TEXT("bIncludeSdkHeader should default to true"), Settings->bIncludeSdkHeader);

	return true;
}


// ═══════════════════════════════════════════════════════════════════════════════
// UEPFSettings — API URL Construction
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEPFSettingsApiUrlTest,
	"UnrealExtendedPlayFab.Settings.ApiBaseUrl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEPFSettingsApiUrlTest::RunTest(const FString& Parameters)
{
	// Test with a fresh mutable CDO copy to avoid mutating the actual settings
	UEPFSettings* Settings = NewObject<UEPFSettings>();

	// Default URL from TitleId
	Settings->TitleId = TEXT("ABC123");
	Settings->ApiBaseUrlOverride.Empty();
	TestEqual(TEXT("URL from TitleId"), Settings->GetApiBaseUrl(), FString(TEXT("https://ABC123.playfabapi.com")));

	// Override takes precedence
	Settings->ApiBaseUrlOverride = TEXT("https://custom.api.com");
	TestEqual(TEXT("Override URL takes precedence"), Settings->GetApiBaseUrl(), FString(TEXT("https://custom.api.com")));

	// Empty TitleId still produces a valid URL pattern
	Settings->ApiBaseUrlOverride.Empty();
	Settings->TitleId = TEXT("");
	TestEqual(TEXT("Empty TitleId"), Settings->GetApiBaseUrl(), FString(TEXT("https://.playfabapi.com")));

	return true;
}
