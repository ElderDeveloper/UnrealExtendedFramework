// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "ExtendedSteamSharedModule.h"
#include "Parties/ESteamPartiesSubsystem.h"
#include "GameSearch/ESteamGameSearchSubsystem.h"
#include "RemotePlay/ESteamRemotePlaySubsystem.h"
#include "ParentalSettings/ESteamParentalSettingsSubsystem.h"
#include "NetworkingUtils/ESteamNetworkingUtilsSubsystem.h"
#include "Timeline/ESteamTimelineSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// Steam may genuinely be initialized when tests run interactively (auto-init in the editor),
// so each test branches: strict offline defaults when Steam is down, weak invariants when up.
// Subsystems are constructed directly without Initialize — every method is guarded by
// IsSteamAvailable(), which only consults the shared module.
namespace ESteamExtrasTestHelpers
{
	bool IsSteamClientUp()
	{
		return FExtendedSteamSharedModule::IsModuleAvailable()
			&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamPartiesOfflineStateTest,
	"UnrealExtendedSteam.Parties.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamPartiesOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamPartiesSubsystem* Parties = NewObject<UESteamPartiesSubsystem>(GameInstance);
	TestNotNull(TEXT("Parties subsystem constructed"), Parties);

	const bool bSteamUp = ESteamExtrasTestHelpers::IsSteamClientUp();

	if (bSteamUp)
	{
		TestTrue(TEXT("Beacon count is non-negative while Steam is up"), Parties->GetNumActiveBeacons() >= 0);
		return true;
	}

	// The unavailable guards log a standard warning; that is the expected behavior under test.
	AddExpectedMessage(TEXT("Steam is not available"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	TestEqual(TEXT("Beacon count is 0 offline"), Parties->GetNumActiveBeacons(), 0);
	TestEqual(TEXT("Beacon by index is 0 offline"), Parties->GetBeaconByIndex(0), static_cast<int64>(0));
	TestEqual(TEXT("Available beacon location count is 0 offline"), Parties->GetNumAvailableBeaconLocations(), 0);

	TArray<FESteamPartyBeaconLocation> Locations;
	TestEqual(TEXT("GetAvailableBeaconLocations returns 0 offline"), Parties->GetAvailableBeaconLocations(Locations), 0);
	TestEqual(TEXT("GetAvailableBeaconLocations outputs no entries offline"), Locations.Num(), 0);

	FString LocationData;
	TestFalse(TEXT("GetBeaconLocationData fails offline"),
		Parties->GetBeaconLocationData(FESteamPartyBeaconLocation(), EESteamPartyBeaconLocationData::Name, LocationData));

	FESteamId Owner;
	FString Metadata;
	TestFalse(TEXT("GetBeaconDetails fails offline"), Parties->GetBeaconDetails(1, Owner, Metadata));
	TestFalse(TEXT("GetBeaconDetails outputs an invalid owner offline"), Owner.IsValid());
	TestTrue(TEXT("GetBeaconDetails outputs empty metadata offline"), Metadata.IsEmpty());

	TestFalse(TEXT("JoinParty fails offline"), Parties->JoinParty(1));
	TestFalse(TEXT("CreateBeacon fails offline"), Parties->CreateBeacon(4, TEXT("+connect test"), FString()));
	TestFalse(TEXT("ChangeNumOpenSlots fails offline"), Parties->ChangeNumOpenSlots(1, 8));
	TestFalse(TEXT("DestroyBeacon fails offline"), Parties->DestroyBeacon(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamGameSearchOfflineStateTest,
	"UnrealExtendedSteam.GameSearch.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamGameSearchOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamGameSearchSubsystem* GameSearch = NewObject<UESteamGameSearchSubsystem>(GameInstance);
	TestNotNull(TEXT("GameSearch subsystem constructed"), GameSearch);

	// ISteamGameSearch was removed from the Steamworks SDK (1.62+): the wrapper is a stub that
	// fails regardless of the Steam client state, in both branches. It logs the removal notice at
	// Verbose (not Warning), so no AddExpectedMessage is needed — the automation framework only
	// flags Warning/Error.
	TestEqual(TEXT("AddGameSearchParams reports Failed"),
		GameSearch->AddGameSearchParams(TEXT("map"), TEXT("de_dust")), EESteamGameSearchResult::Failed);
	TestEqual(TEXT("SearchForGameWithLobby reports Failed"),
		GameSearch->SearchForGameWithLobby(FESteamId(), 1, 4), EESteamGameSearchResult::Failed);
	TestEqual(TEXT("SearchForGameSolo reports Failed"),
		GameSearch->SearchForGameSolo(1, 4), EESteamGameSearchResult::Failed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamRemotePlayOfflineStateTest,
	"UnrealExtendedSteam.RemotePlay.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamRemotePlayOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamRemotePlaySubsystem* RemotePlay = NewObject<UESteamRemotePlaySubsystem>(GameInstance);
	TestNotNull(TEXT("RemotePlay subsystem constructed"), RemotePlay);

	const bool bSteamUp = ESteamExtrasTestHelpers::IsSteamClientUp();

	if (bSteamUp)
	{
		TestTrue(TEXT("Session count is non-negative while Steam is up"), RemotePlay->GetSessionCount() >= 0);
		return true;
	}

	AddExpectedMessage(TEXT("Steam is not available"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	TestEqual(TEXT("Session count is 0 offline"), RemotePlay->GetSessionCount(), 0);
	TestEqual(TEXT("Session id is 0 offline"), RemotePlay->GetSessionId(0), 0);
	TestTrue(TEXT("Session client name is empty offline"), RemotePlay->GetSessionClientName(1).IsEmpty());
	TestFalse(TEXT("Session SteamID is invalid offline"), RemotePlay->GetSessionSteamID(1).IsValid());

	FESteamRemotePlaySessionInfo Info;
	TestFalse(TEXT("GetSessionInfo fails offline"), RemotePlay->GetSessionInfo(1, Info));
	TestTrue(TEXT("GetSessionInfo outputs an empty client name offline"), Info.ClientName.IsEmpty());
	TestEqual(TEXT("GetSessionInfo outputs an Unknown form factor offline"), Info.FormFactor, EESteamRemotePlayFormFactor::Unknown);
	TestEqual(TEXT("GetSessionInfo outputs a zero resolution offline"), Info.Resolution, FIntPoint::ZeroValue);

	TestFalse(TEXT("SendRemotePlayTogetherInvite fails offline"), RemotePlay->SendRemotePlayTogetherInvite(FESteamId()));
	TestFalse(TEXT("ShowRemotePlayTogetherUI fails offline"), RemotePlay->ShowRemotePlayTogetherUI());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamParentalSettingsOfflineStateTest,
	"UnrealExtendedSteam.ParentalSettings.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamParentalSettingsOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamParentalSettingsSubsystem* Parental = NewObject<UESteamParentalSettingsSubsystem>(GameInstance);
	TestNotNull(TEXT("ParentalSettings subsystem constructed"), Parental);

	const bool bSteamUp = ESteamExtrasTestHelpers::IsSteamClientUp();

	if (bSteamUp)
	{
		// A locked Family View implies Family View is enabled — a real invariant of the API.
		TestTrue(TEXT("Locked implies enabled while Steam is up"),
			!Parental->IsParentalLockLocked() || Parental->IsParentalLockEnabled());
		return true;
	}

	TestFalse(TEXT("Parental lock is not enabled offline"), Parental->IsParentalLockEnabled());
	TestFalse(TEXT("Parental lock is not locked offline"), Parental->IsParentalLockLocked());
	TestFalse(TEXT("App is not blocked offline"), Parental->IsAppBlocked(480));
	TestFalse(TEXT("App is not in the block list offline"), Parental->IsAppInBlockList(480));
	TestFalse(TEXT("Feature is not blocked offline"), Parental->IsFeatureBlocked(EESteamParentalFeature::Store));
	TestFalse(TEXT("Feature is not in the block list offline"), Parental->IsFeatureInBlockList(EESteamParentalFeature::Community));
	// Newly exposed features map cleanly and stay unblocked offline.
	TestFalse(TEXT("SiteLicense feature is not blocked offline"), Parental->IsFeatureBlocked(EESteamParentalFeature::SiteLicense));
	TestFalse(TEXT("Desktop feature is not in the block list offline"), Parental->IsFeatureInBlockList(EESteamParentalFeature::Desktop));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamNetworkingUtilsOfflineStateTest,
	"UnrealExtendedSteam.NetworkingUtils.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamNetworkingUtilsOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamNetworkingUtilsSubsystem* NetworkingUtils = NewObject<UESteamNetworkingUtilsSubsystem>(GameInstance);
	TestNotNull(TEXT("NetworkingUtils subsystem constructed"), NetworkingUtils);

	const bool bSteamUp = ESteamExtrasTestHelpers::IsSteamClientUp();

	if (bSteamUp)
	{
		// Empty strings never parse as ping locations, even while Steam is up.
		TestEqual(TEXT("Estimating between unparsable locations returns -1 while Steam is up"),
			NetworkingUtils->EstimatePingBetweenTwoLocations(FString(), FString()), -1);
		return true;
	}

	AddExpectedMessage(TEXT("Steam is not available"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	TestEqual(TEXT("Relay network status is Unknown offline"),
		NetworkingUtils->GetRelayNetworkStatusSimple(), EESteamRelayNetworkStatus::Unknown);

	FString DebugMsg;
	int32 AvailNetworkConfig = 1;
	int32 AvailAnyRelay = 1;
	TestEqual(TEXT("Detailed relay status is Unknown offline"),
		NetworkingUtils->GetRelayNetworkStatusDetailed(DebugMsg, AvailNetworkConfig, AvailAnyRelay), EESteamRelayNetworkStatus::Unknown);
	TestTrue(TEXT("Detailed relay debug message is empty offline"), DebugMsg.IsEmpty());

	FString SerializedLocation;
	TestEqual(TEXT("GetLocalPingLocation returns -1 offline"),
		NetworkingUtils->GetLocalPingLocation(SerializedLocation), -1.f);
	TestTrue(TEXT("GetLocalPingLocation outputs an empty location offline"), SerializedLocation.IsEmpty());

	TestEqual(TEXT("EstimatePingBetweenTwoLocations returns -1 offline"),
		NetworkingUtils->EstimatePingBetweenTwoLocations(FString(), FString()), -1);
	TestEqual(TEXT("EstimatePingTimeFromLocalHost returns -1 offline"),
		NetworkingUtils->EstimatePingTimeFromLocalHost(FString()), -1);
	TestEqual(TEXT("GetLocalTimestamp is 0 offline"), NetworkingUtils->GetLocalTimestamp(), static_cast<int64>(0));
	TestEqual(TEXT("GetPOPCount is 0 offline"), NetworkingUtils->GetPOPCount(), 0);
	TestFalse(TEXT("CheckPingDataUpToDate fails offline"), NetworkingUtils->CheckPingDataUpToDate(60.f));

	int32 ConfigInt = 0;
	TestFalse(TEXT("GetGlobalConfigValueInt32 fails offline"), NetworkingUtils->GetGlobalConfigValueInt32(1, ConfigInt));

	NetworkingUtils->InitRelayNetworkAccess(); // no-op offline, logs the standard warning
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamTimelineOfflineStateTest,
	"UnrealExtendedSteam.Timeline.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamTimelineOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamTimelineSubsystem* Timeline = NewObject<UESteamTimelineSubsystem>(GameInstance);
	TestNotNull(TEXT("Timeline subsystem constructed"), Timeline);

	const bool bSteamUp = ESteamExtrasTestHelpers::IsSteamClientUp();

	if (bSteamUp)
	{
		TestTrue(TEXT("Steam is reported available while Steam is up"), Timeline->IsSteamAvailable());
		return true;
	}

	// All timeline calls degrade to no-ops with a verbose log when the SDK or client lacks ISteamTimeline.
	TestEqual(TEXT("AddInstantaneousTimelineEvent returns 0 offline"),
		Timeline->AddInstantaneousTimelineEvent(TEXT("Title"), TEXT("Description"), TEXT("steam_flag"), 100, 0.f),
		static_cast<int64>(0));
	TestEqual(TEXT("AddRangeTimelineEvent returns 0 offline"),
		Timeline->AddRangeTimelineEvent(TEXT("Title"), TEXT("Description"), TEXT("steam_flag"), 100, -5.f, 5.f),
		static_cast<int64>(0));
	TestEqual(TEXT("StartRangeTimelineEvent returns 0 offline"),
		Timeline->StartRangeTimelineEvent(TEXT("Title"), TEXT("Description"), TEXT("steam_flag"), 100, 0.f),
		static_cast<int64>(0));

	// No-op smoke calls: must not crash without Steam.
	Timeline->SetTimelineTooltip(TEXT("Tooltip"), 0.f);
	Timeline->ClearTimelineTooltip(0.f);
	Timeline->SetTimelineGameMode(EESteamTimelineGameMode::Menus);
	Timeline->UpdateRangeTimelineEvent(1, TEXT("Title"), TEXT("Description"), TEXT("steam_flag"), 100);
	Timeline->EndRangeTimelineEvent(1, 0.f);
	Timeline->RemoveTimelineEvent(1);
	Timeline->DoesEventRecordingExist(1);
	Timeline->StartGamePhase();
	Timeline->SetGamePhaseId(TEXT("phase-1"));
	Timeline->AddGamePhaseTag(TEXT("Boss"), TEXT("steam_flag"), TEXT("BossesDefeated"), 100);
	Timeline->SetGamePhaseAttribute(TEXT("KDA"), TEXT("0/0/0"), 100);
	Timeline->OpenOverlayToGamePhase(TEXT("phase-1"));
	Timeline->OpenOverlayToTimelineEvent(1);
	Timeline->EndGamePhase();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
