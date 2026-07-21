// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Voice/OnlineVoiceExtendedSteam.h"
#include "Core/OnlineSubsystemExtendedSteam.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace ExtendedSteamVoice
{
#if WITH_EXTENDEDSTEAM_SDK
	/** ISteamUser accessor guarded by the shared module's client-API state; null when unavailable. */
	static ISteamUser* GetSteamUser()
	{
		if (FExtendedSteamSharedModule::IsModuleAvailable() && FExtendedSteamSharedModule::Get().IsSteamClientInitialized())
		{
			return SteamUser();
		}
		return nullptr;
	}
#endif

	/** True while the shared module has the Steam client API up. */
	static bool IsSteamClientUp()
	{
#if WITH_EXTENDEDSTEAM_SDK
		return ExtendedSteamVoice::GetSteamUser() != nullptr;
#else
		return false;
#endif
	}
}

FOnlineVoiceExtendedSteam::~FOnlineVoiceExtendedSteam()
{
	// Never leave the microphone hot if the interface is torn down mid-capture.
	if (bIsRecording)
	{
#if WITH_EXTENDEDSTEAM_SDK
		if (ISteamUser* User = ExtendedSteamVoice::GetSteamUser())
		{
			User->StopVoiceRecording();
		}
#endif
		bIsRecording = false;
	}
}

bool FOnlineVoiceExtendedSteam::Init()
{
	bInitialized = ExtendedSteamVoice::IsSteamClientUp();
	if (!bInitialized)
	{
		// The subsystem only constructs this interface once Steam is up, so this is defensive: a
		// caller that Init()s an interface obtained while Steam was down gets an honest false.
		UE_LOG(LogExtendedSteam, Warning, TEXT("FOnlineVoiceExtendedSteam::Init: Steam client API unavailable; voice capture is inactive"));
		return false;
	}

	UE_LOG(LogExtendedSteam, Verbose, TEXT("FOnlineVoiceExtendedSteam initialized"));
	return true;
}

void FOnlineVoiceExtendedSteam::Shutdown()
{
	StopSteamRecordingIfIdle();
	if (bIsRecording)
	{
#if WITH_EXTENDEDSTEAM_SDK
		if (ISteamUser* User = ExtendedSteamVoice::GetSteamUser())
		{
			User->StopVoiceRecording();
		}
#endif
		bIsRecording = false;
	}

	LocalTalkers.Empty();
	RemoteTalkers.Empty();
	MutedTalkers.Empty();
	bInitialized = false;
}

void FOnlineVoiceExtendedSteam::StartSteamRecording()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!bIsRecording)
	{
		if (ISteamUser* User = ExtendedSteamVoice::GetSteamUser())
		{
			User->StartVoiceRecording();
			bIsRecording = true;
			UE_LOG(LogExtendedSteam, Verbose, TEXT("Voice: started Steam voice recording"));
		}
	}
#endif
}

void FOnlineVoiceExtendedSteam::StopSteamRecordingIfIdle()
{
	// Keep recording while any local talker still wants networked voice.
	for (const TPair<uint32, FLocalTalkerState>& Pair : LocalTalkers)
	{
		if (Pair.Value.bNetworkedVoice)
		{
			return;
		}
	}

#if WITH_EXTENDEDSTEAM_SDK
	if (bIsRecording)
	{
		if (ISteamUser* User = ExtendedSteamVoice::GetSteamUser())
		{
			User->StopVoiceRecording();
		}
		bIsRecording = false;
		UE_LOG(LogExtendedSteam, Verbose, TEXT("Voice: stopped Steam voice recording (no networked local talkers)"));
	}
#endif
}

void FOnlineVoiceExtendedSteam::StartNetworkedVoice(uint8 LocalUserNum)
{
	FLocalTalkerState& Talker = LocalTalkers.FindOrAdd(LocalUserNum);
	Talker.bNetworkedVoice = true;
	StartSteamRecording();
}

void FOnlineVoiceExtendedSteam::StopNetworkedVoice(uint8 LocalUserNum)
{
	if (FLocalTalkerState* Talker = LocalTalkers.Find(LocalUserNum))
	{
		Talker->bNetworkedVoice = false;
	}
	StopSteamRecordingIfIdle();
}

bool FOnlineVoiceExtendedSteam::RegisterLocalTalker(uint32 LocalUserNum)
{
	if (!ExtendedSteamVoice::IsSteamClientUp())
	{
		return false;
	}

	LocalTalkers.FindOrAdd(LocalUserNum).bRegistered = true;
	return true;
}

void FOnlineVoiceExtendedSteam::RegisterLocalTalkers()
{
	// Steam has a single signed-in local user, mapped to local index 0.
	RegisterLocalTalker(0);
}

bool FOnlineVoiceExtendedSteam::UnregisterLocalTalker(uint32 LocalUserNum)
{
	if (FLocalTalkerState* Talker = LocalTalkers.Find(LocalUserNum))
	{
		Talker->bNetworkedVoice = false;
		LocalTalkers.Remove(LocalUserNum);
		StopSteamRecordingIfIdle();
		return true;
	}
	return false;
}

void FOnlineVoiceExtendedSteam::UnregisterLocalTalkers()
{
	LocalTalkers.Empty();
	StopSteamRecordingIfIdle();
}

int32 FOnlineVoiceExtendedSteam::IndexOfId(const TArray<FUniqueNetIdRef>& Ids, const FUniqueNetId& Search)
{
	for (int32 Index = 0; Index < Ids.Num(); ++Index)
	{
		if (*Ids[Index] == Search)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool FOnlineVoiceExtendedSteam::RegisterRemoteTalker(const FUniqueNetId& UniqueId)
{
	if (!UniqueId.IsValid())
	{
		return false;
	}

	if (IndexOfId(RemoteTalkers, UniqueId) == INDEX_NONE)
	{
		RemoteTalkers.Add(UniqueId.AsShared());
	}
	return true;
}

bool FOnlineVoiceExtendedSteam::UnregisterRemoteTalker(const FUniqueNetId& UniqueId)
{
	const int32 Index = IndexOfId(RemoteTalkers, UniqueId);
	if (Index != INDEX_NONE)
	{
		RemoteTalkers.RemoveAt(Index);
		return true;
	}
	return false;
}

void FOnlineVoiceExtendedSteam::RemoveAllRemoteTalkers()
{
	RemoteTalkers.Empty();
}

bool FOnlineVoiceExtendedSteam::IsHeadsetPresent(uint32 LocalUserNum)
{
	// Steam exposes no per-device capture query; a running client is the honest best signal.
	return ExtendedSteamVoice::IsSteamClientUp();
}

bool FOnlineVoiceExtendedSteam::IsLocalPlayerTalking(uint32 LocalUserNum)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bIsRecording)
	{
		if (ISteamUser* User = ExtendedSteamVoice::GetSteamUser())
		{
			uint32 CompressedBytes = 0;
			const EVoiceResult Result = User->GetAvailableVoice(&CompressedBytes);
			return Result == k_EVoiceResultOK && CompressedBytes > 0;
		}
	}
#endif
	return false;
}

bool FOnlineVoiceExtendedSteam::IsRemotePlayerTalking(const FUniqueNetId& UniqueId)
{
	// Stub: remote voice playback is a game-layer concern (see class doc). This interface never
	// receives remote packets, so it cannot know a remote talker's live state.
	return false;
}

bool FOnlineVoiceExtendedSteam::IsMuted(uint32 LocalUserNum, const FUniqueNetId& UniqueId) const
{
	return IndexOfId(MutedTalkers, UniqueId) != INDEX_NONE;
}

bool FOnlineVoiceExtendedSteam::MuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide)
{
	// bIsSystemWide (Steam-account-level mute) is not honored: there is no client API for it here;
	// this is an in-game mute list the game-layer playback path is expected to consult.
	if (!PlayerId.IsValid())
	{
		return false;
	}

	if (IndexOfId(MutedTalkers, PlayerId) == INDEX_NONE)
	{
		MutedTalkers.Add(PlayerId.AsShared());
	}
	return true;
}

bool FOnlineVoiceExtendedSteam::UnmuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide)
{
	const int32 Index = IndexOfId(MutedTalkers, PlayerId);
	if (Index != INDEX_NONE)
	{
		MutedTalkers.RemoveAt(Index);
		return true;
	}
	return false;
}

int32 FOnlineVoiceExtendedSteam::GetNumLocalTalkers()
{
	int32 Count = 0;
	for (const TPair<uint32, FLocalTalkerState>& Pair : LocalTalkers)
	{
		if (Pair.Value.bRegistered)
		{
			++Count;
		}
	}
	return Count;
}

IVoiceEnginePtr FOnlineVoiceExtendedSteam::CreateVoiceEngine()
{
	// No IVoiceEngine: capture is driven directly against ISteamUser (see ReadLocalVoiceData), so
	// there is nothing to create. The base class tolerates a null engine.
	return nullptr;
}

void FOnlineVoiceExtendedSteam::ProcessMuteChangeNotification()
{
	// Nothing to re-evaluate against a voice engine (there is none). The mute list is consulted at
	// playback time by the game layer; this is a no-op beyond that bookkeeping already being current.
}

TSharedPtr<FVoicePacket> FOnlineVoiceExtendedSteam::SerializeRemotePacket(FArchive& Ar)
{
	// Stub: packet wire format is owned by the game-layer voice transport, not by this interface.
	return nullptr;
}

TSharedPtr<FVoicePacket> FOnlineVoiceExtendedSteam::GetLocalPacket(uint32 LocalUserNum)
{
	// Stub: this interface does not packetize or network voice. A game-layer transport calls
	// ReadLocalVoiceData to obtain the compressed capture buffer and wraps it in its own packet type.
	return nullptr;
}

void FOnlineVoiceExtendedSteam::ClearVoicePackets()
{
	// No send queue is owned here (packetization is game-layer); nothing to clear.
}

void FOnlineVoiceExtendedSteam::Tick(float DeltaTime)
{
	// Steam voice capture is poll-based (no callbacks) and FExtendedSteamSharedModule owns the
	// plugin-wide Steam callback pump, so there is nothing to service here.
}

uint32 FOnlineVoiceExtendedSteam::ReadLocalVoiceData(TArray<uint8>& OutCompressedData)
{
	OutCompressedData.Reset();

#if WITH_EXTENDEDSTEAM_SDK
	if (!bIsRecording)
	{
		return 0;
	}

	ISteamUser* User = ExtendedSteamVoice::GetSteamUser();
	if (User == nullptr)
	{
		return 0;
	}

	// Deprecated uncompressed-audio trailing params are left at their defaults (compressed only).
	uint32 CompressedBytes = 0;
	const EVoiceResult AvailableResult = User->GetAvailableVoice(&CompressedBytes);
	if (AvailableResult != k_EVoiceResultOK || CompressedBytes == 0)
	{
		return 0;
	}

	OutCompressedData.SetNumUninitialized(static_cast<int32>(CompressedBytes));
	uint32 BytesWritten = 0;
	const EVoiceResult VoiceResult = User->GetVoice(true, OutCompressedData.GetData(), CompressedBytes, &BytesWritten);
	if (VoiceResult != k_EVoiceResultOK)
	{
		OutCompressedData.Reset();
		return 0;
	}

	OutCompressedData.SetNum(static_cast<int32>(BytesWritten), EAllowShrinking::No);
	return BytesWritten;
#else
	return 0;
#endif
}

bool FOnlineVoiceExtendedSteam::DecompressVoiceData(const TArray<uint8>& CompressedData, TArray<uint8>& OutPcmData, uint32& OutSampleRate) const
{
	OutPcmData.Reset();
	OutSampleRate = 0;

#if WITH_EXTENDEDSTEAM_SDK
	if (CompressedData.Num() == 0)
	{
		return false;
	}

	ISteamUser* User = ExtendedSteamVoice::GetSteamUser();
	if (User == nullptr)
	{
		return false;
	}

	// Decompressing at the optimal rate performs the least CPU processing.
	OutSampleRate = User->GetVoiceOptimalSampleRate();

	// One second of 16-bit mono PCM at the optimal rate is a comfortable starting buffer; if Steam
	// reports the buffer is too small it fills nBytesWritten with the required size, so we retry once.
	uint32 DestSize = OutSampleRate * 2u; // 2 bytes per 16-bit mono sample.
	if (DestSize == 0)
	{
		DestSize = 22050u * 2u;
	}

	OutPcmData.SetNumUninitialized(static_cast<int32>(DestSize));
	uint32 BytesWritten = 0;
	EVoiceResult Result = User->DecompressVoice(
		CompressedData.GetData(), static_cast<uint32>(CompressedData.Num()),
		OutPcmData.GetData(), DestSize, &BytesWritten, OutSampleRate);

	if (Result == k_EVoiceResultBufferTooSmall && BytesWritten > DestSize)
	{
		DestSize = BytesWritten;
		OutPcmData.SetNumUninitialized(static_cast<int32>(DestSize));
		Result = User->DecompressVoice(
			CompressedData.GetData(), static_cast<uint32>(CompressedData.Num()),
			OutPcmData.GetData(), DestSize, &BytesWritten, OutSampleRate);
	}

	if (Result != k_EVoiceResultOK)
	{
		OutPcmData.Reset();
		UE_LOG(LogExtendedSteam, Warning, TEXT("DecompressVoiceData: DecompressVoice failed (result %d)"), static_cast<int32>(Result));
		return false;
	}

	OutPcmData.SetNum(static_cast<int32>(BytesWritten), EAllowShrinking::No);
	return true;
#else
	return false;
#endif
}

FString FOnlineVoiceExtendedSteam::GetVoiceDebugState() const
{
	int32 RegisteredLocal = 0;
	int32 NetworkedLocal = 0;
	for (const TPair<uint32, FLocalTalkerState>& Pair : LocalTalkers)
	{
		RegisteredLocal += Pair.Value.bRegistered ? 1 : 0;
		NetworkedLocal += Pair.Value.bNetworkedVoice ? 1 : 0;
	}

	FString State = FString::Printf(
		TEXT("ExtendedSteam voice: initialized=%s recording=%s localTalkers=%d(networked=%d) remoteTalkers=%d muted=%d"),
		bInitialized ? TEXT("yes") : TEXT("no"),
		bIsRecording ? TEXT("yes") : TEXT("no"),
		RegisteredLocal, NetworkedLocal, RemoteTalkers.Num(), MutedTalkers.Num());

#if WITH_EXTENDEDSTEAM_SDK
	if (ISteamUser* User = ExtendedSteamVoice::GetSteamUser())
	{
		uint32 CompressedBytes = 0;
		const EVoiceResult Available = User->GetAvailableVoice(&CompressedBytes);
		State += FString::Printf(TEXT(" optimalSampleRate=%u availableVoice=%uB(result=%d)"),
			User->GetVoiceOptimalSampleRate(), CompressedBytes, static_cast<int32>(Available));
	}
	else
#endif
	{
		State += TEXT(" [Steam client API unavailable]");
	}

	return State;
}

void FOnlineVoiceExtendedSteam::DumpState() const
{
	UE_LOG(LogExtendedSteam, Log, TEXT("%s"), *GetVoiceDebugState());
}
