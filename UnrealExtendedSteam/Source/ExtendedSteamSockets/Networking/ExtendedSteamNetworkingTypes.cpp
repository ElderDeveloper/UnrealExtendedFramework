// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Networking/ExtendedSteamNetworkingTypes.h"

uint64 FInternetAddrExtendedSteam::GetSteamID64() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return Identity.GetSteamID64();
#else
	return SteamId;
#endif
}

void FInternetAddrExtendedSteam::SetSteamID64(uint64 InSteamId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (InSteamId == 0)
	{
		Identity.Clear();
	}
	else
	{
		Identity.SetSteamID64(InSteamId);
	}
#else
	SteamId = InSteamId;
#endif
}

TArray<uint8> FInternetAddrExtendedSteam::GetRawIp() const
{
	// Encode the 64-bit SteamID big-endian so byte-array comparisons are stable across hosts.
	const uint64 Id = GetSteamID64();
	TArray<uint8> Result;
	Result.SetNumUninitialized(8);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		Result[Index] = static_cast<uint8>((Id >> (8 * (7 - Index))) & 0xFF);
	}
	return Result;
}

void FInternetAddrExtendedSteam::SetRawIp(const TArray<uint8>& RawAddr)
{
	if (RawAddr.Num() < 8)
	{
		return;
	}

	uint64 Id = 0;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		Id = (Id << 8) | static_cast<uint64>(RawAddr[Index]);
	}
	SetSteamID64(Id);
}

void FInternetAddrExtendedSteam::SetIp(const TCHAR* InAddr, bool& bIsValid)
{
	// Accepts "<steamid>" or "<steamid>:<virtualport>". Also tolerates a leading "steam:" scheme.
	bIsValid = false;
	if (InAddr == nullptr)
	{
		return;
	}

	FString AddressString(InAddr);
	AddressString.RemoveFromStart(TEXT("steam:"));
	AddressString.RemoveFromStart(TEXT("steamid:"));

	FString SteamIdPart = AddressString;
	FString PortPart;
	if (AddressString.Split(TEXT(":"), &SteamIdPart, &PortPart))
	{
		VirtualPort = FCString::Atoi(*PortPart);
	}

	uint64 ParsedId = 0;
	LexFromString(ParsedId, *SteamIdPart);
	if (ParsedId != 0)
	{
		SetSteamID64(ParsedId);
		bIsValid = true;
	}
}

void FInternetAddrExtendedSteam::SetLoopbackAddress()
{
#if WITH_EXTENDEDSTEAM_SDK
	Identity.SetLocalHost();
#else
	SteamId = 0;
#endif
}

FString FInternetAddrExtendedSteam::ToString(bool bAppendPort) const
{
	const FString Base = FString::Printf(TEXT("steamid:%llu"), GetSteamID64());
	return bAppendPort ? FString::Printf(TEXT("%s:%d"), *Base, VirtualPort) : Base;
}

bool FInternetAddrExtendedSteam::operator==(const FInternetAddr& Other) const
{
	// Same subsystem addresses compare by identity + virtual port; foreign addresses never match.
	if (Other.GetProtocolType() != FExtendedSteamNetworkProtocolTypes::SteamP2P()
		&& Other.GetProtocolType() != FExtendedSteamNetworkProtocolTypes::SteamIP())
	{
		return false;
	}

	const FInternetAddrExtendedSteam& SteamOther = static_cast<const FInternetAddrExtendedSteam&>(Other);
	return GetSteamID64() == SteamOther.GetSteamID64() && VirtualPort == SteamOther.VirtualPort;
}

uint32 FInternetAddrExtendedSteam::GetTypeHash() const
{
	const uint64 Id = GetSteamID64();
	return HashCombine(::GetTypeHash(Id), ::GetTypeHash(VirtualPort));
}

TSharedRef<FInternetAddr> FInternetAddrExtendedSteam::Clone() const
{
	TSharedRef<FInternetAddrExtendedSteam> Copy = MakeShared<FInternetAddrExtendedSteam>(ProtocolType);
#if WITH_EXTENDEDSTEAM_SDK
	Copy->Identity = Identity;
#else
	Copy->SteamId = SteamId;
#endif
	Copy->VirtualPort = VirtualPort;
	Copy->ProtocolType = ProtocolType;
	return Copy;
}
