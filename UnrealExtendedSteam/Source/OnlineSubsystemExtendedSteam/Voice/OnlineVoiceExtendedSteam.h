// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/VoiceInterface.h"
#include "OnlineSubsystemTypes.h"
#include "Core/OnlineSubsystemExtendedSteamPackage.h"

class FOnlineSubsystemExtendedSteam;

/**
 * IOnlineVoice backed by ISteamUser voice capture for the "EXTENDEDSTEAM" online subsystem.
 *
 * Scope: this is a THIN LOCAL-CAPTURE interface. It owns exactly what Steam owns natively — the
 * local microphone recording state — and exposes it through the engine voice interface:
 *  - StartNetworkedVoice / StopNetworkedVoice toggle ISteamUser::StartVoiceRecording /
 *    StopVoiceRecording for the local talker (push-to-talk).
 *  - IsLocalPlayerTalking queries ISteamUser::GetAvailableVoice (there is compressed voice ready).
 *  - IsHeadsetPresent reports whether the Steam client API is up (Steam has no per-device query;
 *    a running client with a capture device is the honest best signal here).
 *  - ReadLocalVoiceData / DecompressVoiceData are the capture/decompress primitives (GetVoice /
 *    DecompressVoice / GetVoiceOptimalSampleRate) a game-layer voice transport calls per frame.
 *  - Register/Unregister local & remote talkers and the mute list are minimal bookkeeping.
 *
 * Out of scope, and intentionally stubbed (documented per method): per-packet voice transport and
 * remote-talker playback. Steam voice provides only local capture and a decompressor; deciding how
 * captured packets are networked, ordered, mixed and played back is a game-layer concern, so
 * CreateVoiceEngine, GetLocalPacket, SerializeRemotePacket, IsRemotePlayerTalking and the audio
 * endpoint patching methods return null / false / empty.
 *
 * Single local user: Steam has exactly one signed-in user, so LocalUserNum bookkeeping tolerates
 * any index but recording is a single global capture stream.
 *
 * Callbacks: Steam voice capture is poll-based (no callbacks), so Tick pumps nothing — and, per the
 * plugin-wide contract, FExtendedSteamSharedModule owns the Steam callback pump regardless.
 * Game-thread only, like the rest of the subsystem. All SDK code lives behind WITH_EXTENDEDSTEAM_SDK
 * in the cpp; this header pulls in no Steamworks types.
 */
class FOnlineVoiceExtendedSteam : public IOnlineVoice
{
public:
	explicit FOnlineVoiceExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem)
		: Subsystem(InSubsystem)
	{
	}

	virtual ~FOnlineVoiceExtendedSteam();

	//~ Begin IOnlineVoice
	virtual bool Init() override;
	virtual void Shutdown() override;
	virtual void StartNetworkedVoice(uint8 LocalUserNum) override;
	virtual void StopNetworkedVoice(uint8 LocalUserNum) override;
	virtual bool RegisterLocalTalker(uint32 LocalUserNum) override;
	virtual void RegisterLocalTalkers() override;
	virtual bool UnregisterLocalTalker(uint32 LocalUserNum) override;
	virtual void UnregisterLocalTalkers() override;
	virtual bool RegisterRemoteTalker(const FUniqueNetId& UniqueId) override;
	virtual bool UnregisterRemoteTalker(const FUniqueNetId& UniqueId) override;
	virtual void RemoveAllRemoteTalkers() override;
	virtual bool IsHeadsetPresent(uint32 LocalUserNum) override;
	virtual bool IsLocalPlayerTalking(uint32 LocalUserNum) override;
	virtual bool IsRemotePlayerTalking(const FUniqueNetId& UniqueId) override;
	virtual bool IsMuted(uint32 LocalUserNum, const FUniqueNetId& UniqueId) const override;
	virtual bool MuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide) override;
	virtual bool UnmuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide) override;
	virtual TSharedPtr<class FVoicePacket> SerializeRemotePacket(FArchive& Ar) override;
	virtual TSharedPtr<class FVoicePacket> GetLocalPacket(uint32 LocalUserNum) override;
	virtual int32 GetNumLocalTalkers() override;
	virtual void ClearVoicePackets() override;
	virtual void Tick(float DeltaTime) override;
	virtual FString GetVoiceDebugState() const override;
	//~ End IOnlineVoice

	// ---------------------------------------------------------------------------------------
	// Local-capture primitives (not part of IOnlineVoice). A game-layer voice transport drives
	// these each frame: the sender reads compressed voice and ships it; the receiver decompresses
	// it before handing PCM to its own audio playback. Kept here so the Steam voice codec stays
	// encapsulated behind this interface even though transport is game-owned.
	// ---------------------------------------------------------------------------------------

	/**
	 * Reads all currently-available compressed voice for the local user (ISteamUser::GetAvailableVoice
	 * + GetVoice). Returns the number of compressed bytes written into OutCompressedData (0 when the
	 * user is not recording or no voice is ready). OutCompressedData is always reset first.
	 */
	uint32 ReadLocalVoiceData(TArray<uint8>& OutCompressedData);

	/**
	 * Decompresses Steam-compressed voice (as produced by ReadLocalVoiceData on the sender) into
	 * 16-bit signed PCM at the Steam-optimal sample rate (ISteamUser::DecompressVoice /
	 * GetVoiceOptimalSampleRate). Returns true on success; OutSampleRate carries the PCM rate.
	 */
	bool DecompressVoiceData(const TArray<uint8>& CompressedData, TArray<uint8>& OutPcmData, uint32& OutSampleRate) const;

	/** Human-readable one-block dump of the voice state to the log (LogExtendedSteam). */
	void DumpState() const;

protected:
	//~ Begin IOnlineVoice (PACKAGE_SCOPE)
	/** No IVoiceEngine: this interface talks to ISteamUser directly, so there is no engine to create. */
	virtual IVoiceEnginePtr CreateVoiceEngine() override;

	/** Re-evaluates the mute list. Steam voice playback is game-owned, so this is local bookkeeping only. */
	virtual void ProcessMuteChangeNotification() override;
	//~ End IOnlineVoice (PACKAGE_SCOPE)

private:
	/** Per-local-talker state. Steam has one local user, but the interface is keyed by LocalUserNum. */
	struct FLocalTalkerState
	{
		/** True once RegisterLocalTalker succeeded for this index. */
		bool bRegistered = false;

		/** True while StartNetworkedVoice is active for this index (push-to-talk transmitting). */
		bool bNetworkedVoice = false;
	};

	/** Starts Steam voice recording if it is not already running and the client API is up. */
	void StartSteamRecording();

	/** Stops Steam voice recording once no local talker still wants networked voice. */
	void StopSteamRecordingIfIdle();

	/** Index into RemoteTalkers / MutedTalkers of an id equal to Search, or INDEX_NONE. */
	static int32 IndexOfId(const TArray<FUniqueNetIdRef>& Ids, const FUniqueNetId& Search);

	/** Owning subsystem (owns this object; outlives it). */
	FOnlineSubsystemExtendedSteam* Subsystem = nullptr;

	/** True between Init and Shutdown. */
	bool bInitialized = false;

	/** True while ISteamUser voice recording is active (a single global capture stream). */
	bool bIsRecording = false;

	/** Registered local talkers keyed by LocalUserNum. GetNumLocalTalkers counts these. */
	TMap<uint32, FLocalTalkerState> LocalTalkers;

	/** Registered remote talkers (bookkeeping only; playback is a game-layer concern). */
	TArray<FUniqueNetIdRef> RemoteTalkers;

	/** Remote talkers muted by the local user (single local user, so not keyed by LocalUserNum). */
	TArray<FUniqueNetIdRef> MutedTalkers;
};

typedef TSharedPtr<FOnlineVoiceExtendedSteam, ESPMode::ThreadSafe> FOnlineVoiceExtendedSteamPtr;
