// Copyright Epic Games, Inc. All Rights Reserved.
//
// For peer-to-peer we need to support multiple incoming and outgoing connections.
//
#pragma once

#include <list>
#include <string>

#include "AccountHelpers.h"
#include "RemoteConnection.h"

#include <eos_anticheatclient_types.h>

class FAntiCheatP2PNetworkTransport
{
public:
	struct FRegistrationInfoMessage
	{
		EOS_EAntiCheatCommonClientPlatform ClientPlatform = EOS_EAntiCheatCommonClientPlatform::EOS_ACCCP_Unknown;
		std::string ProductUserId;
		std::string EOSConnectIdTokenJWT;
	};

	/**
	* Constructor
	*/
	FAntiCheatP2PNetworkTransport() noexcept(false);

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FAntiCheatP2PNetworkTransport(FAntiCheatP2PNetworkTransport const&) = delete;
	FAntiCheatP2PNetworkTransport& operator=(FAntiCheatP2PNetworkTransport const&) = delete;

	/**
	 * Destructor
	 */
	virtual ~FAntiCheatP2PNetworkTransport();

	void SetLocalUserInfo(const FProductUserId& InLocalUserId, const std::string& InEOSConnectIdTokenJWT);

	bool Listen(uint16_t Port);
	FRemoteConnection* Connect(const char* Host, uint16_t Port);
	void Disconnect(const FRemoteConnection* Connection);
	void DisconnectAll();

	void OnIncomingConnection(FRemoteConnection&& NewConnection);
	void OnConnected(FRemoteConnection& Connection);
	void OnDisconnected(FRemoteConnection& Connection);
	void OnDataReceived(FRemoteConnection& Connection, const std::vector<uint8_t>& Buf);

	using FOnNewMessageCallback = std::function<void(void*, const std::vector<uint8_t>&)>;
	void SetOnNewMessageCallback(FOnNewMessageCallback Callback);

	using FOnNewClientCallback = std::function<void(void*, const FRegistrationInfoMessage&, const std::string&)>;
	void SetOnNewClientCallback(FOnNewClientCallback Callback);

	using FOnClientDisconnectedCallback = std::function<void(void*)>;
	void SetOnClientDisconnectedCallback(FOnClientDisconnectedCallback Callback);

	void Send(const EOS_AntiCheatCommon_OnMessageToClientCallbackInfo* Message);
	void Send(FRemoteConnection& Connection, const FRegistrationInfoMessage& Message);

	void Update();

private:
	enum class FMessageType : char
	{
		Opaque = 1,
		RegistrationInfo = 2
	};

	FProductUserId LocalUserId;
	std::string LocalUserEOSConnectIdTokenJWT;
	std::list<FRemoteConnection> Connections;
	std::unique_ptr<FRemoteConnectionListener> ConnectionListener;

	FOnNewMessageCallback OnNewMessageCallback;
	FOnNewClientCallback OnNewClientCallback;
	FOnClientDisconnectedCallback OnClientDisconnectedCallback;
};

