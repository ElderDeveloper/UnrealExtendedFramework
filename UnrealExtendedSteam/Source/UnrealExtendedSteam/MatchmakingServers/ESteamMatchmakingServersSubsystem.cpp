// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "MatchmakingServers/ESteamMatchmakingServersSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	/** Parses "a.b.c.d" into a host-byte-order uint32 (the format ISteamMatchmakingServers expects). */
	bool ParseIPv4(const FString& Ip, uint32& OutHostOrderIp)
	{
		TArray<FString> Octets;
		Ip.ParseIntoArray(Octets, TEXT("."), /*bCullEmpty*/ false);
		if (Octets.Num() != 4)
		{
			return false;
		}

		uint32 Result = 0;
		for (const FString& Octet : Octets)
		{
			if (Octet.IsEmpty() || !Octet.IsNumeric())
			{
				return false;
			}
			const int32 Value = FCString::Atoi(*Octet);
			if (Value < 0 || Value > 255)
			{
				return false;
			}
			Result = (Result << 8) | static_cast<uint32>(Value);
		}

		OutHostOrderIp = Result;
		return true;
	}

	FESteamServerInfo ToServerInfo(const gameserveritem_t& Item)
	{
		FESteamServerInfo Info;
		Info.Address = UTF8_TO_TCHAR(Item.m_NetAdr.GetQueryAddressString());
		Info.GamePort = Item.m_NetAdr.GetConnectionPort();
		Info.Ping = Item.m_nPing;
		Info.Name = UTF8_TO_TCHAR(Item.GetName());
		Info.Map = UTF8_TO_TCHAR(Item.m_szMap);
		Info.GameDescription = UTF8_TO_TCHAR(Item.m_szGameDescription);
		Info.GameDir = UTF8_TO_TCHAR(Item.m_szGameDir);
		Info.Tags = UTF8_TO_TCHAR(Item.m_szGameTags);
		Info.Players = Item.m_nPlayers;
		Info.MaxPlayers = Item.m_nMaxPlayers;
		Info.BotPlayers = Item.m_nBotPlayers;
		Info.bPassworded = Item.m_bPassword;
		Info.bSecure = Item.m_bSecure;
		Info.ServerVersion = Item.m_nServerVersion;
		Info.SteamId = FESteamId(Item.m_steamID.ConvertToUint64());
		return Info;
	}

	/** Builds the MatchMakingKeyValuePair_t array for a filter map. */
	TArray<MatchMakingKeyValuePair_t> BuildFilterPairs(const TMap<FString, FString>& Filters)
	{
		TArray<MatchMakingKeyValuePair_t> Pairs;
		Pairs.Reserve(Filters.Num());
		for (const TPair<FString, FString>& Filter : Filters)
		{
			Pairs.Emplace(TCHAR_TO_UTF8(*Filter.Key), TCHAR_TO_UTF8(*Filter.Value));
		}
		return Pairs;
	}
}

/**
 * Owns the ISteamMatchmakingServers response objects and active query handles.
 *
 * The Steam server browser calls back into raw interface pointers, so the response objects
 * MUST outlive their requests: this holder lives on the subsystem (TUniquePtr) for as long as
 * the Steam client API is up, and its destructor cancels every in-flight query before the
 * response objects go away.
 */
class FESteamServerBrowserQueries
{
public:
	enum class EServerListType : uint8
	{
		Internet,
		LAN,
		Friends,
		Favorites,
		History,
		Spectator
	};

	explicit FESteamServerBrowserQueries(UESteamMatchmakingServersSubsystem* InOwner)
		: Owner(InOwner)
		, ListResponse(*this)
		, PingResponse(*this)
		, RulesResponse(*this)
		, PlayersResponse(*this)
	{
	}

	~FESteamServerBrowserQueries()
	{
		CancelServerList();
		CancelPing();
		CancelRules();
		CancelPlayers();
	}

	bool StartServerList(EServerListType Type, const TMap<FString, FString>& Filters, AppId_t AppIdOverride = 0)
	{
		ISteamMatchmakingServers* Servers = SteamMatchmakingServers();
		if (!Servers || !SteamUtils())
		{
			return false;
		}

		// One in-flight list request: drop the previous one (its complete delegate never fires).
		CancelServerList();
		RespondedCount = 0;

		const AppId_t AppId = AppIdOverride != 0 ? AppIdOverride : SteamUtils()->GetAppID();
		TArray<MatchMakingKeyValuePair_t> Pairs = BuildFilterPairs(Filters);
		MatchMakingKeyValuePair_t* PairsData = Pairs.GetData();
		const uint32 PairCount = static_cast<uint32>(Pairs.Num());

		switch (Type)
		{
		case EServerListType::Internet:
			ActiveListRequest = Servers->RequestInternetServerList(AppId, &PairsData, PairCount, &ListResponse);
			break;
		case EServerListType::LAN:
			ActiveListRequest = Servers->RequestLANServerList(AppId, &ListResponse);
			break;
		case EServerListType::Friends:
			ActiveListRequest = Servers->RequestFriendsServerList(AppId, &PairsData, PairCount, &ListResponse);
			break;
		case EServerListType::Favorites:
			ActiveListRequest = Servers->RequestFavoritesServerList(AppId, &PairsData, PairCount, &ListResponse);
			break;
		case EServerListType::History:
			ActiveListRequest = Servers->RequestHistoryServerList(AppId, &PairsData, PairCount, &ListResponse);
			break;
		case EServerListType::Spectator:
			ActiveListRequest = Servers->RequestSpectatorServerList(AppId, &PairsData, PairCount, &ListResponse);
			break;
		}

		return ActiveListRequest != nullptr;
	}

	void CancelServerList()
	{
		if (ActiveListRequest)
		{
			if (ISteamMatchmakingServers* Servers = SteamMatchmakingServers())
			{
				// Cancelling does not release the handle; both are required.
				Servers->CancelQuery(ActiveListRequest);
				Servers->ReleaseRequest(ActiveListRequest);
			}
			ActiveListRequest = nullptr;
		}
	}

	bool StartPing(uint32 HostOrderIp, uint16 Port)
	{
		ISteamMatchmakingServers* Servers = SteamMatchmakingServers();
		if (!Servers)
		{
			return false;
		}

		CancelPing();
		ActivePingQuery = Servers->PingServer(HostOrderIp, Port, &PingResponse);
		return ActivePingQuery != HSERVERQUERY_INVALID;
	}

	bool StartRules(uint32 HostOrderIp, uint16 Port)
	{
		ISteamMatchmakingServers* Servers = SteamMatchmakingServers();
		if (!Servers)
		{
			return false;
		}

		CancelRules();
		RuleKeys.Reset();
		RuleValues.Reset();
		ActiveRulesQuery = Servers->ServerRules(HostOrderIp, Port, &RulesResponse);
		return ActiveRulesQuery != HSERVERQUERY_INVALID;
	}

	bool StartPlayerDetails(uint32 HostOrderIp, uint16 Port)
	{
		ISteamMatchmakingServers* Servers = SteamMatchmakingServers();
		if (!Servers)
		{
			return false;
		}

		CancelPlayers();
		ActivePlayersQuery = Servers->PlayerDetails(HostOrderIp, Port, &PlayersResponse);
		return ActivePlayersQuery != HSERVERQUERY_INVALID;
	}

private:
	void CancelPing()
	{
		if (ActivePingQuery != HSERVERQUERY_INVALID)
		{
			if (ISteamMatchmakingServers* Servers = SteamMatchmakingServers())
			{
				Servers->CancelServerQuery(ActivePingQuery);
			}
			ActivePingQuery = HSERVERQUERY_INVALID;
		}
	}

	void CancelRules()
	{
		if (ActiveRulesQuery != HSERVERQUERY_INVALID)
		{
			if (ISteamMatchmakingServers* Servers = SteamMatchmakingServers())
			{
				Servers->CancelServerQuery(ActiveRulesQuery);
			}
			ActiveRulesQuery = HSERVERQUERY_INVALID;
		}
	}

	void CancelPlayers()
	{
		if (ActivePlayersQuery != HSERVERQUERY_INVALID)
		{
			if (ISteamMatchmakingServers* Servers = SteamMatchmakingServers())
			{
				Servers->CancelServerQuery(ActivePlayersQuery);
			}
			ActivePlayersQuery = HSERVERQUERY_INVALID;
		}
	}

	// ---- Server list response forwarding ----

	void HandleServerResponded(HServerListRequest Request, int32 ServerIndex)
	{
		if (Request != ActiveListRequest)
		{
			return;
		}

		ISteamMatchmakingServers* Servers = SteamMatchmakingServers();
		UESteamMatchmakingServersSubsystem* Subsystem = Owner.Get();
		if (!Servers || !Subsystem)
		{
			return;
		}

		if (const gameserveritem_t* Item = Servers->GetServerDetails(Request, ServerIndex))
		{
			++RespondedCount;
			Subsystem->OnServerListServerResponded.Broadcast(ToServerInfo(*Item));
		}
	}

	void HandleRefreshComplete(HServerListRequest Request, EMatchMakingServerResponse Response)
	{
		if (Request != ActiveListRequest)
		{
			return;
		}

		ActiveListRequest = nullptr;
		if (ISteamMatchmakingServers* Servers = SteamMatchmakingServers())
		{
			Servers->ReleaseRequest(Request);
		}

		if (UESteamMatchmakingServersSubsystem* Subsystem = Owner.Get())
		{
			// "No servers listed" is a successful (empty) result, not a failure.
			Subsystem->OnServerListComplete.Broadcast(Response != eServerFailedToRespond, RespondedCount);
		}
	}

	// ---- Ping response forwarding ----

	void HandlePingResponded(gameserveritem_t& Server)
	{
		ActivePingQuery = HSERVERQUERY_INVALID;
		if (UESteamMatchmakingServersSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnServerPingResponded.Broadcast(true, ToServerInfo(Server));
		}
	}

	void HandlePingFailed()
	{
		ActivePingQuery = HSERVERQUERY_INVALID;
		if (UESteamMatchmakingServersSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnServerPingResponded.Broadcast(false, FESteamServerInfo());
		}
	}

	// ---- Rules response forwarding ----

	void HandleRuleResponded(const char* Rule, const char* Value)
	{
		RuleKeys.Add(UTF8_TO_TCHAR(Rule));
		RuleValues.Add(UTF8_TO_TCHAR(Value));
	}

	void HandleRulesComplete(bool bSuccess)
	{
		ActiveRulesQuery = HSERVERQUERY_INVALID;
		if (UESteamMatchmakingServersSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnServerRulesReceived.Broadcast(bSuccess, RuleKeys, RuleValues);
		}
		RuleKeys.Reset();
		RuleValues.Reset();
	}

	// ---- Player details response forwarding ----

	void HandlePlayerResponded(const char* Name, int32 Score, float TimePlayed)
	{
		if (UESteamMatchmakingServersSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnServerPlayerResponded.Broadcast(UTF8_TO_TCHAR(Name), Score, TimePlayed);
		}
	}

	void HandlePlayersComplete(bool bSuccess)
	{
		ActivePlayersQuery = HSERVERQUERY_INVALID;
		if (UESteamMatchmakingServersSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnServerPlayerListComplete.Broadcast(bSuccess);
		}
	}

	// ---- Steam response interface implementations ----

	class FListResponse final : public ISteamMatchmakingServerListResponse
	{
	public:
		explicit FListResponse(FESteamServerBrowserQueries& InHolder) : Holder(InHolder) {}

		virtual void ServerResponded(HServerListRequest hRequest, int iServer) override
		{
			Holder.HandleServerResponded(hRequest, iServer);
		}

		virtual void ServerFailedToRespond(HServerListRequest hRequest, int iServer) override
		{
			// Unresponsive entries are not forwarded; the final complete broadcast reports the responsive count.
		}

		virtual void RefreshComplete(HServerListRequest hRequest, EMatchMakingServerResponse response) override
		{
			Holder.HandleRefreshComplete(hRequest, response);
		}

	private:
		FESteamServerBrowserQueries& Holder;
	};

	class FPingResponse final : public ISteamMatchmakingPingResponse
	{
	public:
		explicit FPingResponse(FESteamServerBrowserQueries& InHolder) : Holder(InHolder) {}

		virtual void ServerResponded(gameserveritem_t& server) override
		{
			Holder.HandlePingResponded(server);
		}

		virtual void ServerFailedToRespond() override
		{
			Holder.HandlePingFailed();
		}

	private:
		FESteamServerBrowserQueries& Holder;
	};

	class FRulesResponse final : public ISteamMatchmakingRulesResponse
	{
	public:
		explicit FRulesResponse(FESteamServerBrowserQueries& InHolder) : Holder(InHolder) {}

		virtual void RulesResponded(const char* pchRule, const char* pchValue) override
		{
			Holder.HandleRuleResponded(pchRule, pchValue);
		}

		virtual void RulesFailedToRespond() override
		{
			Holder.HandleRulesComplete(false);
		}

		virtual void RulesRefreshComplete() override
		{
			Holder.HandleRulesComplete(true);
		}

	private:
		FESteamServerBrowserQueries& Holder;
	};

	class FPlayersResponse final : public ISteamMatchmakingPlayersResponse
	{
	public:
		explicit FPlayersResponse(FESteamServerBrowserQueries& InHolder) : Holder(InHolder) {}

		virtual void AddPlayerToList(const char* pchName, int nScore, float flTimePlayed) override
		{
			Holder.HandlePlayerResponded(pchName, nScore, flTimePlayed);
		}

		virtual void PlayersFailedToRespond() override
		{
			Holder.HandlePlayersComplete(false);
		}

		virtual void PlayersRefreshComplete() override
		{
			Holder.HandlePlayersComplete(true);
		}

	private:
		FESteamServerBrowserQueries& Holder;
	};

	TWeakObjectPtr<UESteamMatchmakingServersSubsystem> Owner;

	FListResponse ListResponse;
	FPingResponse PingResponse;
	FRulesResponse RulesResponse;
	FPlayersResponse PlayersResponse;

	HServerListRequest ActiveListRequest = nullptr;
	int32 RespondedCount = 0;
	HServerQuery ActivePingQuery = HSERVERQUERY_INVALID;
	HServerQuery ActiveRulesQuery = HSERVERQUERY_INVALID;
	HServerQuery ActivePlayersQuery = HSERVERQUERY_INVALID;
	TArray<FString> RuleKeys;
	TArray<FString> RuleValues;
};
#else
class FESteamServerBrowserQueries
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamMatchmakingServersSubsystem::Deinitialize()
{
	// Super triggers HandleSteamClientShutdown while Steam is still up, which cancels
	// in-flight queries before the response objects are destroyed.
	Super::Deinitialize();
	Queries.Reset();
}

void UESteamMatchmakingServersSubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Queries)
	{
		Queries = MakeShared<FESteamServerBrowserQueries>(this);
	}
#endif
}

void UESteamMatchmakingServersSubsystem::HandleSteamClientShutdown()
{
	Queries.Reset();
}

bool UESteamMatchmakingServersSubsystem::RequestInternetServerList(const TMap<FString, FString>& Filters)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && Queries)
	{
		return Queries->StartServerList(FESteamServerBrowserQueries::EServerListType::Internet, Filters);
	}
#endif
	LogSteamUnavailable(TEXT("RequestInternetServerList"));
	return false;
}

bool UESteamMatchmakingServersSubsystem::RequestLANServerList()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && Queries)
	{
		return Queries->StartServerList(FESteamServerBrowserQueries::EServerListType::LAN, TMap<FString, FString>());
	}
#endif
	LogSteamUnavailable(TEXT("RequestLANServerList"));
	return false;
}

bool UESteamMatchmakingServersSubsystem::RequestFriendsServerList(const TMap<FString, FString>& Filters)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && Queries)
	{
		return Queries->StartServerList(FESteamServerBrowserQueries::EServerListType::Friends, Filters);
	}
#endif
	LogSteamUnavailable(TEXT("RequestFriendsServerList"));
	return false;
}

bool UESteamMatchmakingServersSubsystem::RequestFavoritesServerList(const TMap<FString, FString>& Filters)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && Queries)
	{
		return Queries->StartServerList(FESteamServerBrowserQueries::EServerListType::Favorites, Filters);
	}
#endif
	LogSteamUnavailable(TEXT("RequestFavoritesServerList"));
	return false;
}

bool UESteamMatchmakingServersSubsystem::RequestHistoryServerList(const TMap<FString, FString>& Filters)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && Queries)
	{
		return Queries->StartServerList(FESteamServerBrowserQueries::EServerListType::History, Filters);
	}
#endif
	LogSteamUnavailable(TEXT("RequestHistoryServerList"));
	return false;
}

bool UESteamMatchmakingServersSubsystem::RequestSpectatorServerList(int32 AppId, const TMap<FString, FString>& Filters)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && Queries)
	{
		return Queries->StartServerList(
			FESteamServerBrowserQueries::EServerListType::Spectator, Filters, static_cast<AppId_t>(AppId));
	}
#endif
	LogSteamUnavailable(TEXT("RequestSpectatorServerList"));
	return false;
}

void UESteamMatchmakingServersSubsystem::CancelServerListRequest()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (Queries)
	{
		Queries->CancelServerList();
	}
#endif
}

bool UESteamMatchmakingServersSubsystem::PingServer(const FString& Ip, int32 Port)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !Queries)
	{
		LogSteamUnavailable(TEXT("PingServer"));
		return false;
	}

	uint32 HostOrderIp = 0;
	if (!ParseIPv4(Ip, HostOrderIp) || Port <= 0 || Port > 65535)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("PingServer: invalid address %s:%d"), *Ip, Port);
		return false;
	}
	return Queries->StartPing(HostOrderIp, static_cast<uint16>(Port));
#else
	LogSteamUnavailable(TEXT("PingServer"));
	return false;
#endif
}

bool UESteamMatchmakingServersSubsystem::ServerRules(const FString& Ip, int32 Port)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !Queries)
	{
		LogSteamUnavailable(TEXT("ServerRules"));
		return false;
	}

	uint32 HostOrderIp = 0;
	if (!ParseIPv4(Ip, HostOrderIp) || Port <= 0 || Port > 65535)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ServerRules: invalid address %s:%d"), *Ip, Port);
		return false;
	}
	return Queries->StartRules(HostOrderIp, static_cast<uint16>(Port));
#else
	LogSteamUnavailable(TEXT("ServerRules"));
	return false;
#endif
}

bool UESteamMatchmakingServersSubsystem::RequestPlayerDetails(const FString& Ip, int32 Port)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !Queries)
	{
		LogSteamUnavailable(TEXT("RequestPlayerDetails"));
		return false;
	}

	uint32 HostOrderIp = 0;
	if (!ParseIPv4(Ip, HostOrderIp) || Port <= 0 || Port > 65535)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("RequestPlayerDetails: invalid address %s:%d"), *Ip, Port);
		return false;
	}
	return Queries->StartPlayerDetails(HostOrderIp, static_cast<uint16>(Port));
#else
	LogSteamUnavailable(TEXT("RequestPlayerDetails"));
	return false;
#endif
}
