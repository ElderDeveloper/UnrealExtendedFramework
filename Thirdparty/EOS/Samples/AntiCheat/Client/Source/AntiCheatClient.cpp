// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"

#include "Game.h"
#include "GameEvent.h"
#include "Platform.h"
#include "AntiCheatNetworkTransport.h"
#include "AntiCheatP2PNetworkTransport.h"
#include "AntiCheatClient.h"
#include "Authentication.h"

#include "DebugLog.h"
#include "AccountHelpers.h"

#include <eos_anticheatclient.h>

FAntiCheatClient::FAntiCheatClient()
{
	FGame::Get().GetAntiCheatNetworkTransport()->SetOnNewMessageCallback([this](const void* Data, uint32_t Length)
	{
		OnMessageFromServerReceived(Data, Length);
	});
	FGame::Get().GetAntiCheatNetworkTransport()->SetOnClientActionRequiredCallback([this](EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo Message)
	{
		FDebugLog::Log(L"OnClientActionRequired: ClientAction=%d, ClientActionReasonCode=%d, ClientActionReasonDetails=%ls", Message.ClientAction, Message.ActionReasonCode, FStringUtils::Widen(Message.ActionReasonDetailsString).c_str());
		FGameEvent Event(EGameEventType::AntiCheatKicked);
		FGame::Get().OnGameEvent(Event);
	});

	FGame::Get().GetAntiCheatP2PNetworkTransport()->SetOnNewClientCallback([this](void* Context, const FAntiCheatP2PNetworkTransport::FRegistrationInfoMessage& Registration, const std::string& IpAddress)
	{
		const EOS_ProductUserId ProductUserId = EOS_ProductUserId_FromString(Registration.ProductUserId.c_str());

		// Store the data needed for the call to RegisterPeer.
		PendingRegistrationData Data;
		Data.ClientPlatform = Registration.ClientPlatform;
		Data.Context = Context;
		Data.IpAddress = IpAddress;
		PendingRegistrations[ProductUserId] = Data;

		// Verify the ConnectId token to ensure the user isn't impersonating someone else.
		FGame::Get().GetAuthentication()->VerifyConnectIdToken(ProductUserId, Registration.EOSConnectIdTokenJWT.c_str());
	});

	FGame::Get().GetAntiCheatP2PNetworkTransport()->SetOnNewMessageCallback([this](void* Context, const std::vector<uint8_t>& Data)
	{
		EOS_AntiCheatClient_ReceiveMessageFromPeerOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCLIENT_RECEIVEMESSAGEFROMPEER_API_LATEST;
		Options.PeerHandle = Context;
		Options.Data = Data.data();
		Options.DataLengthBytes = static_cast<uint32_t>(Data.size());
		const EOS_EResult Result = EOS_AntiCheatClient_ReceiveMessageFromPeer(AntiCheatClientHandle, &Options);
		if (Result != EOS_EResult::EOS_Success)
		{
			FDebugLog::LogError(L"EOS_AntiCheatClient_ReceiveMessageFromPeer failed with %s", FStringUtils::Widen(EOS_EResult_ToString(Result)).c_str());
		}
	});

	FGame::Get().GetAntiCheatP2PNetworkTransport()->SetOnClientDisconnectedCallback([this](void* Context)
	{
		EOS_AntiCheatClient_UnregisterPeerOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCLIENT_UNREGISTERPEER_API_LATEST;
		Options.PeerHandle = Context;
		const EOS_EResult Result = EOS_AntiCheatClient_UnregisterPeer(AntiCheatClientHandle, &Options);
		if (Result != EOS_EResult::EOS_Success)
		{
			FDebugLog::LogError(L"EOS_AntiCheatClient_UnregisterPeer failed with %s", FStringUtils::Widen(EOS_EResult_ToString(Result)).c_str());
		}
	});
}

FAntiCheatClient::~FAntiCheatClient()
{

}

void FAntiCheatClient::Init()
{
	AntiCheatClientHandle = EOS_Platform_GetAntiCheatClientInterface(FPlatform::GetPlatformHandle());
	if (!AntiCheatClientHandle)
	{
		FDebugLog::LogError(L"Can't get a handle to the Anti-Cheat Client Interface. Ensure the game was started via the Anti-Cheat bootstrapper and check DebugOutput.log for more details.");
		return;
	}
	bIsInitialized = true;
}

bool FAntiCheatClient::Start(const std::string& Host, int Port, const FProductUserId& LocalUserId, const std::string& EOSConnectIdTokenJWT)
{
	if (!ConnectToAntiCheatServer(Host, Port))
	{
		FDebugLog::LogError(L"ConnectToAntiCheatServer Error");
		return false;
	}

	if (!AddNotifyMessageToServerCallback())
	{
		DisconnectFromAntiCheatServer();
		FDebugLog::LogError(L"AddNotifyMessageToServerCallback Error");
		return false;
	}

	if (!BeginSession(LocalUserId))
	{
		RemoveNotifyMessageToServerCallback();
		DisconnectFromAntiCheatServer();
		FDebugLog::LogError(L"BeginSession Error");
		return false;
	}

	SendRegistrationInfoToAntiCheatServer(LocalUserId, EOSConnectIdTokenJWT);
	bConnected = true;

	return true;
}

bool FAntiCheatClient::StartP2P(int ListenPort, const FProductUserId& LocalUserId, const std::string& EOSConnectIdTokenJWT)
{
	{
		EOS_AntiCheatClient_AddNotifyMessageToPeerOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYMESSAGETOPEER_API_LATEST;
		MessageToPeerNotificationId = EOS_AntiCheatClient_AddNotifyMessageToPeer(AntiCheatClientHandle, &Options, nullptr, OnMessageToPeerCallback);
		if (MessageToPeerNotificationId == EOS_INVALID_NOTIFICATIONID)
		{
			FDebugLog::LogError(L"EOS_AntiCheatClient_AddNotifyMessageToPeer failed");
			return false;
		}
	}
	
	{
		EOS_AntiCheatClient_AddNotifyPeerActionRequiredOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYPEERACTIONREQUIRED_API_LATEST;
		PeerActionRequiredNotificationId = EOS_AntiCheatClient_AddNotifyPeerActionRequired(AntiCheatClientHandle, &Options, nullptr, OnPeerActionRequiredCallback);
		if (PeerActionRequiredNotificationId == EOS_INVALID_NOTIFICATIONID)
		{
			FDebugLog::LogError(L"EOS_AntiCheatClient_AddNotifyPeerActionRequired failed");
			return false;
		}
	}

	{
		EOS_AntiCheatClient_AddNotifyPeerAuthStatusChangedOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYPEERAUTHSTATUSCHANGED_API_LATEST;
		PeerAuthStatusChangedNotificationId = EOS_AntiCheatClient_AddNotifyPeerAuthStatusChanged(AntiCheatClientHandle, &Options, nullptr, OnPeerAuthStatusChangedCallback);
		if (PeerAuthStatusChangedNotificationId == EOS_INVALID_NOTIFICATIONID)
		{
			FDebugLog::LogError(L"EOS_AntiCheatClient_AddNotifyPeerAuthStatusChanged failed");
			return false;
		}
	}

	{
		EOS_AntiCheatClient_BeginSessionOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCLIENT_BEGINSESSION_API_LATEST;
		Options.LocalUserId = LocalUserId;
		Options.Mode = EOS_EAntiCheatClientMode::EOS_ACCM_PeerToPeer;
		const EOS_EResult Result = EOS_AntiCheatClient_BeginSession(AntiCheatClientHandle, &Options);
		if (Result == EOS_EResult::EOS_Success)
		{
			bConnected = true;
			FGame::Get().GetAntiCheatP2PNetworkTransport()->SetLocalUserInfo(LocalUserId, EOSConnectIdTokenJWT);

			const bool bListening = FGame::Get().GetAntiCheatP2PNetworkTransport()->Listen(ListenPort);
			if (!bListening)
			{
				StopP2P();
				return false;
			}
			return true;
		}
		FDebugLog::LogError(L"EOS_AntiCheatClient_BeginSession failed with %s", FStringUtils::Widen(EOS_EResult_ToString(Result)).c_str());
	}

	return false;
}

bool FAntiCheatClient::ConnectP2PPeer(const std::string& Host, int Port)
{
	return FGame::Get().GetAntiCheatP2PNetworkTransport()->Connect(Host.c_str(), Port);
}

void FAntiCheatClient::Stop()
{
	if (bConnected)
	{
		RemoveNotifyMessageToServerCallback();
		EndSession();
		DisconnectFromAntiCheatServer();
		bConnected = false;
	}
}

void FAntiCheatClient::StopP2P()
{
	if (bConnected)
	{
		EOS_AntiCheatClient_RemoveNotifyMessageToPeer(AntiCheatClientHandle, MessageToPeerNotificationId);
		MessageToPeerNotificationId = EOS_INVALID_NOTIFICATIONID;

		EOS_AntiCheatClient_RemoveNotifyPeerActionRequired(AntiCheatClientHandle, PeerActionRequiredNotificationId);
		PeerActionRequiredNotificationId = EOS_INVALID_NOTIFICATIONID;

		EOS_AntiCheatClient_RemoveNotifyPeerAuthStatusChanged(AntiCheatClientHandle, PeerAuthStatusChangedNotificationId);
		PeerAuthStatusChangedNotificationId = EOS_INVALID_NOTIFICATIONID;

		FGame::Get().GetAntiCheatP2PNetworkTransport()->DisconnectAll();
		EndSession();

		bConnected = false;
	}
}

void FAntiCheatClient::OnGameEvent(const FGameEvent& Event)
{
	if (Event.GetType() == EGameEventType::UserConnectIdTokenVerified || Event.GetType() == EGameEventType::UserConnectIdTokenVerificationFailed)
	{
		const EOS_ProductUserId ProductUserId = Event.GetProductUserId();
		if (PendingRegistrations.count(ProductUserId) != 0)
		{
			if (Event.GetType() == EGameEventType::UserConnectIdTokenVerified)
			{
				const PendingRegistrationData& Data = PendingRegistrations[ProductUserId];
				EOS_AntiCheatClient_RegisterPeerOptions Options = {};
				Options.ApiVersion = EOS_ANTICHEATCLIENT_REGISTERPEER_API_LATEST;
				Options.PeerHandle = Data.Context;
				Options.ClientType = EOS_EAntiCheatCommonClientType::EOS_ACCCT_ProtectedClient;
				Options.ClientPlatform = Data.ClientPlatform;
				Options.AuthenticationTimeout = 120;
				Options.IpAddress = Data.IpAddress.empty() ? nullptr : Data.IpAddress.c_str();
				Options.PeerProductUserId = ProductUserId;
				const EOS_EResult Result = EOS_AntiCheatClient_RegisterPeer(AntiCheatClientHandle, &Options);
				if (Result != EOS_EResult::EOS_Success)
				{
					FDebugLog::LogError(L"RegisterPeer failed with %s", FStringUtils::Widen(EOS_EResult_ToString(Result)).c_str());
				}
			}
			else
			{
				FDebugLog::LogError(L"ConnectIdToken verification failed");
			}
			PendingRegistrations.erase(ProductUserId);
		}
	}
}

void FAntiCheatClient::EndSession()
{
	EOS_AntiCheatClient_EndSessionOptions EndSessionOptions{};
	EndSessionOptions.ApiVersion = EOS_ANTICHEATCLIENT_ENDSESSION_API_LATEST;
	EOS_AntiCheatClient_EndSession(AntiCheatClientHandle, &EndSessionOptions);
}

void FAntiCheatClient::OnShutdown()
{
	Stop();
}

void FAntiCheatClient::OnMessageFromServerReceived(const void* Data, uint32_t DataLengthBytes) const
{
	EOS_AntiCheatClient_ReceiveMessageFromServerOptions ReceiveMessageFromServerOptions = {};

	ReceiveMessageFromServerOptions.ApiVersion = EOS_ANTICHEATCLIENT_RECEIVEMESSAGEFROMSERVER_API_LATEST;
	ReceiveMessageFromServerOptions.Data = Data;
	ReceiveMessageFromServerOptions.DataLengthBytes = DataLengthBytes;

	EOS_AntiCheatClient_ReceiveMessageFromServer(AntiCheatClientHandle, &ReceiveMessageFromServerOptions);
}

bool FAntiCheatClient::AddNotifyMessageToServerCallback()
{
	EOS_AntiCheatClient_AddNotifyMessageToServerOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYMESSAGETOSERVER_API_LATEST;
	MessageToServerNotificationId = EOS_AntiCheatClient_AddNotifyMessageToServer(AntiCheatClientHandle, &Options, nullptr, OnMessageToServerCallback);

	return MessageToServerNotificationId != EOS_INVALID_NOTIFICATIONID;
}

void FAntiCheatClient::RemoveNotifyMessageToServerCallback()
{
	EOS_AntiCheatClient_RemoveNotifyMessageToServer(AntiCheatClientHandle, MessageToServerNotificationId);
}

bool FAntiCheatClient::BeginSession(const FProductUserId& LocalUserId)
{
	EOS_AntiCheatClient_BeginSessionOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATCLIENT_BEGINSESSION_API_LATEST;
	Options.LocalUserId = LocalUserId;
	Options.Mode = EOS_EAntiCheatClientMode::EOS_ACCM_ClientServer;

	const EOS_EResult Result = EOS_AntiCheatClient_BeginSession(AntiCheatClientHandle, &Options);

	return Result == EOS_EResult::EOS_Success;
}

bool FAntiCheatClient::ConnectToAntiCheatServer(const std::string& Host, int Port)
{
	const bool bIsConnected = FGame::Get().GetAntiCheatNetworkTransport()->Connect(Host.c_str(), Port);
	return bIsConnected;
}

void FAntiCheatClient::DisconnectFromAntiCheatServer()
{
	FGame::Get().GetAntiCheatNetworkTransport()->Disconnect();
}

void FAntiCheatClient::SendRegistrationInfoToAntiCheatServer(const FProductUserId& LocalUserId, const std::string& EOSConnectIdTokenJWT)
{
	FAntiCheatNetworkTransport::FRegistrationInfoMessage Message;

	Message.ProductUserId = FStringUtils::Narrow(LocalUserId.ToString());
	Message.EOSConnectIdTokenJWT = EOSConnectIdTokenJWT;
	Message.ClientPlatform = EOS_EAntiCheatCommonClientPlatform::EOS_ACCCP_Windows;

	FGame::Get().GetAntiCheatNetworkTransport()->Send(Message);
}

void FAntiCheatClient::OnMessageToServerCallback(const EOS_AntiCheatClient_OnMessageToServerCallbackInfo* Message)
{
	FGame::Get().GetAntiCheatNetworkTransport()->Send(Message);
}

void FAntiCheatClient::OnMessageToPeerCallback(const EOS_AntiCheatCommon_OnMessageToClientCallbackInfo* Message)
{
	FGame::Get().GetAntiCheatP2PNetworkTransport()->Send(Message);
}

void FAntiCheatClient::OnPeerActionRequiredCallback(const EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo* Message)
{
	if (Message->ClientAction == EOS_EAntiCheatCommonClientAction::EOS_ACCCA_RemovePlayer)
	{
		if (Message->ClientHandle == EOS_ANTICHEATCLIENT_PEER_SELF)
		{
			// Self - the SDK is telling us that we will be rejected by other peers and cannot play online.
			FDebugLog::LogError(L"PeerActionRequired requests removing ourselves - we cannot play online. See logs for details.");
			FGame::Get().OnGameEvent(FGameEvent(EGameEventType::AntiCheatKicked));
		}
		else
		{
			// Remote peer - disconnect them.
			FGame::Get().GetAntiCheatP2PNetworkTransport()->Disconnect(reinterpret_cast<FRemoteConnection*>(Message->ClientHandle));
		}
	}
}

void FAntiCheatClient::OnPeerAuthStatusChangedCallback(const EOS_AntiCheatCommon_OnClientAuthStatusChangedCallbackInfo* Message)
{
}
