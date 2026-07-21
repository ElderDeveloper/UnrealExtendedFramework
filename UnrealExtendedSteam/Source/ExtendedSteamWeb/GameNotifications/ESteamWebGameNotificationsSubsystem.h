// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebGameNotificationsSubsystem.generated.h"

/**
 * IGameNotificationsService — asynchronous game session notifications shown in the Steam client
 * ("it's your turn" style) (partner host, publisher Web API key required).
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebGameNotificationsSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * IGameNotificationsService/CreateSession/v1 (POST) — creates an async game session.
	 * ContextJson is a game-defined context value; TitleJson is the localized session title
	 * (token + parameters); UsersJson is the array of participants with their initial message/state.
	 * SteamId (making the request on behalf of that user) is omitted when empty.
	 * AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Notifications", meta = (AutoCreateRefTerm = "OnResponse"))
	void CreateSession(int32 AppId, FString ContextJson, FString TitleJson, FString UsersJson, FString SteamId, const FOnSteamWebResponse& OnResponse);

	/**
	 * IGameNotificationsService/UpdateSession/v1 (POST) — updates an existing session's title
	 * and/or user states. TitleJson/UsersJson/SteamId are optional and omitted when empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Notifications", meta = (AutoCreateRefTerm = "OnResponse"))
	void UpdateSession(int64 SessionId, int32 AppId, FString TitleJson, FString UsersJson, FString SteamId, const FOnSteamWebResponse& OnResponse);

	/**
	 * IGameNotificationsService/EnumerateSessionsForApp/v1 — all sessions for the app that the
	 * given user is a member of. bIncludeAllUserMessages returns every user's message/state,
	 * bIncludeAuthMessages only the requesting user's. Language selects title localization
	 * (omitted when empty). Empty SteamId falls back to DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Notifications", meta = (AutoCreateRefTerm = "OnResponse"))
	void EnumerateSessionsForApp(int32 AppId, FString SteamId, bool bIncludeAllUserMessages, bool bIncludeAuthMessages, FString Language, const FOnSteamWebResponse& OnResponse);

	/**
	 * IGameNotificationsService/GetSessionDetailsForApp/v1 — details for specific sessions.
	 * SessionIdsJson is the JSON "sessions" message (array of {"sessionid": ...} entries).
	 * Language is omitted when empty. Requires the publisher key to own the app.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Notifications", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetSessionDetailsForApp(int32 AppId, FString SessionIdsJson, FString Language, const FOnSteamWebResponse& OnResponse);

	/** IGameNotificationsService/RequestNotifications/v1 (POST) — asks Steam to re-send the user's pending notifications for the app. Empty SteamId falls back to DevSteamId. */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Notifications", meta = (AutoCreateRefTerm = "OnResponse"))
	void RequestNotifications(FString SteamId, int32 AppId, const FOnSteamWebResponse& OnResponse);

	/** IGameNotificationsService/DeleteSession/v1 (POST) — removes a session and its notifications. SteamId (on-behalf-of user) is omitted when empty. */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Notifications", meta = (AutoCreateRefTerm = "OnResponse"))
	void DeleteSession(int64 SessionId, int32 AppId, FString SteamId, const FOnSteamWebResponse& OnResponse);

	/** IGameNotificationsService/DeleteSessionBatch/v1 (POST) — removes multiple sessions at once; SessionIdsCsv is a comma-separated list of session ids sent as "sessionid". */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Notifications", meta = (AutoCreateRefTerm = "OnResponse"))
	void DeleteSessionBatch(FString SessionIdsCsv, int32 AppId, const FOnSteamWebResponse& OnResponse);
};
