// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamGameSearchSubsystem.generated.h"

/** Result of a game-search call (maps EGameSearchErrorCode_t: OK vs everything else). */
UENUM(BlueprintType)
enum class EESteamGameSearchResult : uint8
{
	Ok,
	Failed
};

/** Fired when Steam reports a game-search result (a host to join). Never fires on SDKs without ISteamGameSearch. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamGameSearchResult, bool, bSuccess, FESteamId, HostId, bool, bFinalCallback);

/**
 * Wraps ISteamGameSearch (thin, rarely used API).
 *
 * IMPORTANT: Valve removed ISteamGameSearch from the Steamworks SDK in 1.62; the SDK this
 * plugin builds against (1.64, bundled with the engine) no longer ships the interface, so
 * every method here is a documented stub that returns Failed / no-ops with a warning.
 * The Blueprint surface is kept so projects created against it keep compiling if the
 * interface ever returns. The game-host side of the flow (accepting players as a host)
 * was out of the initial scope even while the interface existed.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamGameSearchSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	/** Adds a key/value filter for the next search. ValuesToFind is a comma-separated list. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameSearch")
	EESteamGameSearchResult AddGameSearchParams(const FString& KeyToFind, const FString& ValuesToFind);

	/** Searches for a game as a party: all lobby members are placed together. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameSearch")
	EESteamGameSearchResult SearchForGameWithLobby(FESteamId LobbyId, int32 PlayerMin, int32 PlayerMax);

	/** Searches for a game as a solo player. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameSearch")
	EESteamGameSearchResult SearchForGameSolo(int32 PlayerMin, int32 PlayerMax);

	/** Accepts the game found by the current search. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameSearch")
	void AcceptGame();

	/** Declines the game found by the current search. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameSearch")
	void DeclineGame();

	/** Ends the current game search. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameSearch")
	void EndGameSearch();

	UPROPERTY(BlueprintAssignable, Category = "Steam|GameSearch")
	FOnSteamGameSearchResult OnGameSearchResult;

private:
	/** Logs the standard "interface removed from the SDK" warning for the given call site. */
	void LogGameSearchRemoved(const TCHAR* Context) const;
};
