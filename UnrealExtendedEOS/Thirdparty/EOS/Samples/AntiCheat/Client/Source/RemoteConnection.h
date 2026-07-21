// Copyright Epic Games, Inc. All Rights Reserved.
//
// For peer-to-peer we need to support multiple incoming and outgoing connections.
//
#pragma once

#include <memory>
#include <functional>
#include <vector>
#include "DebugLog.h"
#include "StringUtils.h"

#define WITHOUT_SDL // Stop SDL_net from trying to include the main SDL library.
#include "SDL_net.h"

class FRemoteConnection;
using FOnIncomingConnection = std::function<void(FRemoteConnection&& Connection)>;
using FOnConnected = std::function<void(FRemoteConnection& Connection)>;
using FOnDisconnected = std::function<void(FRemoteConnection& Connection)>;
using FOnDataReceived = std::function<void(FRemoteConnection& Connection, const std::vector<uint8_t>& Buf)>;

class FRemoteConnection
{
public:
	FRemoteConnection(FOnConnected InOnConnected, FOnDisconnected InOnDisconnected, FOnDataReceived InOnDataReceived)
	{
		OnConnected = InOnConnected;
		OnDisconnected = InOnDisconnected;
		OnDataReceived = InOnDataReceived;
		SDLNet_Init();
		SocketSet = SDLNet_AllocSocketSet(1);
	}

	FRemoteConnection(FRemoteConnection&& Other)
	{
		if (this != &Other)
		{
			Socket = Other.Socket;
			SocketSet = Other.SocketSet;
			OnConnected = Other.OnConnected;
			OnDisconnected = Other.OnDisconnected;
			OnDataReceived = Other.OnDataReceived;
			bIsConnected = Other.bIsConnected;
			Other.Socket = nullptr;
			Other.SocketSet = nullptr;
			Other.bIsConnected = false;
		}
	}

	virtual ~FRemoteConnection()
	{
		// If we were connected at the time the destructor is triggered, trigger the disconnect callback.
		if (bIsConnected)
		{
			bIsConnected = false;
			OnDisconnected(*this);
		}

		if (Socket)
		{
			if (SocketSet)
			{
				SDLNet_TCP_DelSocket(SocketSet, Socket);
			}
			SDLNet_TCP_Close(Socket);
		}

		if (SocketSet)
		{
			SDLNet_FreeSocketSet(SocketSet);
		}
	}

	virtual bool Send(std::vector<uint8_t>& Buf)
	{
		const int SendResult = SDLNet_TCP_Send(Socket, Buf.data(), static_cast<int>(Buf.size()));
		if (SendResult != Buf.size())
		{
			FDebugLog::LogError(L"SDLNet_TCP_Send returned %i, expected %i", SendResult, Buf.size());
			return false;
		}
		return true;
	}

	virtual bool Update()
	{
		if (SDLNet_CheckSockets(SocketSet, 0) > 0 && SDLNet_SocketReady(Socket))
		{
			std::vector<uint8_t> Buffer(RECV_BUFFER_SIZE);
			const int ReceivedBufferLength = SDLNet_TCP_Recv(Socket, &Buffer[0], static_cast<int>(Buffer.size()));
			if (ReceivedBufferLength <= 0)
			{
				// Socket is closed or errored, trigger the disconnect callback.
				bIsConnected = false;
				OnDisconnected(*this);
				return false;
			}

			Buffer.resize(ReceivedBufferLength);
			OnDataReceived(*this, Buffer);
		}
		return true;
	}

	virtual std::string GetRemoteIP() const
	{
		if (Socket)
		{
			IPaddress* Address = SDLNet_TCP_GetPeerAddress(Socket);
			if (Address)
			{
				const uint8_t* HostBytes = reinterpret_cast<const uint8_t*>(&Address->host);

				char Buf[32] = {0};
				std::snprintf(Buf, sizeof(Buf), "%d.%d.%d.%d", HostBytes[0], HostBytes[1], HostBytes[2], HostBytes[3]);
				return Buf;
			}
		}
		return std::string();
	}

	void* GetConnectionHandle() const { return Socket; }

protected:
	virtual bool SetSocket(TCPsocket&& InSocket)
	{
		Socket = std::move(InSocket);
		if (Socket && SDLNet_TCP_AddSocket(SocketSet, Socket) > 0)
		{
			bIsConnected = true;
			OnConnected(*this);
			return true;
		}
		return false;
	}

	FOnConnected OnConnected;
	FOnDisconnected OnDisconnected;
	FOnDataReceived OnDataReceived;

	static const size_t RECV_BUFFER_SIZE = 4096;
	TCPsocket Socket = nullptr;
	SDLNet_SocketSet SocketSet = nullptr;
	bool bIsConnected = false;
};

class FRemoteConnectionOutgoing : public FRemoteConnection
{
public:
	FRemoteConnectionOutgoing(FOnConnected InOnConnected, FOnDisconnected InOnDisconnected, FOnDataReceived InOnDataReceived) : FRemoteConnection(InOnConnected, InOnDisconnected, InOnDataReceived) {}
	virtual ~FRemoteConnectionOutgoing() {}
	bool Connect(const char* Host, uint16_t Port)
	{
		IPaddress IP;
		SDLNet_ResolveHost(&IP, Host, Port);
		TCPsocket Socket = SDLNet_TCP_Open(&IP);
		if (!Socket)
		{
			FDebugLog::LogError(L"SDLNet_TCP_Open failed (%s)", FStringUtils::Widen(SDLNet_GetError()).c_str());
			return false;
		}
		return SetSocket(std::move(Socket));
	}
};

class FRemoteConnectionIncoming : public FRemoteConnection
{
public:
	FRemoteConnectionIncoming(FOnConnected InOnConnected, FOnDisconnected InOnDisconnected, FOnDataReceived InOnDataReceived, TCPsocket InSocket) : FRemoteConnection(InOnConnected, InOnDisconnected, InOnDataReceived) 
	{
		SetSocket(std::move(InSocket));
	}
	virtual ~FRemoteConnectionIncoming() {}
};

class FRemoteConnectionListener : public FRemoteConnection
{
public:
	FRemoteConnectionListener(FOnDisconnected InOnDisconnected, FOnDataReceived InOnDataReceived, FOnIncomingConnection InOnIncomingConnection) : FRemoteConnection(EmptyOnConnected, InOnDisconnected, InOnDataReceived)
	{
		OnIncomingConnection = InOnIncomingConnection;
	}
	FRemoteConnectionListener(FRemoteConnectionListener&& Other) : FRemoteConnection(std::move(Other))
	{
		OnIncomingConnection = Other.OnIncomingConnection;
	}
	virtual ~FRemoteConnectionListener()
	{
		bIsConnected = false; // Don't call OnDisconnect
	}

	bool Listen(uint16_t Port)
	{
		IPaddress IP;
		SDLNet_ResolveHost(&IP, nullptr, Port);
		TCPsocket Socket = SDLNet_TCP_Open(&IP);
		if (!Socket)
		{
			FDebugLog::LogError(L"SDLNet_TCP_Open failed (%s)", FStringUtils::Widen(SDLNet_GetError()).c_str());
			return false;
		}
		if (SetSocket(std::move(Socket)))
		{
			ListenPort = Port;
			return true;
		}
		return false;
	}

	virtual bool Update() override
	{
		if (SDLNet_CheckSockets(SocketSet, 0) > 0 && SDLNet_SocketReady(Socket))
		{
			TCPsocket NewSocket = SDLNet_TCP_Accept(Socket);
			if (!NewSocket)
			{
				// Socket is closed or errored.
				OnDisconnected(*this);
				ListenPort = 0;
				return false;
			}

			FRemoteConnectionIncoming NewConnection(EmptyOnConnected, OnDisconnected, OnDataReceived, NewSocket);
			OnIncomingConnection(std::move(NewConnection));
		}
		return true;
	}

	uint16_t GetListenPort() { return ListenPort; }

	// This is redundant for a listener since its only purpose is to establish other connections. Use OnIncomingConnection instead.
	static void EmptyOnConnected(FRemoteConnection& Connection) {};

private:
	FOnIncomingConnection OnIncomingConnection;
	uint16_t ListenPort = 0;
};