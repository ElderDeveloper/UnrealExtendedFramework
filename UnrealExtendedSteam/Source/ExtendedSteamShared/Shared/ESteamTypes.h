// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ESteamTypes.generated.h"

/** Online subsystem service name for the Extended Steam OSS implementation. */
#ifndef ESTEAM_SUBSYSTEM
	#define ESTEAM_SUBSYSTEM FName(TEXT("EXTENDEDSTEAM"))
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
