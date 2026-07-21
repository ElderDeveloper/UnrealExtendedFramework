// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "ExtendedSteamSharedModule.h"
#include "Voice/ESteamVoiceSubsystem.h"
#include "Voice/ESteamVoiceSoundWave.h"
#include "Networking/ESteamNetworkingSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// Steam may genuinely be initialized when tests run interactively (auto-init in the editor),
// so each test branches: strict offline defaults when Steam is down, weak invariants when up.
// Subsystems are constructed directly without Initialize — every method is guarded by
// IsSteamAvailable(), which only consults the shared module.
namespace ESteamVoiceNetworkingTestHelpers
{
	bool IsSteamClientUp()
	{
		return FExtendedSteamSharedModule::IsModuleAvailable()
			&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamVoiceOfflineStateTest,
	"UnrealExtendedSteam.Voice.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamVoiceOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamVoiceSubsystem* Voice = NewObject<UESteamVoiceSubsystem>(GameInstance);
	TestNotNull(TEXT("Voice subsystem constructed"), Voice);

	const bool bSteamUp = ESteamVoiceNetworkingTestHelpers::IsSteamClientUp();

	TestFalse(TEXT("Not recording initially"), Voice->IsRecording());

	if (bSteamUp)
	{
		TestTrue(TEXT("Optimal sample rate is positive while Steam is up"), Voice->GetVoiceOptimalSampleRate() > 0);
		UESteamVoiceSoundWave* Wave = Voice->CreateVoiceSoundWave();
		TestNotNull(TEXT("CreateVoiceSoundWave returns a wave while Steam is up"), Wave);
		return true;
	}

	// The unavailable guards log a standard warning; that is the expected behavior under test.
	AddExpectedMessage(TEXT("Steam is not available"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	Voice->StartVoiceRecording();
	TestFalse(TEXT("StartVoiceRecording is a safe no-op offline"), Voice->IsRecording());
	Voice->StopVoiceRecording();
	TestFalse(TEXT("StopVoiceRecording is a safe no-op offline"), Voice->IsRecording());

	TestEqual(TEXT("Optimal sample rate is 0 offline"), Voice->GetVoiceOptimalSampleRate(), 0);

	TArray<uint8> Compressed;
	Compressed.Add(0);
	TArray<uint8> Pcm;
	TestFalse(TEXT("DecompressVoice fails offline"), Voice->DecompressVoice(Compressed, Pcm));
	TestEqual(TEXT("DecompressVoice outputs no PCM offline"), Pcm.Num(), 0);

	// Offline behavior by design: no wave rather than one with a guessed sample rate.
	TestNull(TEXT("CreateVoiceSoundWave returns null offline"), Voice->CreateVoiceSoundWave());
	TestFalse(TEXT("PlayVoiceData fails on a null target"), Voice->PlayVoiceData(nullptr, Compressed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESteamNetworkingOfflineStateTest,
	"UnrealExtendedSteam.Networking.OfflineState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESteamNetworkingOfflineStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UESteamNetworkingSubsystem* Networking = NewObject<UESteamNetworkingSubsystem>(GameInstance);
	TestNotNull(TEXT("Networking subsystem constructed"), Networking);

	// Channel count clamping is pure local state — valid with or without Steam.
	TestEqual(TEXT("Polled channel count defaults to 1"), Networking->GetPolledChannelCount(), 1);
	Networking->SetPolledChannelCount(0);
	TestEqual(TEXT("Polled channel count clamps 0 to 1"), Networking->GetPolledChannelCount(), 1);
	Networking->SetPolledChannelCount(-5);
	TestEqual(TEXT("Polled channel count clamps negatives to 1"), Networking->GetPolledChannelCount(), 1);
	Networking->SetPolledChannelCount(4);
	TestEqual(TEXT("Polled channel count accepts valid values"), Networking->GetPolledChannelCount(), 4);

	const bool bSteamUp = ESteamVoiceNetworkingTestHelpers::IsSteamClientUp();

	if (bSteamUp)
	{
		FESteamP2PSessionState State;
		TestFalse(TEXT("No session state for an invalid Steam id while Steam is up"),
			Networking->GetP2PSessionState(FESteamId(), State));
		return true;
	}

	// The unavailable guards log a standard warning; that is the expected behavior under test.
	AddExpectedMessage(TEXT("Steam is not available"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	TArray<uint8> Payload;
	Payload.Add(42);
	TestFalse(TEXT("SendP2PPacket fails offline"),
		Networking->SendP2PPacket(FESteamId(1), Payload, EESteamP2PSendType::Reliable, 0));
	TestFalse(TEXT("AcceptP2PSessionWithUser fails offline"), Networking->AcceptP2PSessionWithUser(FESteamId(1)));
	TestFalse(TEXT("CloseP2PSessionWithUser fails offline"), Networking->CloseP2PSessionWithUser(FESteamId(1)));
	TestFalse(TEXT("CloseP2PChannelWithUser fails offline"), Networking->CloseP2PChannelWithUser(FESteamId(1), 0));
	TestFalse(TEXT("AllowP2PPacketRelay fails offline"), Networking->AllowP2PPacketRelay(true));

	FESteamP2PSessionState State;
	State.bConnectionActive = true;
	State.P2PSessionError = EESteamP2PSessionError::Timeout;
	TestFalse(TEXT("GetP2PSessionState fails offline"), Networking->GetP2PSessionState(FESteamId(1), State));
	TestFalse(TEXT("GetP2PSessionState resets the output offline"), State.bConnectionActive);
	TestEqual(TEXT("GetP2PSessionState resets the error output offline"), State.P2PSessionError, EESteamP2PSessionError::None);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
