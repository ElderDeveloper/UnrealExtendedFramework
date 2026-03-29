// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <eos_anticheatclient_types.h>

class FGameEvent;

template<typename> struct TEpicAccountId;
using FProductUserId = TEpicAccountId<EOS_ProductUserId>;

class FAntiCheatClient
{
public:
	/**
	* Constructor
	*/
	FAntiCheatClient() noexcept(false);

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FAntiCheatClient(FAntiCheatClient const&) = delete;
	FAntiCheatClient& operator=(FAntiCheatClient const&) = delete;

	/**
	 * Destructor
	 */
	virtual ~FAntiCheatClient();

	void Init();
	void OnShutdown();

	bool Start(const std::string& Host, int Port, const FProductUserId& LocalUserId, const std::string& EOSConnectIdTokenJWT);
	bool StartP2P(int ListenPort, const FProductUserId& LocalUserId, const std::string& EOSConnectIdTokenJWT);
	bool ConnectP2PPeer(const std::string& Host, int Port);
	void Stop();
	void StopP2P();

	void OnGameEvent(const FGameEvent& Event);

	bool IsInitialized() { return bIsInitialized; }

private:
	bool AddNotifyMessageToServerCallback();
	void RemoveNotifyMessageToServerCallback();
	bool BeginSession(const FProductUserId& LocalUserId);
	void EndSession();

	bool ConnectToAntiCheatServer(const std::string& Host, int Port);
	void SendRegistrationInfoToAntiCheatServer(const FProductUserId& LocalUserId, const std::string& EOSConnectIdTokenJWT);
	void DisconnectFromAntiCheatServer();

	void OnMessageFromServerReceived(const void* Data, uint32_t DataLengthBytes) const;
	static void EOS_CALL OnMessageToServerCallback(const EOS_AntiCheatClient_OnMessageToServerCallbackInfo* Message);

	static void EOS_CALL OnMessageToPeerCallback(const EOS_AntiCheatCommon_OnMessageToClientCallbackInfo* Message);
	static void EOS_CALL OnPeerActionRequiredCallback(const EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo* Message);
	static void EOS_CALL OnPeerAuthStatusChangedCallback(const EOS_AntiCheatCommon_OnClientAuthStatusChangedCallbackInfo* Message);

private:
	struct PendingRegistrationData
	{
		void* Context = nullptr;
		std::string IpAddress;
		EOS_EAntiCheatCommonClientPlatform ClientPlatform = EOS_EAntiCheatCommonClientPlatform::EOS_ACCCP_Unknown;
	};

	std::map<EOS_ProductUserId, PendingRegistrationData> PendingRegistrations;
	EOS_HAntiCheatClient AntiCheatClientHandle = nullptr;
	EOS_NotificationId MessageToServerNotificationId = EOS_INVALID_NOTIFICATIONID;
	EOS_NotificationId MessageToPeerNotificationId = EOS_INVALID_NOTIFICATIONID;
	EOS_NotificationId PeerActionRequiredNotificationId = EOS_INVALID_NOTIFICATIONID;
	EOS_NotificationId PeerAuthStatusChangedNotificationId = EOS_INVALID_NOTIFICATIONID;
	bool bIsInitialized = false;
	bool bConnected = false;
};

