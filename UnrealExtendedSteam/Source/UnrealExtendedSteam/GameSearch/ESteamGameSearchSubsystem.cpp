// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "GameSearch/ESteamGameSearchSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

// ISteamGameSearch was removed from the Steamworks SDK in 1.62 (last shipped in 1.61).
// There is no header to include and no interface accessor to call in SDK >= 1.62, so this
// wrapper is a pure stub: every method fails cleanly and OnGameSearchResult never fires.

void UESteamGameSearchSubsystem::LogGameSearchRemoved(const TCHAR* Context) const
{
	UE_LOG(LogExtendedSteam, Verbose,
		TEXT("%s: ISteamGameSearch was removed from the Steamworks SDK (1.62+); this call is a stub"), Context);
}

EESteamGameSearchResult UESteamGameSearchSubsystem::AddGameSearchParams(const FString& KeyToFind, const FString& ValuesToFind)
{
	LogGameSearchRemoved(TEXT("AddGameSearchParams"));
	return EESteamGameSearchResult::Failed;
}

EESteamGameSearchResult UESteamGameSearchSubsystem::SearchForGameWithLobby(FESteamId LobbyId, int32 PlayerMin, int32 PlayerMax)
{
	LogGameSearchRemoved(TEXT("SearchForGameWithLobby"));
	return EESteamGameSearchResult::Failed;
}

EESteamGameSearchResult UESteamGameSearchSubsystem::SearchForGameSolo(int32 PlayerMin, int32 PlayerMax)
{
	LogGameSearchRemoved(TEXT("SearchForGameSolo"));
	return EESteamGameSearchResult::Failed;
}

void UESteamGameSearchSubsystem::AcceptGame()
{
	LogGameSearchRemoved(TEXT("AcceptGame"));
}

void UESteamGameSearchSubsystem::DeclineGame()
{
	LogGameSearchRemoved(TEXT("DeclineGame"));
}

void UESteamGameSearchSubsystem::EndGameSearch()
{
	LogGameSearchRemoved(TEXT("EndGameSearch"));
}
