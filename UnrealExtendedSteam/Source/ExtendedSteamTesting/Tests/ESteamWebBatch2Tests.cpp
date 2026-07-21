// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Core/ESteamWebSettings.h"
#include "MicroTxn/ESteamWebMicroTxnSubsystem.h"
#include "Economy/ESteamWebEconomySubsystem.h"
#include "GameServersService/ESteamWebGameServersSubsystem.h"
#include "Leaderboards/ESteamWebLeaderboardsSubsystem.h"
#include "GameNotifications/ESteamWebGameNotificationsSubsystem.h"
#include "CheatReporting/ESteamWebCheatReportingSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// Deterministic construction test for the second batch of Steam Web interface subsystems —
// no network traffic: subsystems are only instantiated, never sent through.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamWebBatch2ConstructionTest,
	"UnrealExtendedSteam.Web.Batch2.Construction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamWebBatch2ConstructionTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	TestNotNull(TEXT("Game instance"), GameInstance);

	TestNotNull(TEXT("MicroTxn subsystem constructs"),
		NewObject<UESteamWebMicroTxnSubsystem>(GameInstance));
	TestNotNull(TEXT("Economy subsystem constructs"),
		NewObject<UESteamWebEconomySubsystem>(GameInstance));
	TestNotNull(TEXT("GameServers subsystem constructs"),
		NewObject<UESteamWebGameServersSubsystem>(GameInstance));
	TestNotNull(TEXT("Leaderboards subsystem constructs"),
		NewObject<UESteamWebLeaderboardsSubsystem>(GameInstance));
	TestNotNull(TEXT("GameNotifications subsystem constructs"),
		NewObject<UESteamWebGameNotificationsSubsystem>(GameInstance));
	TestNotNull(TEXT("CheatReporting subsystem constructs"),
		NewObject<UESteamWebCheatReportingSubsystem>(GameInstance));

	// The MicroTxn sandbox switch must default off — real ISteamMicroTxn unless opted in.
	const UESteamWebSettings* Settings = UESteamWebSettings::Get();
	TestNotNull(TEXT("Settings CDO"), Settings);
	TestFalse(TEXT("MicroTxn sandbox off by default"), Settings->bMicroTxnSandbox);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
