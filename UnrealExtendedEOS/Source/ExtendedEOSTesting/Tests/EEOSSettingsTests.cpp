// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Shared/EEOSSettings.h"
#include "Shared/EEOSTypes.h"
#include "UObject/UnrealType.h"

// ─────────────────────────────────────────────────────────────────────────────
// These tests deliberately do NOT assert user-editable config VALUES (e.g.
// "DefaultMaxPlayers == 4") — those assertions break the moment a developer
// legitimately edits Project Settings. Instead they assert behavior and shape
// that must hold for ANY legal configuration:
//   - the settings singleton resolves to the class default object,
//   - clamped numeric properties actually sit inside their declared ranges,
//   - enum-typed properties hold valid entries of their enums,
//   - the config property schema (names + CPF_Config flag) is intact,
//   - code-defined behavior (settings category) is unchanged.
// ─────────────────────────────────────────────────────────────────────────────

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
	TestEqual(TEXT("Get() should resolve to the class default object"),
		Settings, GetDefault<UEEOSSettings>());
	return true;
}


// ── Clamp Ranges ────────────────────────────────────────────────────────────
// The ranges mirror the ClampMin/ClampMax metadata declared in EEOSSettings.h.
// The Project Settings UI enforces them, so any UI-edited config satisfies these;
// a violation means either hand-edited out-of-range config (a real hazard) or a
// silently changed clamp declaration.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSSettingsClampRangesTest,
	"UnrealExtendedEOS.Settings.ClampRanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSSettingsClampRangesTest::RunTest(const FString& Parameters)
{
	const UEEOSSettings* Settings = UEEOSSettings::Get();
	if (!TestNotNull(TEXT("Settings must be valid"), Settings)) return false;

	TestTrue(TEXT("DefaultMaxPlayers within its declared clamp range [2, 64]"),
		Settings->DefaultMaxPlayers >= 2 && Settings->DefaultMaxPlayers <= 64);

	TestTrue(TEXT("DefaultOutputVolume within its declared clamp range [0, 1]"),
		Settings->DefaultOutputVolume >= 0.0f && Settings->DefaultOutputVolume <= 1.0f);

	TestTrue(TEXT("DefaultInputVolume within its declared clamp range [0, 1]"),
		Settings->DefaultInputVolume >= 0.0f && Settings->DefaultInputVolume <= 1.0f);

	return true;
}


// ── Enum Validity ───────────────────────────────────────────────────────────
// Whatever the project configured, enum-typed settings must deserialize to real
// entries of their enums (a stale config after an enum rename would not).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSSettingsEnumValidityTest,
	"UnrealExtendedEOS.Settings.EnumValidity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSSettingsEnumValidityTest::RunTest(const FString& Parameters)
{
	const UEEOSSettings* Settings = UEEOSSettings::Get();
	if (!TestNotNull(TEXT("Settings must be valid"), Settings)) return false;

	TestTrue(TEXT("DefaultLoginType is a valid EEOSLoginType entry"),
		StaticEnum<EEOSLoginType>()->IsValidEnumValue(static_cast<int64>(Settings->DefaultLoginType)));

	TestTrue(TEXT("DefaultConnectLoginType is a valid EEOSConnectLoginType entry"),
		StaticEnum<EEOSConnectLoginType>()->IsValidEnumValue(static_cast<int64>(Settings->DefaultConnectLoginType)));

	TestTrue(TEXT("PreferredRegion is a valid EEOSRegionInfo entry"),
		StaticEnum<EEOSRegionInfo>()->IsValidEnumValue(static_cast<int64>(Settings->PreferredRegion)));

	return true;
}


// ── Config Schema ───────────────────────────────────────────────────────────
// CDO-vs-schema check: every setting the docs and subsystems rely on must exist
// under its exact name and be config-backed. Catches accidental renames/removals
// that would silently orphan users' saved config values.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSSettingsConfigSchemaTest,
	"UnrealExtendedEOS.Settings.ConfigSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSSettingsConfigSchemaTest::RunTest(const FString& Parameters)
{
	static const TCHAR* ExpectedConfigProperties[] =
	{
		// Credentials
		TEXT("ProductId"), TEXT("SandboxId"), TEXT("DeploymentId"),
		TEXT("ClientId"), TEXT("ClientSecret"), TEXT("EncryptionKey"),
		// Auth
		TEXT("DefaultLoginType"), TEXT("bAutoLoginOnStart"), TEXT("bAutoConnectLoginOnStart"),
		TEXT("DefaultConnectLoginType"), TEXT("bAutoCreateProductUser"),
		// Sessions
		TEXT("DefaultMaxPlayers"), TEXT("DefaultSessionName"),
		TEXT("bPublicSessionsByDefault"), TEXT("bUseLobbiesByDefault"), TEXT("PreferredRegion"),
		// Voice
		TEXT("DefaultOutputVolume"), TEXT("DefaultInputVolume"), TEXT("bStartMuted"),
		// P2P
		TEXT("DefaultRelayMode"),
		// Developer
		TEXT("DevAuthToolAddress"), TEXT("DevAuthCredentialName"), TEXT("bEnableVerboseLogging"),
		// Features
		TEXT("bEnableAntiCheat"), TEXT("bEnableVoiceChat"), TEXT("bEnableP2P"),
		TEXT("bEnableLeaderboards"), TEXT("bEnableAchievements"),
	};

	UClass* SettingsClass = UEEOSSettings::StaticClass();
	for (const TCHAR* PropertyName : ExpectedConfigProperties)
	{
		FProperty* Property = SettingsClass->FindPropertyByName(PropertyName);
		if (!TestNotNull(*FString::Printf(TEXT("Property '%s' should exist on UEEOSSettings"), PropertyName), Property))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("Property '%s' should be config-backed (CPF_Config)"), PropertyName),
			Property->HasAnyPropertyFlags(CPF_Config));
	}

	return true;
}


// ── Category Name ───────────────────────────────────────────────────────────
// Code-defined behavior (not config): the settings must register under the
// shared "Extended Framework" category in Project Settings.

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
