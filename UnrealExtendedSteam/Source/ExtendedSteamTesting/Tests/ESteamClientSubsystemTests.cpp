// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "ExtendedSteamSharedModule.h"
#include "User/ESteamUserSubsystem.h"
#include "Utils/ESteamUtilsSubsystem.h"
#include "Apps/ESteamAppsSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// Steam may genuinely be initialized when tests run interactively (auto-init in the editor),
// so each test branches: strict offline defaults when Steam is down, weak invariants when up.
// Subsystems are constructed directly without Initialize — every method is guarded by
// IsSteamAvailable(), which only consults the shared module.
namespace ESteamClientTestHelpers
{
	bool IsSteamClientUp()
	{
		return FExtendedSteamSharedModule::IsModuleAvailable()
			&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamUserOfflineStateTest,
	"UnrealExtendedSteam.User.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamUserOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamUserSubsystem* User = NewObject<UESteamUserSubsystem>(GameInstance);
	TestNotNull(TEXT("User subsystem constructed"), User);

	const bool bSteamUp = ESteamClientTestHelpers::IsSteamClientUp();

	if (bSteamUp)
	{
		TestTrue(TEXT("Local Steam id is valid while Steam is up"), User->GetLocalSteamId().IsValid());
		return true;
	}

	// The unavailable guards log a standard warning; that is the expected behavior under test.
	AddExpectedMessage(TEXT("Steam is not available"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	TestFalse(TEXT("Local Steam id is invalid offline"), User->GetLocalSteamId().IsValid());
	TestFalse(TEXT("Not logged on offline"), User->IsLoggedOn());
	TestEqual(TEXT("Steam level is 0 offline"), User->GetPlayerSteamLevel(), 0);

	FString HexTicket;
	TestEqual(TEXT("GetAuthSessionTicket returns 0 offline"), User->GetAuthSessionTicket(HexTicket), 0);
	TestTrue(TEXT("GetAuthSessionTicket outputs an empty ticket offline"), HexTicket.IsEmpty());
	TestEqual(TEXT("RequestWebApiAuthTicket returns 0 offline"), User->RequestWebApiAuthTicket(FString()), 0);
	TestEqual(TEXT("BeginAuthSession reports Steam unavailable offline"),
		User->BeginAuthSession(FString(), FESteamId()), EESteamBeginAuthResult::SteamUnavailable);

	TestEqual(TEXT("UserHasLicenseForApp reports NoAuth offline"),
		User->UserHasLicenseForApp(FESteamId(), 480), EESteamUserHasLicenseResult::NoAuth);
	TestEqual(TEXT("GetGameBadgeLevel is 0 offline"), User->GetGameBadgeLevel(1, false), 0);
	TestTrue(TEXT("GetUserDataFolder is empty offline"), User->GetUserDataFolder().IsEmpty());
	TestFalse(TEXT("IsPhoneIdentifying is false offline"), User->IsPhoneIdentifying());
	TestFalse(TEXT("IsPhoneRequiringVerification is false offline"), User->IsPhoneRequiringVerification());
	TestFalse(TEXT("SetDurationControlOnlineState fails offline"), User->SetDurationControlOnlineState(2));
	TestFalse(TEXT("GetMarketEligibility fails offline"), User->GetMarketEligibility());
	TestFalse(TEXT("GetDurationControl fails offline"), User->GetDurationControl());
	TestFalse(TEXT("RequestStoreAuthURL fails offline"), User->RequestStoreAuthURL(TEXT("https://store.steampowered.com")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamUtilsOfflineStateTest,
	"UnrealExtendedSteam.Utils.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamUtilsOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamUtilsSubsystem* Utils = NewObject<UESteamUtilsSubsystem>(GameInstance);
	TestNotNull(TEXT("Utils subsystem constructed"), Utils);

	const bool bSteamUp = ESteamClientTestHelpers::IsSteamClientUp();

	if (bSteamUp)
	{
		TestTrue(TEXT("App id is non-zero while Steam is up"), Utils->GetAppId() > 0);
		return true;
	}

	TestEqual(TEXT("App id is 0 offline"), Utils->GetAppId(), 0);
	TestTrue(TEXT("IP country is empty offline"), Utils->GetIPCountry().IsEmpty());
	TestFalse(TEXT("Overlay is disabled offline"), Utils->IsOverlayEnabled());
	TestEqual(TEXT("Server real time is 0 offline"), Utils->GetServerRealTime(), static_cast<int64>(0));

	TestEqual(TEXT("Seconds since computer active is 0 offline"), Utils->GetSecondsSinceComputerActive(), 0);
	TestEqual(TEXT("Connected universe is 0 offline"), Utils->GetConnectedUniverse(), 0);
	TestFalse(TEXT("Not running in VR offline"), Utils->IsSteamRunningInVR());
	TestFalse(TEXT("Overlay does not need present offline"), Utils->OverlayNeedsPresent());
	TestEqual(TEXT("Entered gamepad text length is 0 offline"), Utils->GetEnteredGamepadTextLength(), 0);
	TestTrue(TEXT("Entered gamepad text is empty offline"), Utils->GetEnteredGamepadTextInput().IsEmpty());
	TestFalse(TEXT("InitFilterText fails offline"), Utils->InitFilterText());
	TestEqual(TEXT("FilterText passes input through offline"),
		Utils->FilterText(EESteamTextFilteringContext::Chat, FESteamId(), TEXT("hello")), FString(TEXT("hello")));
	TestEqual(TEXT("IPv6 connectivity is Unknown offline"),
		Utils->GetIPv6ConnectivityState(EESteamIPv6ConnectivityProtocol::HTTP), EESteamIPv6ConnectivityState::Unknown);

	int32 ImageWidth = 0;
	int32 ImageHeight = 0;
	TestFalse(TEXT("GetImageSize fails offline"), Utils->GetImageSize(1, ImageWidth, ImageHeight));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamAppsOfflineStateTest,
	"UnrealExtendedSteam.Apps.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamAppsOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamAppsSubsystem* Apps = NewObject<UESteamAppsSubsystem>(GameInstance);
	TestNotNull(TEXT("Apps subsystem constructed"), Apps);

	const bool bSteamUp = ESteamClientTestHelpers::IsSteamClientUp();

	if (bSteamUp)
	{
		TestFalse(TEXT("Game language is set while Steam is up"), Apps->GetCurrentGameLanguage().IsEmpty());
		TestTrue(TEXT("App owner is valid while Steam is up"), Apps->GetAppOwner().IsValid());
		return true;
	}

	TestFalse(TEXT("Not subscribed offline"), Apps->IsSubscribed());
	TestEqual(TEXT("DLC count is 0 offline"), Apps->GetDlcCount(), 0);
	TestTrue(TEXT("Game language is empty offline"), Apps->GetCurrentGameLanguage().IsEmpty());
	TestFalse(TEXT("App owner is invalid offline"), Apps->GetAppOwner().IsValid());

	FESteamDlcData Dlc;
	TestFalse(TEXT("GetDlcDataByIndex fails offline"), Apps->GetDlcDataByIndex(0, Dlc));

	TestEqual(TEXT("App build id is 0 offline"), Apps->GetAppBuildId(), 0);

	int32 SecondsAllowed = 0;
	int32 SecondsPlayed = 0;
	TestFalse(TEXT("IsTimedTrial is false offline"), Apps->IsTimedTrial(SecondsAllowed, SecondsPlayed));
	TestFalse(TEXT("SetDlcContext fails offline"), Apps->SetDlcContext(0));

	int32 BetasAvailable = 0;
	int32 BetasPrivate = 0;
	TestEqual(TEXT("GetNumBetas is 0 offline"), Apps->GetNumBetas(BetasAvailable, BetasPrivate), 0);

	int32 BetaFlags = 0;
	int32 BetaBuildId = 0;
	FString BetaName;
	FString BetaDescription;
	TestFalse(TEXT("GetBetaInfo fails offline"), Apps->GetBetaInfo(0, BetaFlags, BetaBuildId, BetaName, BetaDescription));

	TArray<int32> Depots;
	Apps->GetInstalledDepots(480, Depots);
	TestEqual(TEXT("No installed depots offline"), Depots.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
