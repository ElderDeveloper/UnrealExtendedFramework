// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamTypes.h"
#include "OnlineSubsystem.h"
#include "Interfaces/VoiceInterface.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamOSSVoiceOfflineStateTest,
	"UnrealExtendedSteam.OSS.Voice.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamOSSVoiceOfflineStateTest::RunTest(const FString& Parameters)
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(ESTEAM_SUBSYSTEM);

	// Sampled AFTER Get: FOnlineSubsystemExtendedSteam::Init attempts a one-shot Steam client
	// initialization through the shared module, so Get itself can bring the client up.
	const bool bSteamClientUp = FExtendedSteamSharedModule::IsModuleAvailable()
		&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();

	if (!bSteamClientUp || Subsystem == nullptr)
	{
		// Without an initialized Steam client the subsystem never comes up; the null-subsystem
		// contract itself is covered by UnrealExtendedSteam.OSS.Registration. Early-out success.
		return true;
	}

	// Steam up: the voice interface must be implemented.
	const IOnlineVoicePtr Voice = Subsystem->GetVoiceInterface();
	TestTrue(TEXT("Voice interface is implemented"), Voice.IsValid());
	if (!Voice.IsValid())
	{
		return false;
	}

	// Before any RegisterLocalTalker call no local talkers are tracked.
	TestEqual(TEXT("GetNumLocalTalkers before registration is zero"), Voice->GetNumLocalTalkers(), 0);

	// With the Steam client up a capture device is assumed present (Steam has no per-device query).
	TestTrue(TEXT("Headset reported present while the Steam client is up"), Voice->IsHeadsetPresent(0));

	// The debug-state string is always available and non-empty.
	TestFalse(TEXT("Voice debug state is non-empty"), Voice->GetVoiceDebugState().IsEmpty());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
