// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebLobbyMatchmakingSubsystem.generated.h"

/**
 * ILobbyMatchmakingService — server-side lobby creation and membership management
 * (publisher key required, partner host — trusted-server use only).
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebLobbyMatchmakingSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * ILobbyMatchmakingService/CreateLobby/v1 (POST) — creates a lobby on behalf of the app
	 * (publisher key). LobbyType matches ELobbyType (0 private, 1 friends-only, 2 public,
	 * 3 invisible). SteamIdInvitedMembersCsv is a comma-separated SteamID64 list, expanded into
	 * indexed steamid_invited_members[N] params. Empty LobbyName/InputJson are omitted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|LobbyMatchmaking", meta = (AutoCreateRefTerm = "OnResponse"))
	void CreateLobby(int32 AppId, int32 MaxMembers, int32 LobbyType, FString LobbyName, FString InputJson, FString SteamIdInvitedMembersCsv, const FOnSteamWebResponse& OnResponse);

	/**
	 * ILobbyMatchmakingService/RemoveUserFromLobby/v1 (POST) — removes a user from a lobby the
	 * app owns (publisher key). An empty InputJson is omitted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|LobbyMatchmaking", meta = (AutoCreateRefTerm = "OnResponse"))
	void RemoveUserFromLobby(int32 AppId, FString SteamIdToRemove, FString SteamIdLobby, FString InputJson, const FOnSteamWebResponse& OnResponse);
};
