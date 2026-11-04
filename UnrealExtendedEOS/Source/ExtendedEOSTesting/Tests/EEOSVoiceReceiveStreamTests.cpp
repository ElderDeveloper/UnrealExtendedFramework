// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Voice/EEOSVoiceReceiveRouter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace EEOSVoiceReceiveStreamTests
{
	/** Collects what a stream hands on, the way an audio bus patch would take it. */
	struct FCollector
	{
		TArray<float> Samples;
		int32 Calls = 0;
	};

	TSharedRef<FEEOSVoiceReceiveStream, ESPMode::ThreadSafe> MakeStream(const TSharedRef<FCollector>& Collector, const int32 OutputRate, const int32 BufferFrames)
	{
		return MakeShared<FEEOSVoiceReceiveStream, ESPMode::ThreadSafe>(OutputRate, BufferFrames,
			[Collector](const float* Samples, const int32 NumSamples)
			{
				Collector->Samples.Append(Samples, NumSamples);
				++Collector->Calls;
				return NumSamples;
			});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEEOSVoiceReceiveStreamTest,
	"UnrealExtendedEOS.Voice.ReceiveStream",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEEOSVoiceReceiveStreamTest::RunTest(const FString& Parameters)
{
	using namespace EEOSVoiceReceiveStreamTests;
	const TCHAR* Room = TEXT("Lobby");

	// A cushion of 60 ms at 48 kHz, filled by EOS's 10 ms frames.
	{
		const TSharedRef<FCollector> Collector = MakeShared<FCollector>();
		const TSharedRef<FEEOSVoiceReceiveStream, ESPMode::ThreadSafe> Stream = MakeStream(Collector, 48000, 2880);
		TArray<int16> Frame;
		Frame.Init(16384, 480);
		double Now = 100.0;
		for (int32 Push = 0; Push < 5; ++Push, Now += 0.01)
		{
			Stream->Push(Frame, 48000, 1, 1.0f, Room, Now);
		}
		TestEqual(TEXT("Nothing plays before the cushion is full"), Collector->Samples.Num(), 0);
		Stream->Push(Frame, 48000, 1, 1.0f, Room, Now);
		Now += 0.01;
		TestEqual(TEXT("The full cushion goes out at once"), Collector->Samples.Num(), 2880);
		TestTrue(TEXT("Samples are scaled from 16-bit"), FMath::IsNearlyEqual(Collector->Samples[0], 0.5f, 1.0e-4f));
		Stream->Push(Frame, 48000, 1, 1.0f, Room, Now);
		Now += 0.01;
		TestEqual(TEXT("Once flowing, each frame goes straight out"), Collector->Samples.Num(), 2880 + 480);

		// A pause longer than the cushion: the receiver has run dry, so it gathers again.
		Now += 0.2;
		Stream->Push(Frame, 48000, 1, 1.0f, Room, Now);
		TestEqual(TEXT("After a pause the cushion is gathered again"), Collector->Samples.Num(), 2880 + 480);
	}

	// Stereo is mixed to mono; the listener's gain and the output volume both scale it.
	{
		const TSharedRef<FCollector> Collector = MakeShared<FCollector>();
		const TSharedRef<FEEOSVoiceReceiveStream, ESPMode::ThreadSafe> Stream = MakeStream(Collector, 48000, 1);
		Stream->Gain.store(0.5f);
		const TArray<int16> Stereo = { 16384, 0, 16384, 0 };
		Stream->Push(Stereo, 48000, 2, 0.5f, Room, 1.0);
		TestEqual(TEXT("Two stereo frames make two mono samples"), Collector->Samples.Num(), 2);
		TestTrue(TEXT("Mono mix with both gains"), FMath::IsNearlyEqual(Collector->Samples[0], 0.25f * 0.5f * 0.5f, 1.0e-4f));
	}

	// 48 kHz in, 44.1 kHz out: the count follows the rate across many frames, and a steady level stays steady.
	{
		const TSharedRef<FCollector> Collector = MakeShared<FCollector>();
		const TSharedRef<FEEOSVoiceReceiveStream, ESPMode::ThreadSafe> Stream = MakeStream(Collector, 44100, 1);
		TArray<int16> Frame;
		Frame.Init(8192, 480);
		double Now = 10.0;
		for (int32 Push = 0; Push < 100; ++Push, Now += 0.01)
		{
			Stream->Push(Frame, 48000, 1, 1.0f, Room, Now);
		}
		TestTrue(TEXT("One second at 48 kHz becomes one second at 44.1 kHz"), FMath::Abs(Collector->Samples.Num() - 44100) <= 2);
		bool bSteady = true;
		for (int32 Index = 1; Index < Collector->Samples.Num(); ++Index)
		{
			bSteady &= FMath::IsNearlyEqual(Collector->Samples[Index], 0.25f, 1.0e-4f);
		}
		TestTrue(TEXT("Resampling leaves no seam between frames"), bSteady);
	}

	// The same player heard in a second room is ignored while the first one flows.
	{
		const TSharedRef<FCollector> Collector = MakeShared<FCollector>();
		const TSharedRef<FEEOSVoiceReceiveStream, ESPMode::ThreadSafe> Stream = MakeStream(Collector, 48000, 1);
		const TArray<int16> Frame = { 100, 100 };
		Stream->Push(Frame, 48000, 1, 1.0f, Room, 1.0);
		Stream->Push(Frame, 48000, 1, 1.0f, TEXT("Radio"), 1.0);
		TestEqual(TEXT("A second room's copy is dropped"), Collector->Samples.Num(), 2);
		Stream->Push(Frame, 48000, 1, 1.0f, TEXT("Radio"), 2.0);
		TestEqual(TEXT("Once the first room goes quiet, the second is taken"), Collector->Samples.Num(), 4);
	}

	// The router silences EOS's own playback of the mix only while voice is routed.
	{
		const TSharedRef<FEEOSVoiceReceiveRouter, ESPMode::ThreadSafe> Router = MakeShared<FEEOSVoiceReceiveRouter, ESPMode::ThreadSafe>();
		TArray<int16> Mix = { 1000, -1000 };
		Router->HandleMixedAudio(Mix, 48000, 1, false, Room, FString());
		TestTrue(TEXT("The mix is left alone while not routed"), Mix[0] == 1000 && Mix[1] == -1000);
		Router->bSilenceMix.store(true);
		Router->HandleMixedAudio(Mix, 48000, 1, false, Room, FString());
		TestTrue(TEXT("The mix is silenced while routed"), Mix[0] == 0 && Mix[1] == 0);

		const TSharedRef<FCollector> Collector = MakeShared<FCollector>();
		Router->SetPlayerGain(TEXT("PlayerA"), 0.0f);
		Router->AddStream(TEXT("PlayerA"), MakeStream(Collector, 48000, 1));
		TArray<int16> Voice = { 16384 };
		Router->HandleUnmixedAudio(Voice, 48000, 1, false, Room, TEXT("PlayerB"));
		TestEqual(TEXT("Another player's audio does not reach this stream"), Collector->Samples.Num(), 0);
		Router->HandleUnmixedAudio(Voice, 48000, 1, false, Room, TEXT("PlayerA"));
		TestTrue(TEXT("A gain set before the stream existed applies"), Collector->Samples.Num() == 1 && Collector->Samples[0] == 0.0f);
	}

	return true;
}

#endif
