// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPAddress.h"
#include "Shared/ExtendedSteamSocketsTypes.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steamnetworkingtypes.h"
THIRD_PARTY_INCLUDES_END
#endif

/**
 * FInternetAddr implementation for the ExtendedSteam SteamNetworkingSockets subsystem.
 *
 * An ExtendedSteam address identifies a peer by a Steam identity (a CSteamID for the P2P case)
 * plus a "virtual port". When the SDK is present the identity is stored as a SteamNetworkingIdentity;
 * without the SDK we degrade to a bare 64-bit SteamID so the type still compiles and round-trips.
 *
 * Only the P2P (SteamID) case is fully modelled. Direct-IP (SteamNetworkingIPAddr) addresses are
 * accepted structurally but the string/raw round-trips below are written for the SteamID case;
 * a production driver that needs LAN direct-connect would extend GetRawIp/ToString accordingly.
 */
class FInternetAddrExtendedSteam : public FInternetAddr
{
public:
	explicit FInternetAddrExtendedSteam(const FName InProtocolType = NAME_None)
		: VirtualPort(0)
		, ProtocolType(InProtocolType.IsNone() ? FExtendedSteamNetworkProtocolTypes::SteamP2P() : InProtocolType)
	{
#if WITH_EXTENDEDSTEAM_SDK
		Identity.Clear();
#else
		SteamId = 0;
#endif
	}

	explicit FInternetAddrExtendedSteam(uint64 InSteamId)
		: VirtualPort(0)
		, ProtocolType(FExtendedSteamNetworkProtocolTypes::SteamP2P())
	{
		SetSteamID64(InSteamId);
	}

	//~ Begin FInternetAddr Interface
	virtual TArray<uint8> GetRawIp() const override;
	virtual void SetRawIp(const TArray<uint8>& RawAddr) override;

	/** IPv4 dword form is meaningless for SteamID identities. Kept as a no-op by design. */
	virtual void SetIp(uint32 InAddr) override {}
	virtual void SetIp(const TCHAR* InAddr, bool& bIsValid) override;
	virtual void GetIp(uint32& OutAddr) const override { OutAddr = 0; }

	virtual void SetPort(int32 InPort) override { VirtualPort = InPort; }
	virtual int32 GetPort() const override { return VirtualPort; }

	/** The Steam "virtual port" doubles as the platform port for this addressing scheme. */
	virtual void SetPlatformPort(int32 InPort) override { VirtualPort = InPort; }
	virtual int32 GetPlatformPort() const override { return VirtualPort; }

	virtual void SetAnyAddress() override { SetSteamID64(0); }
	virtual void SetBroadcastAddress() override {}
	virtual void SetLoopbackAddress() override;

	virtual FString ToString(bool bAppendPort) const override;

	virtual bool operator==(const FInternetAddr& Other) const override;
	virtual uint32 GetTypeHash() const override;
	virtual bool IsValid() const override { return GetSteamID64() != 0; }

	virtual TSharedRef<FInternetAddr> Clone() const override;
	virtual FName GetProtocolType() const override { return ProtocolType; }
	//~ End FInternetAddr Interface

	/** Raw 64-bit SteamID accessors, valid with or without the SDK compiled in. */
	uint64 GetSteamID64() const;
	void SetSteamID64(uint64 InSteamId);

#if WITH_EXTENDEDSTEAM_SDK
	/** Direct read access to the underlying identity for the socket/net-driver layer. */
	const SteamNetworkingIdentity& GetIdentity() const { return Identity; }
	SteamNetworkingIdentity& GetIdentity() { return Identity; }
#endif

private:
#if WITH_EXTENDEDSTEAM_SDK
	SteamNetworkingIdentity Identity;
#else
	uint64 SteamId;
#endif

	/** Steam virtual port (P2P) — a small integer used to distinguish multiple listen sockets. */
	int32 VirtualPort;

	/** ExtendedSteamP2P or ExtendedSteamIP. */
	FName ProtocolType;
};
