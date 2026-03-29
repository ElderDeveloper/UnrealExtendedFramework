// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSVoiceSubsystem.h"
#include "Shared/EEOSSettings.h"
#include "OnlineSubsystemUtils.h"
#include "VoiceChat.h"
#include "UnrealExtendedEOS.h"

void UEEOSVoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UEEOSSettings* Settings = GetEOSSettings();
	if (Settings && !Settings->bEnableVoiceChat)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Voice chat is disabled in settings"));
		return;
	}

	// Create and cache the voice chat user once during initialization
	IVoiceChat* VoiceChat = IVoiceChat::Get();
	if (VoiceChat)
	{
		CachedVoiceChatUser = VoiceChat->CreateUser();
		if (CachedVoiceChatUser)
		{
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Voice chat user created and cached"));

			// Login the voice chat user with the EOS identity
			if (IsEOSAvailable())
			{
				IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
				IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
				if (!IdentityInterface.IsValid())
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: Identity interface not available — voice login deferred"));
				}
				else
				{
					FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
					if (UserId.IsValid())
					{
						FString PlayerName = IdentityInterface->GetPlayerNickname(0);
						FPlatformUserId PlatformUserId = FPlatformMisc::GetPlatformUserForUserIndex(0);
						CachedVoiceChatUser->Login(PlatformUserId, PlayerName, TEXT(""),
							FOnVoiceChatLoginCompleteDelegate::CreateLambda(
								[this](const FString& LoggedInUserId, const FVoiceChatResult& Result)
								{
									if (Result.IsSuccess())
									{
										bVoiceUserLoggedIn = true;
										UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Voice chat user logged in as '%s'"), *LoggedInUserId);
									}
									else
									{
										UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: Voice chat user login failed — %s"), *Result.ErrorDesc);
									}
								}));
					}
				}
			}
		}
		else
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: Failed to create voice chat user"));
		}
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: IVoiceChat interface not available — voice will not function"));
	}
}

void UEEOSVoiceSubsystem::Deinitialize()
{
	LeaveAllVoiceRooms();

	// Logout and release the voice chat user
	if (CachedVoiceChatUser)
	{
		if (bVoiceUserLoggedIn)
		{
			// Capture the user pointer for the callback — release only after logout completes
			IVoiceChatUser* UserToRelease = CachedVoiceChatUser;
			CachedVoiceChatUser = nullptr;
			bVoiceUserLoggedIn = false;

			UserToRelease->Logout(
				FOnVoiceChatLogoutCompleteDelegate::CreateLambda(
					[UserToRelease](const FString& UserId, const FVoiceChatResult& Result)
					{
						UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Voice chat user logged out — %s"),
							Result.IsSuccess() ? TEXT("success") : TEXT("failed"));

						// Now safe to release — logout flow is complete
						IVoiceChat* VoiceChat = IVoiceChat::Get();
						if (VoiceChat)
						{
							VoiceChat->ReleaseUser(UserToRelease);
						}
					}));
		}
		else
		{
			// Not logged in — safe to release immediately
			IVoiceChat* VoiceChat = IVoiceChat::Get();
			if (VoiceChat)
			{
				VoiceChat->ReleaseUser(CachedVoiceChatUser);
			}
			CachedVoiceChatUser = nullptr;
			bVoiceUserLoggedIn = false;
		}
	}

	Super::Deinitialize();
}

IVoiceChatUser* UEEOSVoiceSubsystem::GetCachedVoiceChatUser() const
{
	return CachedVoiceChatUser;
}

// ── Room Management ──────────────────────────────────────────────────────────

void UEEOSVoiceSubsystem::JoinVoiceRoom(const FString& RoomName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("JoinVoiceRoom"));
		return;
	}

	if (JoinedRooms.Contains(RoomName))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: Already in room '%s'"), *RoomName);
		return;
	}

	if (!CachedVoiceChatUser)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSVoiceSubsystem::JoinVoiceRoom — No voice chat user available. Voice will not function."));
		// DO NOT add to JoinedRooms or broadcast success — voice is not live
		return;
	}

	if (!bVoiceUserLoggedIn)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem::JoinVoiceRoom — Voice user not logged in yet. Cannot join room '%s'."), *RoomName);
		return;
	}

	CachedVoiceChatUser->JoinChannel(RoomName, TEXT(""), EVoiceChatChannelType::NonPositional,
		FOnVoiceChatChannelJoinCompleteDelegate::CreateLambda(
			[this, RoomName](const FString& ChannelName, const FVoiceChatResult& Result)
			{
				if (Result.IsSuccess())
				{
					JoinedRooms.Add(ChannelName);
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Joined room '%s'"), *ChannelName);
					OnVoiceRoomJoined.Broadcast(ChannelName);
				}
				else
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: Failed to join room '%s' — %s"), *RoomName, *Result.ErrorDesc);
					// Do NOT broadcast OnVoiceRoomJoined on failure
				}
			}));
}

void UEEOSVoiceSubsystem::LeaveVoiceRoom(const FString& RoomName)
{
	if (!JoinedRooms.Contains(RoomName))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem::LeaveVoiceRoom — Not in room '%s'"), *RoomName);
		return;
	}

	if (CachedVoiceChatUser)
	{
		CachedVoiceChatUser->LeaveChannel(RoomName,
			FOnVoiceChatChannelLeaveCompleteDelegate::CreateLambda(
				[this, RoomName](const FString& ChannelName, const FVoiceChatResult& Result)
				{
					if (Result.IsSuccess())
					{
						JoinedRooms.Remove(RoomName);
						OnVoiceRoomLeft.Broadcast(RoomName);
						UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Left room '%s' — success"), *ChannelName);
					}
					else
					{
						UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: Failed to leave room '%s' — %s (local state preserved)"),
							*ChannelName, *Result.ErrorDesc);
					}
				}));
	}
	else
	{
		// No voice user — just clean up local state
		JoinedRooms.Remove(RoomName);
		OnVoiceRoomLeft.Broadcast(RoomName);
	}
}

void UEEOSVoiceSubsystem::LeaveAllVoiceRooms()
{
	TArray<FString> RoomsCopy = JoinedRooms.Array();
	for (const FString& Room : RoomsCopy)
	{
		LeaveVoiceRoom(Room);
	}
}

// ── Per-Player Controls ──────────────────────────────────────────────────────

void UEEOSVoiceSubsystem::MutePlayer(const FString& UserId)
{
	if (!CachedVoiceChatUser)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem::MutePlayer — No voice chat user"));
		return;
	}

	CachedVoiceChatUser->SetPlayerMuted(UserId, true);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Muted '%s'"), *UserId);
}

void UEEOSVoiceSubsystem::UnmutePlayer(const FString& UserId)
{
	if (!CachedVoiceChatUser)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem::UnmutePlayer — No voice chat user"));
		return;
	}

	CachedVoiceChatUser->SetPlayerMuted(UserId, false);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Unmuted '%s'"), *UserId);
}

void UEEOSVoiceSubsystem::SetPlayerVolume(const FString& UserId, float Volume)
{
	Volume = FMath::Clamp(Volume, 0.f, 2.f);
	if (CachedVoiceChatUser)
	{
		CachedVoiceChatUser->SetPlayerVolume(UserId, Volume);
	}
}

float UEEOSVoiceSubsystem::GetPlayerVolume(const FString& UserId) const
{
	if (CachedVoiceChatUser)
	{
		return CachedVoiceChatUser->GetPlayerVolume(UserId);
	}
	return 1.0f;
}

bool UEEOSVoiceSubsystem::IsPlayerTalking(const FString& UserId) const
{
	if (CachedVoiceChatUser)
	{
		return CachedVoiceChatUser->IsPlayerTalking(UserId);
	}
	return false;
}

bool UEEOSVoiceSubsystem::IsPlayerMuted(const FString& UserId) const
{
	if (CachedVoiceChatUser)
	{
		return CachedVoiceChatUser->IsPlayerMuted(UserId);
	}
	return false;
}

// ── Volume & Muting ──────────────────────────────────────────────────────────

void UEEOSVoiceSubsystem::SetOutputVolume(float Volume)
{
	Volume = FMath::Clamp(Volume, 0.f, 1.f);
	if (CachedVoiceChatUser)
	{
		CachedVoiceChatUser->SetAudioOutputVolume(Volume);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Output volume set to %.2f"), Volume);
	}
}

void UEEOSVoiceSubsystem::SetInputVolume(float Volume)
{
	Volume = FMath::Clamp(Volume, 0.f, 1.f);
	CurrentInputVolume = Volume;

	if (CachedVoiceChatUser)
	{
		CachedVoiceChatUser->SetAudioInputVolume(Volume);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Input volume set to %.2f"), Volume);
	}
}

float UEEOSVoiceSubsystem::GetInputVolume() const
{
	return CurrentInputVolume;
}

void UEEOSVoiceSubsystem::SetLocalMuted(bool bMuted)
{
	bLocalMuted = bMuted;

	if (CachedVoiceChatUser)
	{
		CachedVoiceChatUser->SetAudioInputDeviceMuted(bMuted);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Local muted = %s"), bMuted ? TEXT("true") : TEXT("false"));
	}
}

// ── Transmit Modes ───────────────────────────────────────────────────────────

void UEEOSVoiceSubsystem::TransmitToAllRooms()
{
	if (CachedVoiceChatUser)
	{
		CachedVoiceChatUser->TransmitToAllChannels();
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Transmitting to all rooms"));
	}
}

void UEEOSVoiceSubsystem::TransmitToSelectedRoom(const FString& RoomName)
{
	if (CachedVoiceChatUser)
	{
		TSet<FString> Channels;
		Channels.Add(RoomName);
		CachedVoiceChatUser->TransmitToSpecificChannels(Channels);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Transmitting to '%s'"), *RoomName);
	}
}

void UEEOSVoiceSubsystem::TransmitToNoRoom()
{
	if (CachedVoiceChatUser)
	{
		CachedVoiceChatUser->TransmitToNoChannels();
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Stopped transmitting"));
	}
}

// ── Device Management ────────────────────────────────────────────────────────

TArray<FEEOSVoiceDeviceInfo> UEEOSVoiceSubsystem::GetInputDevices() const
{
	TArray<FEEOSVoiceDeviceInfo> Devices;

	if (CachedVoiceChatUser)
	{
		TArray<FVoiceChatDeviceInfo> RawDevices = CachedVoiceChatUser->GetAvailableInputDeviceInfos();
		for (const auto& RawDevice : RawDevices)
		{
			FEEOSVoiceDeviceInfo Info;
			Info.DeviceId = RawDevice.Id;
			Info.DisplayName = RawDevice.DisplayName;
			Devices.Add(Info);
		}
	}

	return Devices;
}

TArray<FEEOSVoiceDeviceInfo> UEEOSVoiceSubsystem::GetOutputDevices() const
{
	TArray<FEEOSVoiceDeviceInfo> Devices;

	if (CachedVoiceChatUser)
	{
		TArray<FVoiceChatDeviceInfo> RawDevices = CachedVoiceChatUser->GetAvailableOutputDeviceInfos();
		for (const auto& RawDevice : RawDevices)
		{
			FEEOSVoiceDeviceInfo Info;
			Info.DeviceId = RawDevice.Id;
			Info.DisplayName = RawDevice.DisplayName;
			Devices.Add(Info);
		}
	}

	return Devices;
}

void UEEOSVoiceSubsystem::SetInputDevice(const FString& DeviceId)
{
	if (CachedVoiceChatUser)
	{
		CachedVoiceChatUser->SetInputDeviceId(DeviceId);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Input device set to '%s'"), *DeviceId);
	}
}

void UEEOSVoiceSubsystem::SetOutputDevice(const FString& DeviceId)
{
	if (CachedVoiceChatUser)
	{
		CachedVoiceChatUser->SetOutputDeviceId(DeviceId);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Output device set to '%s'"), *DeviceId);
	}
}

// ── Queries ──────────────────────────────────────────────────────────────────

bool UEEOSVoiceSubsystem::IsInVoiceRoom() const
{
	return JoinedRooms.Num() > 0;
}

bool UEEOSVoiceSubsystem::IsInRoom(const FString& RoomName) const
{
	return JoinedRooms.Contains(RoomName);
}

FString UEEOSVoiceSubsystem::GetCurrentRoomName() const
{
	if (JoinedRooms.Num() > 0)
	{
		for (const FString& Room : JoinedRooms)
		{
			return Room; // Return first room
		}
	}
	return FString();
}

bool UEEOSVoiceSubsystem::IsLocalMuted() const
{
	return bLocalMuted;
}

TArray<FString> UEEOSVoiceSubsystem::GetPlayersInRoom(const FString& RoomName) const
{
	TArray<FString> Players;

	if (CachedVoiceChatUser && !RoomName.IsEmpty())
	{
		Players = CachedVoiceChatUser->GetPlayersInChannel(RoomName);
	}

	return Players;
}

TArray<FString> UEEOSVoiceSubsystem::GetJoinedRooms() const
{
	TArray<FString> Rooms;

	if (CachedVoiceChatUser)
	{
		Rooms = CachedVoiceChatUser->GetChannels();
	}
	else
	{
		Rooms = JoinedRooms.Array();
	}

	return Rooms;
}
