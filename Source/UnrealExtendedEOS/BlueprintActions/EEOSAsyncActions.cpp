// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSAsyncActions.h"
#include "Stats/EEOSStatsSubsystem.h"
#include "Achievements/EEOSAchievementSubsystem.h"
#include "Auth/EEOSAuthSubsystem.h"
#include "Storage/EEOSPlayerStorageSubsystem.h"
#include "Ecom/EEOSEcomSubsystem.h"
#include "Sessions/EEOSSessionSubsystem.h"
#include "Sessions/EEOSLobbySubsystem.h"
#include "Matchmaking/EEOSMatchmakingSubsystem.h"
#include "Leaderboards/EEOSLeaderboardSubsystem.h"
#include "Social/EEOSFriendsSubsystem.h"
#include "Chat/EEOSChatSubsystem.h"
#include "Sanctions/EEOSSanctionsSubsystem.h"
#include "UnrealExtendedEOS.h"
#include "Engine/GameInstance.h"

// Helper: get a game instance subsystem from a world context
template<typename T>
static T* GetSubsystem(UObject* WorldContext)
{
	if (!WorldContext) return nullptr;
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	if (!GI) return nullptr;
	return GI->GetSubsystem<T>();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Stats — Query
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncQueryStats* UEOSAsyncQueryStats::QueryStats(UObject* WorldContext, const TArray<FString>& StatNames)
{
	UEOSAsyncQueryStats* Action = NewObject<UEOSAsyncQueryStats>();
	Action->WorldContext = WorldContext;
	Action->StatNames = StatNames;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncQueryStats::Activate()
{
	auto* Sub = GetSubsystem<UEEOSStatsSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("StatsSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnStatsQueried.AddDynamic(this, &UEOSAsyncQueryStats::HandleComplete);
	Sub->QueryLocalStats(StatNames);
}

void UEOSAsyncQueryStats::HandleComplete(bool bSuccess, const TArray<FEEOSStat>& Stats)
{
	auto* Sub = GetSubsystem<UEEOSStatsSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnStatsQueried.RemoveDynamic(this, &UEOSAsyncQueryStats::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast(Stats);
	else
		OnFailure.Broadcast(TEXT("QueryLocalStats failed"));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Stats — Ingest
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncIngestStat* UEOSAsyncIngestStat::IngestStat(UObject* WorldContext, const FString& StatName, int32 Amount)
{
	UEOSAsyncIngestStat* Action = NewObject<UEOSAsyncIngestStat>();
	Action->WorldContext = WorldContext;
	Action->StatName = StatName;
	Action->Amount = Amount;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncIngestStat::Activate()
{
	auto* Sub = GetSubsystem<UEEOSStatsSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("StatsSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnStatIngested.AddDynamic(this, &UEOSAsyncIngestStat::HandleComplete);
	Sub->IngestStat(StatName, Amount);
}

void UEOSAsyncIngestStat::HandleComplete(bool bSuccess)
{
	auto* Sub = GetSubsystem<UEEOSStatsSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnStatIngested.RemoveDynamic(this, &UEOSAsyncIngestStat::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast();
	else
		OnFailure.Broadcast(TEXT("IngestStat failed"));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Achievements — Query
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncQueryAchievements* UEOSAsyncQueryAchievements::QueryAchievements(UObject* WorldContext)
{
	UEOSAsyncQueryAchievements* Action = NewObject<UEOSAsyncQueryAchievements>();
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncQueryAchievements::Activate()
{
	auto* Sub = GetSubsystem<UEEOSAchievementSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("AchievementSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnAchievementsQueried.AddDynamic(this, &UEOSAsyncQueryAchievements::HandleComplete);
	Sub->QueryAchievements();
}

void UEOSAsyncQueryAchievements::HandleComplete(bool bSuccess, const TArray<FEEOSAchievement>& Achievements)
{
	auto* Sub = GetSubsystem<UEEOSAchievementSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnAchievementsQueried.RemoveDynamic(this, &UEOSAsyncQueryAchievements::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast(Achievements);
	else
		OnFailure.Broadcast(TEXT("QueryAchievements failed"));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Achievements — Unlock
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncUnlockAchievement* UEOSAsyncUnlockAchievement::UnlockAchievement(UObject* WorldContext, const FString& AchievementId)
{
	UEOSAsyncUnlockAchievement* Action = NewObject<UEOSAsyncUnlockAchievement>();
	Action->WorldContext = WorldContext;
	Action->AchievementId = AchievementId;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncUnlockAchievement::Activate()
{
	auto* Sub = GetSubsystem<UEEOSAchievementSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("AchievementSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnAchievementUnlocked.AddDynamic(this, &UEOSAsyncUnlockAchievement::HandleComplete);
	Sub->UnlockAchievement(AchievementId);
}

void UEOSAsyncUnlockAchievement::HandleComplete(bool bSuccess, const FString& Id)
{
	auto* Sub = GetSubsystem<UEEOSAchievementSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnAchievementUnlocked.RemoveDynamic(this, &UEOSAsyncUnlockAchievement::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast(Id);
	else
		OnFailure.Broadcast(FString::Printf(TEXT("UnlockAchievement '%s' failed"), *Id));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Connect Login
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncConnectLogin* UEOSAsyncConnectLogin::ConnectLogin(UObject* WorldContext, EEOSConnectLoginType LoginType, const FString& Token, const FString& DisplayName)
{
	UEOSAsyncConnectLogin* Action = NewObject<UEOSAsyncConnectLogin>();
	Action->WorldContext = WorldContext;
	Action->LoginType = LoginType;
	Action->Token = Token;
	Action->DisplayName = DisplayName;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncConnectLogin::Activate()
{
	auto* Sub = GetSubsystem<UEEOSAuthSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("AuthSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnConnectLoginComplete.AddDynamic(this, &UEOSAsyncConnectLogin::HandleComplete);

	if (LoginType == EEOSConnectLoginType::DeviceId)
	{
		Sub->ConnectLoginWithDeviceId(DisplayName);
	}
	else
	{
		Sub->ConnectLogin(LoginType, Token);
	}
}

void UEOSAsyncConnectLogin::HandleComplete(bool bSuccess, const FString& ProductUserId, const FString& Error)
{
	auto* Sub = GetSubsystem<UEEOSAuthSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnConnectLoginComplete.RemoveDynamic(this, &UEOSAsyncConnectLogin::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast(ProductUserId);
	else
		OnFailure.Broadcast(Error);

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Player Storage — Read SaveGame
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncReadSaveGame* UEOSAsyncReadSaveGame::ReadSaveGame(UObject* WorldContext, const FString& FileName)
{
	UEOSAsyncReadSaveGame* Action = NewObject<UEOSAsyncReadSaveGame>();
	Action->WorldContext = WorldContext;
	Action->FileName = FileName;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncReadSaveGame::Activate()
{
	auto* Sub = GetSubsystem<UEEOSPlayerStorageSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("PlayerStorageSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnSaveGameRead.AddDynamic(this, &UEOSAsyncReadSaveGame::HandleComplete);
	Sub->ReadSaveGame(FileName);
}

void UEOSAsyncReadSaveGame::HandleComplete(bool bSuccess, const FString& InFileName, USaveGame* SaveGame)
{
	auto* Sub = GetSubsystem<UEEOSPlayerStorageSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnSaveGameRead.RemoveDynamic(this, &UEOSAsyncReadSaveGame::HandleComplete);

	if (bSuccess && SaveGame)
		OnSuccess.Broadcast(SaveGame);
	else
		OnFailure.Broadcast(FString::Printf(TEXT("ReadSaveGame '%s' failed"), *InFileName));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Player Storage — Write SaveGame
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncWriteSaveGame* UEOSAsyncWriteSaveGame::WriteSaveGame(UObject* WorldContext, const FString& FileName, USaveGame* SaveGameObject)
{
	UEOSAsyncWriteSaveGame* Action = NewObject<UEOSAsyncWriteSaveGame>();
	Action->WorldContext = WorldContext;
	Action->FileName = FileName;
	Action->SaveGameObject = SaveGameObject;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncWriteSaveGame::Activate()
{
	auto* Sub = GetSubsystem<UEEOSPlayerStorageSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("PlayerStorageSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnPlayerDataWritten.AddDynamic(this, &UEOSAsyncWriteSaveGame::HandleComplete);
	Sub->WriteSaveGame(FileName, SaveGameObject);
}

void UEOSAsyncWriteSaveGame::HandleComplete(bool bSuccess, const FString& InFileName)
{
	auto* Sub = GetSubsystem<UEEOSPlayerStorageSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnPlayerDataWritten.RemoveDynamic(this, &UEOSAsyncWriteSaveGame::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast();
	else
		OnFailure.Broadcast(FString::Printf(TEXT("WriteSaveGame '%s' failed"), *InFileName));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Ecom — Query Offers
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncQueryOffers* UEOSAsyncQueryOffers::QueryOffers(UObject* WorldContext)
{
	UEOSAsyncQueryOffers* Action = NewObject<UEOSAsyncQueryOffers>();
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncQueryOffers::Activate()
{
	auto* Sub = GetSubsystem<UEEOSEcomSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("EcomSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnOffersQueried.AddDynamic(this, &UEOSAsyncQueryOffers::HandleComplete);
	Sub->QueryOffers();
}

void UEOSAsyncQueryOffers::HandleComplete(bool bSuccess, const TArray<FEEOSCatalogOffer>& Offers)
{
	auto* Sub = GetSubsystem<UEEOSEcomSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnOffersQueried.RemoveDynamic(this, &UEOSAsyncQueryOffers::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast(Offers);
	else
		OnFailure.Broadcast(TEXT("QueryOffers failed"));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Ecom — Checkout
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncCheckout* UEOSAsyncCheckout::Checkout(UObject* WorldContext, const FString& OfferId)
{
	UEOSAsyncCheckout* Action = NewObject<UEOSAsyncCheckout>();
	Action->WorldContext = WorldContext;
	Action->OfferId = OfferId;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncCheckout::Activate()
{
	auto* Sub = GetSubsystem<UEEOSEcomSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("EcomSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnCheckoutComplete.AddDynamic(this, &UEOSAsyncCheckout::HandleComplete);
	Sub->Checkout(OfferId);
}

void UEOSAsyncCheckout::HandleComplete(bool bSuccess, const FString& TransactionId)
{
	auto* Sub = GetSubsystem<UEEOSEcomSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnCheckoutComplete.RemoveDynamic(this, &UEOSAsyncCheckout::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast(TransactionId);
	else
		OnFailure.Broadcast(TEXT("Checkout failed"));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Sessions — Create
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncCreateSession* UEOSAsyncCreateSession::CreateSession(UObject* WorldContext, int32 MaxPlayers, bool bIsLAN, bool bIsPresence, const FString& SessionName)
{
	UEOSAsyncCreateSession* Action = NewObject<UEOSAsyncCreateSession>();
	Action->WorldContext = WorldContext;
	Action->MaxPlayers = MaxPlayers;
	Action->bIsLAN = bIsLAN;
	Action->bIsPresence = bIsPresence;
	Action->SessionName = SessionName;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncCreateSession::Activate()
{
	auto* Sub = GetSubsystem<UEEOSSessionSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("SessionSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnSessionCreated.AddDynamic(this, &UEOSAsyncCreateSession::HandleComplete);
	Sub->CreateSession(MaxPlayers, bIsLAN, bIsPresence, SessionName);
}

void UEOSAsyncCreateSession::HandleComplete(bool bSuccess, const FString& InSessionName)
{
	auto* Sub = GetSubsystem<UEEOSSessionSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnSessionCreated.RemoveDynamic(this, &UEOSAsyncCreateSession::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast(InSessionName);
	else
		OnFailure.Broadcast(FString::Printf(TEXT("CreateSession '%s' failed"), *InSessionName));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Sessions — Find
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncFindSessions* UEOSAsyncFindSessions::FindSessions(UObject* WorldContext, int32 MaxResults)
{
	UEOSAsyncFindSessions* Action = NewObject<UEOSAsyncFindSessions>();
	Action->WorldContext = WorldContext;
	Action->MaxResults = MaxResults;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncFindSessions::Activate()
{
	auto* Sub = GetSubsystem<UEEOSSessionSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("SessionSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnSessionsFound.AddDynamic(this, &UEOSAsyncFindSessions::HandleComplete);
	Sub->FindSessions(MaxResults);
}

void UEOSAsyncFindSessions::HandleComplete(const TArray<FEEOSSessionSearchResult>& Results)
{
	auto* Sub = GetSubsystem<UEEOSSessionSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnSessionsFound.RemoveDynamic(this, &UEOSAsyncFindSessions::HandleComplete);

	if (Results.Num() > 0)
		OnSuccess.Broadcast(Results);
	else
		OnFailure.Broadcast(TEXT("No sessions found"));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Sessions — Join
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncJoinSession* UEOSAsyncJoinSession::JoinSession(UObject* WorldContext, int32 SearchResultIndex, const FString& SessionName)
{
	UEOSAsyncJoinSession* Action = NewObject<UEOSAsyncJoinSession>();
	Action->WorldContext = WorldContext;
	Action->SearchResultIndex = SearchResultIndex;
	Action->SessionName = SessionName;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncJoinSession::Activate()
{
	auto* Sub = GetSubsystem<UEEOSSessionSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("SessionSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnSessionJoined.AddDynamic(this, &UEOSAsyncJoinSession::HandleComplete);
	Sub->JoinSession(SearchResultIndex, SessionName);
}

void UEOSAsyncJoinSession::HandleComplete(bool bSuccess, const FString& InSessionName)
{
	auto* Sub = GetSubsystem<UEEOSSessionSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnSessionJoined.RemoveDynamic(this, &UEOSAsyncJoinSession::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast(InSessionName);
	else
		OnFailure.Broadcast(FString::Printf(TEXT("JoinSession '%s' failed"), *InSessionName));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Lobbies — Create
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncCreateLobby* UEOSAsyncCreateLobby::CreateLobby(UObject* WorldContext, int32 MaxMembers, bool bIsPublic, bool bUseVoiceChat)
{
	UEOSAsyncCreateLobby* Action = NewObject<UEOSAsyncCreateLobby>();
	Action->WorldContext = WorldContext;
	Action->MaxMembers = MaxMembers;
	Action->bIsPublic = bIsPublic;
	Action->bUseVoiceChat = bUseVoiceChat;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncCreateLobby::Activate()
{
	auto* Sub = GetSubsystem<UEEOSLobbySubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("LobbySubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnLobbyCreated.AddDynamic(this, &UEOSAsyncCreateLobby::HandleComplete);
	Sub->CreateLobby(MaxMembers, bIsPublic, bUseVoiceChat);
}

void UEOSAsyncCreateLobby::HandleComplete(bool bSuccess, const FString& LobbyId)
{
	auto* Sub = GetSubsystem<UEEOSLobbySubsystem>(WorldContext.Get());
	if (Sub) Sub->OnLobbyCreated.RemoveDynamic(this, &UEOSAsyncCreateLobby::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast(LobbyId);
	else
		OnFailure.Broadcast(TEXT("CreateLobby failed"));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Lobbies — Find
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncFindLobbies* UEOSAsyncFindLobbies::FindLobbies(UObject* WorldContext, int32 MaxResults)
{
	UEOSAsyncFindLobbies* Action = NewObject<UEOSAsyncFindLobbies>();
	Action->WorldContext = WorldContext;
	Action->MaxResults = MaxResults;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncFindLobbies::Activate()
{
	auto* Sub = GetSubsystem<UEEOSLobbySubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("LobbySubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnLobbiesFound.AddDynamic(this, &UEOSAsyncFindLobbies::HandleComplete);
	Sub->FindLobbies(MaxResults);
}

void UEOSAsyncFindLobbies::HandleComplete(const TArray<FEEOSSessionSearchResult>& Results)
{
	auto* Sub = GetSubsystem<UEEOSLobbySubsystem>(WorldContext.Get());
	if (Sub) Sub->OnLobbiesFound.RemoveDynamic(this, &UEOSAsyncFindLobbies::HandleComplete);

	if (Results.Num() > 0)
		OnSuccess.Broadcast(Results);
	else
		OnFailure.Broadcast(TEXT("No lobbies found"));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Lobbies — Join
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncJoinLobby* UEOSAsyncJoinLobby::JoinLobby(UObject* WorldContext, int32 SearchResultIndex)
{
	UEOSAsyncJoinLobby* Action = NewObject<UEOSAsyncJoinLobby>();
	Action->WorldContext = WorldContext;
	Action->SearchResultIndex = SearchResultIndex;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncJoinLobby::Activate()
{
	auto* Sub = GetSubsystem<UEEOSLobbySubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("LobbySubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnLobbyJoined.AddDynamic(this, &UEOSAsyncJoinLobby::HandleComplete);
	Sub->JoinLobby(SearchResultIndex);
}

void UEOSAsyncJoinLobby::HandleComplete(bool bSuccess, const FString& LobbyId)
{
	auto* Sub = GetSubsystem<UEEOSLobbySubsystem>(WorldContext.Get());
	if (Sub) Sub->OnLobbyJoined.RemoveDynamic(this, &UEOSAsyncJoinLobby::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast(LobbyId);
	else
		OnFailure.Broadcast(TEXT("JoinLobby failed"));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Matchmaking — Start
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncStartMatchmaking* UEOSAsyncStartMatchmaking::StartMatchmaking(UObject* WorldContext, const FString& QueueName)
{
	UEOSAsyncStartMatchmaking* Action = NewObject<UEOSAsyncStartMatchmaking>();
	Action->WorldContext = WorldContext;
	Action->QueueName = QueueName;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncStartMatchmaking::Activate()
{
	auto* Sub = GetSubsystem<UEEOSMatchmakingSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("MatchmakingSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnMatchFound.AddDynamic(this, &UEOSAsyncStartMatchmaking::HandleMatchFound);
	Sub->OnMatchmakingComplete.AddDynamic(this, &UEOSAsyncStartMatchmaking::HandleComplete);
	Sub->StartMatchmaking(QueueName);
}

void UEOSAsyncStartMatchmaking::HandleMatchFound(const FString& SessionId)
{
	auto* Sub = GetSubsystem<UEEOSMatchmakingSubsystem>(WorldContext.Get());
	if (Sub)
	{
		Sub->OnMatchFound.RemoveDynamic(this, &UEOSAsyncStartMatchmaking::HandleMatchFound);
		Sub->OnMatchmakingComplete.RemoveDynamic(this, &UEOSAsyncStartMatchmaking::HandleComplete);
	}

	OnSuccess.Broadcast(SessionId);
	SetReadyToDestroy();
}

void UEOSAsyncStartMatchmaking::HandleComplete(bool bSuccess, const FString& ErrorMessage)
{
	auto* Sub = GetSubsystem<UEEOSMatchmakingSubsystem>(WorldContext.Get());
	if (Sub)
	{
		Sub->OnMatchFound.RemoveDynamic(this, &UEOSAsyncStartMatchmaking::HandleMatchFound);
		Sub->OnMatchmakingComplete.RemoveDynamic(this, &UEOSAsyncStartMatchmaking::HandleComplete);
	}

	if (!bSuccess)
		OnFailure.Broadcast(ErrorMessage);

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Leaderboards — Query
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncQueryLeaderboard* UEOSAsyncQueryLeaderboard::QueryLeaderboard(UObject* WorldContext, const FString& LeaderboardId)
{
	UEOSAsyncQueryLeaderboard* Action = NewObject<UEOSAsyncQueryLeaderboard>();
	Action->WorldContext = WorldContext;
	Action->LeaderboardId = LeaderboardId;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncQueryLeaderboard::Activate()
{
	auto* Sub = GetSubsystem<UEEOSLeaderboardSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("LeaderboardSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnLeaderboardQueried.AddDynamic(this, &UEOSAsyncQueryLeaderboard::HandleComplete);
	Sub->QueryLeaderboard(LeaderboardId);
}

void UEOSAsyncQueryLeaderboard::HandleComplete(bool bSuccess, const TArray<FEEOSLeaderboardEntry>& Entries)
{
	auto* Sub = GetSubsystem<UEEOSLeaderboardSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnLeaderboardQueried.RemoveDynamic(this, &UEOSAsyncQueryLeaderboard::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast(Entries);
	else
		OnFailure.Broadcast(TEXT("QueryLeaderboard failed"));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Leaderboards — Upload Score
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncUploadScore* UEOSAsyncUploadScore::UploadScore(UObject* WorldContext, const FString& LeaderboardId, int32 Score)
{
	UEOSAsyncUploadScore* Action = NewObject<UEOSAsyncUploadScore>();
	Action->WorldContext = WorldContext;
	Action->LeaderboardId = LeaderboardId;
	Action->Score = Score;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncUploadScore::Activate()
{
	auto* Sub = GetSubsystem<UEEOSLeaderboardSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("LeaderboardSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnScoreUploaded.AddDynamic(this, &UEOSAsyncUploadScore::HandleComplete);
	Sub->UploadScore(LeaderboardId, Score);
}

void UEOSAsyncUploadScore::HandleComplete(bool bSuccess, const FString& InLeaderboardId)
{
	auto* Sub = GetSubsystem<UEEOSLeaderboardSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnScoreUploaded.RemoveDynamic(this, &UEOSAsyncUploadScore::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast(InLeaderboardId);
	else
		OnFailure.Broadcast(TEXT("UploadScore failed"));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Friends — Read List
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncReadFriendsList* UEOSAsyncReadFriendsList::ReadFriendsList(UObject* WorldContext)
{
	UEOSAsyncReadFriendsList* Action = NewObject<UEOSAsyncReadFriendsList>();
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncReadFriendsList::Activate()
{
	auto* Sub = GetSubsystem<UEEOSFriendsSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("FriendsSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnFriendsListReady.AddDynamic(this, &UEOSAsyncReadFriendsList::HandleComplete);
	Sub->ReadFriendsList();
}

void UEOSAsyncReadFriendsList::HandleComplete(const TArray<FEEOSFriendInfo>& Friends)
{
	auto* Sub = GetSubsystem<UEEOSFriendsSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnFriendsListReady.RemoveDynamic(this, &UEOSAsyncReadFriendsList::HandleComplete);

	OnSuccess.Broadcast(Friends);
	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Chat — Join Channel
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncJoinChatChannel* UEOSAsyncJoinChatChannel::JoinChatChannel(UObject* WorldContext, const FString& ChannelName)
{
	UEOSAsyncJoinChatChannel* Action = NewObject<UEOSAsyncJoinChatChannel>();
	Action->WorldContext = WorldContext;
	Action->ChannelName = ChannelName;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncJoinChatChannel::Activate()
{
	auto* Sub = GetSubsystem<UEEOSChatSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("ChatSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnChannelJoined.AddDynamic(this, &UEOSAsyncJoinChatChannel::HandleComplete);
	Sub->JoinChannel(ChannelName);
}

void UEOSAsyncJoinChatChannel::HandleComplete(bool bSuccess, const FString& InChannelName)
{
	auto* Sub = GetSubsystem<UEEOSChatSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnChannelJoined.RemoveDynamic(this, &UEOSAsyncJoinChatChannel::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast(InChannelName);
	else
		OnFailure.Broadcast(FString::Printf(TEXT("Failed to join channel '%s'"), *InChannelName));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Chat — Send Message
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncSendChatMessage* UEOSAsyncSendChatMessage::SendChatMessage(UObject* WorldContext, const FString& ChannelName, const FString& Message)
{
	UEOSAsyncSendChatMessage* Action = NewObject<UEOSAsyncSendChatMessage>();
	Action->WorldContext = WorldContext;
	Action->ChannelName = ChannelName;
	Action->Message = Message;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncSendChatMessage::Activate()
{
	auto* Sub = GetSubsystem<UEEOSChatSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("ChatSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnMessageSent.AddDynamic(this, &UEOSAsyncSendChatMessage::HandleComplete);
	Sub->SendMessage(ChannelName, Message);
}

void UEOSAsyncSendChatMessage::HandleComplete(bool bSuccess, const FString& InChannelName)
{
	auto* Sub = GetSubsystem<UEEOSChatSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnMessageSent.RemoveDynamic(this, &UEOSAsyncSendChatMessage::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast(InChannelName);
	else
		OnFailure.Broadcast(FString::Printf(TEXT("Failed to send message to '%s'"), *InChannelName));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Sanctions — Query
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncQuerySanctions* UEOSAsyncQuerySanctions::QuerySanctions(UObject* WorldContext, const FString& TargetUserId)
{
	UEOSAsyncQuerySanctions* Action = NewObject<UEOSAsyncQuerySanctions>();
	Action->WorldContext = WorldContext;
	Action->TargetUserId = TargetUserId;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncQuerySanctions::Activate()
{
	auto* Sub = GetSubsystem<UEEOSSanctionsSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("SanctionsSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnSanctionsQueried.AddDynamic(this, &UEOSAsyncQuerySanctions::HandleComplete);
	Sub->QueryActiveSanctions(TargetUserId);
}

void UEOSAsyncQuerySanctions::HandleComplete(bool bSuccess, const TArray<FEEOSSanction>& Sanctions)
{
	auto* Sub = GetSubsystem<UEEOSSanctionsSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnSanctionsQueried.RemoveDynamic(this, &UEOSAsyncQuerySanctions::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast(Sanctions);
	else
		OnFailure.Broadcast(TEXT("QuerySanctions failed"));

	SetReadyToDestroy();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Sanctions — Report Player
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncReportPlayer* UEOSAsyncReportPlayer::ReportPlayer(UObject* WorldContext, const FString& TargetUserId, const FString& Reason, const FString& Message)
{
	UEOSAsyncReportPlayer* Action = NewObject<UEOSAsyncReportPlayer>();
	Action->WorldContext = WorldContext;
	Action->TargetUserId = TargetUserId;
	Action->Reason = Reason;
	Action->Message = Message;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEOSAsyncReportPlayer::Activate()
{
	auto* Sub = GetSubsystem<UEEOSSanctionsSubsystem>(WorldContext.Get());
	if (!Sub)
	{
		OnFailure.Broadcast(TEXT("SanctionsSubsystem not available"));
		SetReadyToDestroy();
		return;
	}

	Sub->OnPlayerReportSent.AddDynamic(this, &UEOSAsyncReportPlayer::HandleComplete);
	Sub->SendPlayerReport(TargetUserId, Reason, Message);
}

void UEOSAsyncReportPlayer::HandleComplete(bool bSuccess)
{
	auto* Sub = GetSubsystem<UEEOSSanctionsSubsystem>(WorldContext.Get());
	if (Sub) Sub->OnPlayerReportSent.RemoveDynamic(this, &UEOSAsyncReportPlayer::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast();
	else
		OnFailure.Broadcast(TEXT("ReportPlayer failed"));

	SetReadyToDestroy();
}
