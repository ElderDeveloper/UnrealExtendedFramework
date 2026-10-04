// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Shared/EEOSOperationTypes.h"
#include "Shared/EEOSTypes.h"
#include "eos_lobby_types.h"

namespace EEOSCapacity
{
	inline EEOSLobbyPermission ReadPermission(EOS_ELobbyPermissionLevel Permission)
	{
		switch (Permission)
		{
		case EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED: return EEOSLobbyPermission::Public;
		case EOS_ELobbyPermissionLevel::EOS_LPL_JOINVIAPRESENCE: return EEOSLobbyPermission::Presence;
		case EOS_ELobbyPermissionLevel::EOS_LPL_INVITEONLY: return EEOSLobbyPermission::InviteOnly;
		default: return EEOSLobbyPermission::Unknown;
		}
	}

	inline bool IsReservedAttribute(FName Key)
	{
		static const TSet<FName> Keys = {TEXT("NumPublicConnections"), TEXT("NumPrivateConnections"), TEXT("bAntiCheatProtected"),
			TEXT("bUsesStats"), TEXT("bIsDedicated"), TEXT("BuildUniqueId"), TEXT("LOBBYSEARCH"), TEXT("PRESENCESEARCH")};
		return Key == SETTING_HOST_MIGRATION || Keys.Contains(Key);
	}
	/** UE 5.8 CopyLobbyData maps permission-selected slots, whereas AddSearchResult maps
	 * sessions-backend slots into NumOpenPrivateConnections. Never mix these mappings. */
	inline FEEOSCapacitySnapshot Read(const FOnlineSession& Session, EEOSSessionBackend Backend)
	{
		FEEOSCapacitySnapshot Result;
		Result.Backend = Backend;
		Result.RawOpenPublic = Session.NumOpenPublicConnections;
		Result.RawOpenPrivate = Session.NumOpenPrivateConnections;
		const auto& Settings = Session.SessionSettings;
		const int64 Total = int64(Settings.NumPublicConnections) + Settings.NumPrivateConnections;
		if (Settings.NumPublicConnections < 0 || Settings.NumPrivateConnections < 0 || Total <= 0 || Total > MAX_int32) { Result.UnknownReason = TEXT("InvalidMaximumOrNegativeCapacity"); return Result; }
		int32 Slots = -1;
		if (Backend == EEOSSessionBackend::Lobby)
		{
			Slots = Settings.bAllowJoinViaPresence ? Session.NumOpenPublicConnections : Session.NumOpenPrivateConnections;
			Result.Source = Settings.bAllowJoinViaPresence ? TEXT("UE58LobbyPublicSlots") : TEXT("UE58LobbyPrivateSlots");
		}
		else if (Backend == EEOSSessionBackend::Session)
		{
			Slots = Session.NumOpenPrivateConnections;
			Result.Source = TEXT("UE58SessionDetailsSlots");
		}
		else if (Backend == EEOSSessionBackend::LAN)
		{
			const int64 Open = int64(Session.NumOpenPublicConnections) + Session.NumOpenPrivateConnections;
			if (Open >= 0 && Open <= MAX_int32) Slots = int32(Open);
			Result.Source = TEXT("NativeLANSlots");
		}
		Result.Maximum = int32(Total);
		Result.bConsistent = Slots >= 0 && Slots <= Total;
		Result.bKnown = Result.bConsistent && Backend != EEOSSessionBackend::Unknown;
		if (!Result.bKnown) Result.UnknownReason = Backend == EEOSSessionBackend::Unknown ? TEXT("UnknownBackend") : TEXT("AvailableSlotsOutsideMaximum");
		if (Result.bKnown) { Result.AvailableSlots = Slots; Result.Members = Result.Maximum - Slots; }
		return Result;
	}
	inline FEEOSSessionSearchResult Describe(const FOnlineSessionSearchResult& Native, EEOSSessionBackend Backend, int64 Generation, double AgeSeconds = 0)
	{
		FEEOSSessionSearchResult Result;
		Result.SessionId = Native.IsValid() ? Native.GetSessionIdStr() : FString();
		Result.OwnerName = Native.Session.OwningUserName;
		Result.bOwnerKnown = Native.Session.OwningUserId.IsValid() && Native.Session.OwningUserId->IsValid();
		if (Result.bOwnerKnown) Result.OwnerId = Native.Session.OwningUserId->ToString();
		Result.SearchGeneration = Generation; Result.SnapshotAgeSeconds = AgeSeconds;
		Result.Capacity = Read(Native.Session, Backend);
		Result.Capacity.SearchGeneration = Generation; Result.Capacity.SnapshotAgeSeconds = AgeSeconds;
		Result.CurrentPlayers = Result.Capacity.Members; Result.MaxPlayers = Result.Capacity.Maximum;
		Result.Ping = Native.PingInMs;
		Result.bNativeFlagsKnown = Native.IsValid();
		const auto& Settings = Native.Session.SessionSettings;
		Result.bIsDedicatedServer = Settings.bIsDedicated; Result.bShouldAdvertise = Settings.bShouldAdvertise;
		Result.bAllowInvites = Settings.bAllowInvites; Result.bAllowJoinViaPresence = Settings.bAllowJoinViaPresence;
		Result.bAllowJoinInProgress = Settings.bAllowJoinInProgress;
		for (const auto& Field : Settings.Settings)
		{
			Result.Settings.Add(Field.Key, Field.Value.Data.ToString());
			if (!IsReservedAttribute(Field.Key)) Result.ApplicationAttributes.Add(Field.Key, Field.Value.Data.ToString());
		}
		return Result;
	}

}
