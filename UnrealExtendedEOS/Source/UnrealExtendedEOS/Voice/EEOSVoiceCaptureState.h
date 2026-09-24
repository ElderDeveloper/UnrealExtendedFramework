#pragma once
#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

/** Component-owned state shared with EOS's audio callback. No UObject access on the audio thread. */
struct FEEOSVoiceCaptureState
{
	mutable FCriticalSection Mutex;
	TSet<FString> Rooms;
	bool bEnabled = false;
	/** The local capture test (Unreal's own microphone stream, not EOS) is running. Its buffers arrive with an empty room. */
	bool bLocalCapture = false;
	/** bEnabled without the transmit permission: the local capture test sends nothing, so it needs none. */
	bool bLocalEnabled = false;
	bool bVoiceActivation = false;
	bool bPushToTalkHeld = false;
	bool bTest = false;
	float Threshold = 0.02f;
	float Release = 0.28f;
	double HoldUntil = 0.0;
	bool bSpeech = false;
	float Level = 0.0f;
	/** An EOS room, or the empty name for the local capture test. Mutex must be held. */
	bool AcceptsSource(const FString& Room) const { return Room.IsEmpty() ? bLocalCapture : Rooms.Contains(Room); }
	bool IsSourceOpen(const FString& Room) const { return Room.IsEmpty() ? bLocalEnabled : bEnabled; }

	bool Process(const FString& Room, float Rms, double Now)
	{
		FScopeLock Lock(&Mutex);
		if (!AcceptsSource(Room)) return false;
		Level = Rms;
		if (Rms >= Threshold) HoldUntil = Now + Release;
		bSpeech = Rms >= Threshold || Now < HoldUntil;
		return IsSourceOpen(Room) && !bTest && (bVoiceActivation ? bSpeech : bPushToTalkHeld);
	}
	bool IsTalking() const
	{
		FScopeLock Lock(&Mutex);
		return (bEnabled || (bLocalCapture && bLocalEnabled)) && !bTest && bSpeech && FPlatformTime::Seconds() < HoldUntil
			&& (bVoiceActivation || bPushToTalkHeld);
	}

	// Push-to-talk replay: what the key captured, kept for a local playback on release.
	bool bRecordOnPushToTalk = false;
	float MaxRecordSeconds = 15.0f;
	FString RecordRoom;
	bool bTakeStarted = false;
	TArray<int16> Recorded;
	int32 RecordedSampleRate = 0;
	int32 RecordedChannels = 0;

	/**
	 * Keeps the samples push-to-talk captured. The test mode does not stop it, so a take can be
	 * made without sending anything. One source per take: the local capture test while it runs,
	 * otherwise the first EOS room heard after the key went down (EOS calls back once per room,
	 * and every room carries the same microphone).
	 */
	void Record(const FString& Room, TArrayView<const int16> Samples, int32 SampleRate, int32 Channels)
	{
		FScopeLock Lock(&Mutex);
		const bool bTakeSource = bLocalCapture ? Room.IsEmpty() : Rooms.Contains(Room);
		if (!bRecordOnPushToTalk || bVoiceActivation || !bPushToTalkHeld || !bTakeSource || !IsSourceOpen(Room)
			|| SampleRate <= 0 || Channels <= 0)
		{
			return;
		}
		if (!bTakeStarted)
		{
			bTakeStarted = true;
			RecordRoom = Room;
			RecordedSampleRate = SampleRate;
			RecordedChannels = Channels;
			Recorded.Reserve(FMath::CeilToInt32(MaxRecordSeconds * SampleRate * Channels));
		}
		if (Room != RecordRoom || SampleRate != RecordedSampleRate || Channels != RecordedChannels)
		{
			return;
		}
		const int32 Space = FMath::CeilToInt32(MaxRecordSeconds * SampleRate * Channels) - Recorded.Num();
		if (Space > 0)
		{
			Recorded.Append(Samples.GetData(), FMath::Min(Space, Samples.Num()));
		}
	}

	/** Hands the take over and starts the next one empty. */
	void TakeRecording(TArray<int16>& OutSamples, int32& OutSampleRate, int32& OutChannels)
	{
		FScopeLock Lock(&Mutex);
		OutSamples = MoveTemp(Recorded);
		Recorded.Reset();
		OutSampleRate = RecordedSampleRate;
		OutChannels = RecordedChannels;
		RecordRoom.Reset();
		bTakeStarted = false;
		RecordedSampleRate = 0;
		RecordedChannels = 0;
	}
};
