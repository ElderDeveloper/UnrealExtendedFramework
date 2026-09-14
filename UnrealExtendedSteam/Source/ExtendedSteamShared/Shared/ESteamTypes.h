// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ESteamTypes.generated.h"

/**
 * Online subsystem service name for the Extended Steam OSS implementation.
 *
 * "STEAM" on purpose, the same name the engine's own OnlineSubsystemSteam registers: OnlineSubsystemEOS
 * maps a platform token to an EOS credential type by SUBSYSTEM NAME (UserManagerEOS.cpp
 * ToEOS_EExternalCredentialType), and only the literal STEAM_SUBSYSTEM yields
 * EOS_ECT_STEAM_SESSION_TICKET — anything else is sent as an OpenID token and the Connect login
 * fails. The two Steam OSS plugins cannot be enabled together anyway (two Steamworks initialisers
 * in one process), so the name is free.
 */
#ifndef ESTEAM_SUBSYSTEM
	#define ESTEAM_SUBSYSTEM FName(TEXT("STEAM"))
#endif

/** Authentication mode for the Steam game server API. */
UENUM(BlueprintType)
enum class EESteamServerMode : uint8
{
	/** No authentication, no VAC. LAN-style servers. */
	NoAuthentication UMETA(DisplayName = "No Authentication"),
	/** Authenticate players, no VAC. */
	Authentication,
	/** Authenticate players and enable VAC. */
	AuthenticationAndSecure UMETA(DisplayName = "Authentication and VAC")
};

/**
 * A 64-bit Steam identifier (user, lobby, clan, game server...).
 * Blueprint-opaque: use UESteamBlueprintLibrary for string conversion in Blueprints.
 */
USTRUCT(BlueprintType)
struct EXTENDEDSTEAMSHARED_API FESteamId
{
	GENERATED_BODY()

	FESteamId() = default;

	explicit FESteamId(uint64 InValue)
		: Value(InValue)
	{
	}

	UPROPERTY()
	uint64 Value = 0;

	bool IsValid() const { return Value != 0; }

	FString ToString() const { return LexToString(Value); }

	static FESteamId FromString(const FString& InString)
	{
		FESteamId Result;
		LexFromString(Result.Value, *InString);
		return Result;
	}

	bool operator==(const FESteamId& Other) const { return Value == Other.Value; }
	bool operator!=(const FESteamId& Other) const { return Value != Other.Value; }

	friend uint32 GetTypeHash(const FESteamId& Id) { return GetTypeHash(Id.Value); }
};
