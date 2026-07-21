// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebBroadcastSubsystem.generated.h"

/**
 * IBroadcastService — Steam Broadcast integration (publisher key required, partner host —
 * trusted-server use only).
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebBroadcastSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * IBroadcastService/PostGameDataFrame/v1 (POST) — adds a game metadata frame to a user's
	 * live broadcast (publisher key). FrameDataJson is sent verbatim as "frame_data".
	 * An empty SteamId falls back to the configured DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Broadcast", meta = (AutoCreateRefTerm = "OnResponse"))
	void PostGameDataFrame(int32 AppId, FString SteamId, FString BroadcastId, FString FrameDataJson, const FOnSteamWebResponse& OnResponse);
};
