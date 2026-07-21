// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"

#include "DebugLog.h"
#include "AntiCheatP2PNetworkTransport.h"

FAntiCheatP2PNetworkTransport::FAntiCheatP2PNetworkTransport()
{
}

FAntiCheatP2PNetworkTransport::~FAntiCheatP2PNetworkTransport()
{
}

bool FAntiCheatP2PNetworkTransport::Listen(uint16_t Port)
{
	ConnectionListener = std::make_unique<FRemoteConnectionListener>(
		// OnDisconnected
		[this](FRemoteConnection& Connection) { this->OnDisconnected(Connection); },
		// OnDataReceived - called when new data is received on an incoming connection
		[this](FRemoteConnection& Connection, const std::vector<uint8_t>& Buf) { this->OnDataReceived(Connection, Buf); },
		// OnIncomingConnection - called when a new connection is established to this listener
		[this](FRemoteConnection&& NewConnection) { this->OnIncomingConnection(std::move(NewConnection)); });
	return ConnectionListener->Listen(Port);
}

void FAntiCheatP2PNetworkTransport::SetLocalUserInfo(const FProductUserId& InLocalUserId, const std::string& InEOSConnectIdTokenJWT)
{
	LocalUserId = InLocalUserId;
	LocalUserEOSConnectIdTokenJWT = InEOSConnectIdTokenJWT;
}

FRemoteConnection* FAntiCheatP2PNetworkTransport::Connect(const char* Host, uint16_t Port)
{
	if (Port == ConnectionListener->GetListenPort())
	{
		FDebugLog::LogError(L"Already listening on port %u. If you want to connect to this instance of the sample, start another instance to simulate a different game client.", Port);
		return nullptr;
	}

	FRemoteConnectionOutgoing Connection(
		// OnConnected
		[this](FRemoteConnection& Connection) { this->OnConnected(Connection); },
		// OnDisconnected
		[this](FRemoteConnection& Connection) { this->OnDisconnected(Connection); },
		// OnDataReceived
		[this](FRemoteConnection& Connection, const std::vector<uint8_t>& Buf) { this->OnDataReceived(Connection, Buf); });

	if (Connection.Connect(Host, Port))
	{
		auto it = Connections.insert(Connections.end(), std::move(Connection));
		return &(*it);
	}
	return nullptr;
}

void FAntiCheatP2PNetworkTransport::Disconnect(const FRemoteConnection* InConnectionHandle)
{
	// Remove the specified connection from our list which will call its destructor and disconnect it as well.
	if (InConnectionHandle != nullptr)
	{
		Connections.remove_if([InConnectionHandle](const FRemoteConnection& c) { return &c == InConnectionHandle; });
	}
}

void FAntiCheatP2PNetworkTransport::DisconnectAll()
{
	Connections.clear();
	ConnectionListener = nullptr;
}

void FAntiCheatP2PNetworkTransport::OnIncomingConnection(FRemoteConnection&& NewConnection)
{
	OnConnected(NewConnection);
	Connections.push_back(std::move(NewConnection));
}

void FAntiCheatP2PNetworkTransport::OnConnected(FRemoteConnection& Connection)
{
	FRegistrationInfoMessage Message = {};
	Message.ClientPlatform = EOS_EAntiCheatCommonClientPlatform::EOS_ACCCP_Windows;
	Message.EOSConnectIdTokenJWT = LocalUserEOSConnectIdTokenJWT;
	Message.ProductUserId = FStringUtils::Narrow(LocalUserId.ToString());
	Send(Connection, Message);
}

void FAntiCheatP2PNetworkTransport::OnDisconnected(FRemoteConnection& Connection)
{
	// This function is called on disconnect.
	OnClientDisconnectedCallback(&Connection);
}

void FAntiCheatP2PNetworkTransport::OnDataReceived(FRemoteConnection& Connection, const std::vector<uint8_t>& Buf)
{
	// There can be multiple messages streamed together here so we have to separate them.
	std::vector<uint8_t> ReadBuf(Buf);
	while (!ReadBuf.empty())
	{
		const FMessageType MessageType = *reinterpret_cast<const FMessageType*>(ReadBuf.data());
		const uint32_t MessageSize = *reinterpret_cast<const uint32_t*>(ReadBuf.data() + sizeof(MessageType));
		const size_t SizeOfHeader = sizeof(MessageType) + sizeof(MessageSize);
		const size_t MessageSizeWithHeader = SizeOfHeader + MessageSize;
		if (MessageSizeWithHeader > ReadBuf.size())
		{
			FDebugLog::LogError(L"FAntiCheatP2PNetworkTransport::OnDataReceived - Message appears to span multiple TCP buffers, not supported");
			return;
		}

		const std::vector<uint8_t> MessageData(ReadBuf.begin() + SizeOfHeader, ReadBuf.begin() + MessageSizeWithHeader);
		switch (MessageType)
		{
			case FMessageType::Opaque:
				OnNewMessageCallback(&Connection, MessageData);
				break;
			case FMessageType::RegistrationInfo:
			{
				FRegistrationInfoMessage Message = {};

				// The last character must be a null terminator since we're parsing strings as null terminated.
				if (MessageData.size() >= sizeof(Message.ClientPlatform) + 2 && MessageData.back() == '\0')
				{
					Message.ClientPlatform = *reinterpret_cast<const EOS_EAntiCheatCommonClientPlatform*>(MessageData.data());
					Message.ProductUserId = reinterpret_cast<const char*>(std::min(&MessageData.back(), MessageData.data() + sizeof(Message.ClientPlatform)));
					Message.EOSConnectIdTokenJWT = reinterpret_cast<const char*>(std::min(&MessageData.back(), MessageData.data() + sizeof(Message.ClientPlatform) + Message.ProductUserId.size() + 1));
					OnNewClientCallback(&Connection, Message, Connection.GetRemoteIP());
				}
				else
				{
					FDebugLog::LogError(L"FAntiCheatP2PNetworkTransport::OnDataReceived - Invalid message received");
				}
				break;
			}
			default:
				FDebugLog::LogError(L"FAntiCheatP2PNetworkTransport::OnDataReceived - Unknown MessageType received");
				break;
		}

		ReadBuf = std::vector<uint8_t>(ReadBuf.begin() + MessageSizeWithHeader, ReadBuf.end());
	}
}

void FAntiCheatP2PNetworkTransport::Send(const EOS_AntiCheatCommon_OnMessageToClientCallbackInfo* Message)
{
	std::vector<uint8_t> Buffer;

	const FMessageType MessageType = FMessageType::Opaque;
	const uint32_t MessageSize = Message->MessageDataSizeBytes;

	const uint8_t* const MessageTypePtr = reinterpret_cast<const uint8_t*>(&MessageType);
	Buffer.insert(Buffer.end(), MessageTypePtr, MessageTypePtr + sizeof(MessageType));

	const uint8_t* const MessageSizePtr = reinterpret_cast<const uint8_t*>(&MessageSize);
	Buffer.insert(Buffer.end(), MessageSizePtr, MessageSizePtr + sizeof(MessageSize));

	const uint8_t* const MessageDataPtr = reinterpret_cast<const uint8_t*>(Message->MessageData);
	Buffer.insert(Buffer.end(), MessageDataPtr, MessageDataPtr + Message->MessageDataSizeBytes);

	FRemoteConnection *const Connection = reinterpret_cast<FRemoteConnection*>(Message->ClientHandle);
	if (Connection)
	{
		Connection->Send(Buffer);
	}
}

void FAntiCheatP2PNetworkTransport::Send(FRemoteConnection& Connection, const FRegistrationInfoMessage& Message)
{
	std::vector<uint8_t> Buffer;

	const FMessageType MessageType = FMessageType::RegistrationInfo;
	const uint32_t MessageSize = static_cast<uint32_t>(sizeof(Message.ClientPlatform) + Message.ProductUserId.size() + 1 + Message.EOSConnectIdTokenJWT.size() + 1);

	const uint8_t* const MessageTypePtr = reinterpret_cast<const uint8_t*>(&MessageType);
	Buffer.insert(Buffer.end(), MessageTypePtr, MessageTypePtr + sizeof(MessageType));

	const uint8_t* const MessageSizePtr = reinterpret_cast<const uint8_t*>(&MessageSize);
	Buffer.insert(Buffer.end(), MessageSizePtr, MessageSizePtr + sizeof(MessageSize));

	const uint8_t* const ClientPlatformPtr = reinterpret_cast<const uint8_t*>(&Message.ClientPlatform);
	Buffer.insert(Buffer.end(), ClientPlatformPtr, ClientPlatformPtr + sizeof(Message.ClientPlatform));

	const uint8_t* const ProductUserIdPtr = reinterpret_cast<const uint8_t*>(Message.ProductUserId.c_str());
	Buffer.insert(Buffer.end(), ProductUserIdPtr, ProductUserIdPtr + Message.ProductUserId.size() + 1); // +1 for null terminator

	const uint8_t* const EOSConnectIdTokenJWTPtr = reinterpret_cast<const uint8_t*>(Message.EOSConnectIdTokenJWT.c_str());
	Buffer.insert(Buffer.end(), EOSConnectIdTokenJWTPtr, EOSConnectIdTokenJWTPtr + Message.EOSConnectIdTokenJWT.size() + 1); // +1 for null terminator

	Connection.Send(Buffer);
}

void FAntiCheatP2PNetworkTransport::Update()
{
	// Update listener.
	if (ConnectionListener) { ConnectionListener->Update(); }
	
	// Update all peer connections and remove any where the Update function returns false meaning the connection is closed.
	Connections.remove_if([](FRemoteConnection& c) { return !c.Update(); });
}

void FAntiCheatP2PNetworkTransport::SetOnNewMessageCallback(FOnNewMessageCallback Callback)
{
	OnNewMessageCallback = std::move(Callback);
}

void FAntiCheatP2PNetworkTransport::SetOnNewClientCallback(FOnNewClientCallback Callback)
{
	OnNewClientCallback = std::move(Callback);
}

void FAntiCheatP2PNetworkTransport::SetOnClientDisconnectedCallback(FOnClientDisconnectedCallback Callback)
{
	OnClientDisconnectedCallback = std::move(Callback);
}
