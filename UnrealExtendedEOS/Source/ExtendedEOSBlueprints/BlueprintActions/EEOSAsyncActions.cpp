// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/EEOSAsyncActions.h"
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
#include "Shared/EEOSBlueprintLibrary.h"
#include "UnrealExtendedEOS.h"
#include "Engine/GameInstance.h"

// Helper: get a game instance subsystem from a world context
template<typename T>
static T* GetSubsystem(UObject* WorldContext)
{
	if (!WorldContext || !GEngine) return nullptr;
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	if (!GI) return nullptr;
	return GI->GetSubsystem<T>();
}

// NOTE on the uniform Activate pattern (see the header's "Completion pattern" block):
// handlers are bound BEFORE the subsystem call so a synchronous pre-flight failure
// broadcast still reaches this node. When the call returns false AND no handler fired
// (bCompleted is false), the subsystem will never broadcast for this call — the node
// unbinds and fires OnFailure exactly once.

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

	Subsystem = Sub;
	Sub->OnStatsQueried.AddDynamic(this, &UEOSAsyncQueryStats::HandleComplete);
	if (!Sub->QueryLocalStats(StatNames) && !bCompleted)
	{
		Sub->OnStatsQueried.RemoveDynamic(this, &UEOSAsyncQueryStats::HandleComplete);
		OnFailure.Broadcast(TEXT("QueryLocalStats could not be started"));
		SetReadyToDestroy();
	}
}

void UEOSAsyncQueryStats::HandleComplete(bool bSuccess, const TArray<FEEOSStat>& Stats)
{
	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	Subsystem = Sub;
	Sub->OnStatIngested.AddDynamic(this, &UEOSAsyncIngestStat::HandleComplete);
	if (!Sub->IngestStat(StatName, Amount) && !bCompleted)
	{
		Sub->OnStatIngested.RemoveDynamic(this, &UEOSAsyncIngestStat::HandleComplete);
		OnFailure.Broadcast(FString::Printf(TEXT("IngestStat '%s' could not be started"), *StatName));
		SetReadyToDestroy();
	}
}

void UEOSAsyncIngestStat::HandleComplete(bool bSuccess)
{
	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	Subsystem = Sub;
	Sub->OnAchievementsQueried.AddDynamic(this, &UEOSAsyncQueryAchievements::HandleComplete);
	if (!Sub->QueryAchievements() && !bCompleted)
	{
		Sub->OnAchievementsQueried.RemoveDynamic(this, &UEOSAsyncQueryAchievements::HandleComplete);
		OnFailure.Broadcast(TEXT("QueryAchievements could not be started"));
		SetReadyToDestroy();
	}
}

void UEOSAsyncQueryAchievements::HandleComplete(bool bSuccess, const TArray<FEEOSAchievement>& Achievements)
{
	// No request key in the delegate payload — first-completion semantics: if two REAL
	// operations of this type are in flight, both nodes complete on whichever completion
	// broadcasts first. (In-flight rejections no longer broadcast, so a rejected duplicate
	// can no longer falsely complete this node — it fails fast in Activate instead.)
	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	Subsystem = Sub;
	Sub->OnAchievementUnlocked.AddDynamic(this, &UEOSAsyncUnlockAchievement::HandleComplete);
	if (!Sub->UnlockAchievement(AchievementId) && !bCompleted)
	{
		Sub->OnAchievementUnlocked.RemoveDynamic(this, &UEOSAsyncUnlockAchievement::HandleComplete);
		OnFailure.Broadcast(FString::Printf(TEXT("UnlockAchievement '%s' could not be started"), *AchievementId));
		SetReadyToDestroy();
	}
}

void UEOSAsyncUnlockAchievement::HandleComplete(bool bSuccess, const FString& Id)
{
	// Correlate by achievement ID: a completion for a different in-flight unlock is not ours —
	// stay bound and wait for our own.
	if (Id != AchievementId) return;

	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	Subsystem = Sub;
	Sub->OnConnectLoginComplete.AddDynamic(this, &UEOSAsyncConnectLogin::HandleComplete);

	const bool bStarted = (LoginType == EEOSConnectLoginType::DeviceId)
		? Sub->ConnectLoginWithDeviceId(DisplayName)
		: Sub->ConnectLogin(LoginType, Token);

	if (!bStarted && !bCompleted)
	{
		// false without a broadcast = rejected because a Connect login is already in flight
		// (other pre-flight failures broadcast OnConnectLoginComplete(false) synchronously
		// and were consumed by the handler above).
		Sub->OnConnectLoginComplete.RemoveDynamic(this, &UEOSAsyncConnectLogin::HandleComplete);
		OnFailure.Broadcast(TEXT("Connect login could not be started (a Connect login is already in flight)"));
		SetReadyToDestroy();
	}
}

void UEOSAsyncConnectLogin::HandleComplete(bool bSuccess, const FString& ProductUserId, const FString& Error)
{
	// No request key in the delegate payload — first-completion semantics: if two REAL
	// logins are in flight (not possible through this subsystem's guard, but e.g. auto-login
	// plus this node), both listeners complete on whichever completion broadcasts first.
	// (In-flight rejections no longer broadcast, so a rejected duplicate can no longer
	// falsely complete this node — it fails fast in Activate instead.)
	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	Subsystem = Sub;
	Sub->OnSaveGameRead.AddDynamic(this, &UEOSAsyncReadSaveGame::HandleComplete);
	if (!Sub->ReadSaveGame(FileName) && !bCompleted)
	{
		// false without a broadcast = same-file duplicate rejected log-only (the in-flight
		// operation's completion must not be misattributed to this call).
		Sub->OnSaveGameRead.RemoveDynamic(this, &UEOSAsyncReadSaveGame::HandleComplete);
		OnFailure.Broadcast(FString::Printf(TEXT("ReadSaveGame '%s' could not be started (an operation on this file is already in flight)"), *FileName));
		SetReadyToDestroy();
	}
}

void UEOSAsyncReadSaveGame::HandleComplete(bool bSuccess, const FString& InFileName, USaveGame* SaveGame)
{
	// Correlate by file name: a completion for a different in-flight read is not ours —
	// stay bound and wait for our own.
	if (InFileName != FileName) return;

	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	Subsystem = Sub;
	Sub->OnPlayerDataWritten.AddDynamic(this, &UEOSAsyncWriteSaveGame::HandleComplete);
	if (!Sub->WriteSaveGame(FileName, SaveGameObject) && !bCompleted)
	{
		// false without a broadcast = same-file duplicate rejected log-only (the in-flight
		// operation's completion must not be misattributed to this call).
		Sub->OnPlayerDataWritten.RemoveDynamic(this, &UEOSAsyncWriteSaveGame::HandleComplete);
		OnFailure.Broadcast(FString::Printf(TEXT("WriteSaveGame '%s' could not be started (an operation on this file is already in flight)"), *FileName));
		SetReadyToDestroy();
	}
}

void UEOSAsyncWriteSaveGame::HandleComplete(bool bSuccess, const FString& InFileName)
{
	// Correlate by file name: a completion for a different in-flight write is not ours —
	// stay bound and wait for our own.
	if (InFileName != FileName) return;

	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	Subsystem = Sub;
	Sub->OnOffersQueried.AddDynamic(this, &UEOSAsyncQueryOffers::HandleComplete);
	if (!Sub->QueryOffers() && !bCompleted)
	{
		Sub->OnOffersQueried.RemoveDynamic(this, &UEOSAsyncQueryOffers::HandleComplete);
		OnFailure.Broadcast(TEXT("QueryOffers could not be started (an offers query is already in flight)"));
		SetReadyToDestroy();
	}
}

void UEOSAsyncQueryOffers::HandleComplete(bool bSuccess, const TArray<FEEOSCatalogOffer>& Offers)
{
	// No request key in the delegate payload — first-completion semantics: if two REAL
	// operations of this type are in flight, both nodes complete on whichever completion
	// broadcasts first. (In-flight rejections no longer broadcast, so a rejected duplicate
	// can no longer falsely complete this node — it fails fast in Activate instead.)
	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	Subsystem = Sub;
	Sub->OnCheckoutComplete.AddDynamic(this, &UEOSAsyncCheckout::HandleComplete);
	if (!Sub->Checkout(OfferId) && !bCompleted)
	{
		Sub->OnCheckoutComplete.RemoveDynamic(this, &UEOSAsyncCheckout::HandleComplete);
		OnFailure.Broadcast(FString::Printf(TEXT("Checkout '%s' could not be started (a checkout is already in flight)"), *OfferId));
		SetReadyToDestroy();
	}
}

void UEOSAsyncCheckout::HandleComplete(bool bSuccess, const FString& TransactionId)
{
	// No request key in the delegate payload (TransactionId is a result, not the requested
	// OfferId) — first-completion semantics: if two REAL checkouts are in flight, both nodes
	// complete on whichever completion broadcasts first. (In-flight rejections no longer
	// broadcast, so a rejected duplicate can no longer falsely complete this node.)
	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	Subsystem = Sub;
	Sub->OnSessionCreated.AddDynamic(this, &UEOSAsyncCreateSession::HandleComplete);
	if (!Sub->CreateSession(MaxPlayers, bIsLAN, bIsPresence, SessionName) && !bCompleted)
	{
		// false without a broadcast = rejected (a create/destroy is already in flight); the
		// pre-flight cases broadcast OnSessionCreated(false) synchronously and were consumed
		// by the handler above.
		Sub->OnSessionCreated.RemoveDynamic(this, &UEOSAsyncCreateSession::HandleComplete);
		OnFailure.Broadcast(FString::Printf(TEXT("CreateSession '%s' could not be started (a session create/destroy is already in flight)"), *SessionName));
		SetReadyToDestroy();
	}
}

void UEOSAsyncCreateSession::HandleComplete(bool bSuccess, const FString& InSessionName)
{
	// Correlate by session name: a completion for a different in-flight create is not ours —
	// stay bound and wait for our own.
	if (InSessionName != SessionName) return;

	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	Subsystem = Sub;
	Sub->OnSessionsFound.AddDynamic(this, &UEOSAsyncFindSessions::HandleComplete);
	if (!Sub->FindSessions(MaxResults) && !bCompleted)
	{
		// false without a broadcast = the search was rejected (our own or a sibling
		// session/lobby search is already in flight). The OnFailure doc contract —
		// "fires when the search could not be started" — is exactly this path.
		Sub->OnSessionsFound.RemoveDynamic(this, &UEOSAsyncFindSessions::HandleComplete);
		OnFailure.Broadcast(TEXT("FindSessions could not be started (another session/lobby search is already in flight)"));
		SetReadyToDestroy();
	}
}

void UEOSAsyncFindSessions::HandleComplete(const TArray<FEEOSSessionSearchResult>& Results)
{
	// No request key in the delegate payload — first-completion semantics: if two REAL
	// searches were somehow in flight, both nodes complete on whichever completion broadcasts
	// first. (In-flight rejections no longer broadcast, so a rejected duplicate can no longer
	// falsely complete this node with empty results — it fails fast in Activate instead.)
	bCompleted = true;
	auto* Sub = Subsystem.Get();
	if (Sub) Sub->OnSessionsFound.RemoveDynamic(this, &UEOSAsyncFindSessions::HandleComplete);

	// A search that completed but found nothing is still a success — empty array maps to
	// OnSuccess. (Pre-flight failures that broadcast deliver an empty result here too; only a
	// search that could not be STARTED fires OnFailure, from Activate's false-return path.)
	OnSuccess.Broadcast(Results);
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

	Subsystem = Sub;
	Sub->OnSessionJoined.AddDynamic(this, &UEOSAsyncJoinSession::HandleComplete);
	if (!Sub->JoinSession(SearchResultIndex, SessionName) && !bCompleted)
	{
		Sub->OnSessionJoined.RemoveDynamic(this, &UEOSAsyncJoinSession::HandleComplete);
		OnFailure.Broadcast(FString::Printf(TEXT("JoinSession '%s' could not be started (a join is already in flight)"), *SessionName));
		SetReadyToDestroy();
	}
}

void UEOSAsyncJoinSession::HandleComplete(bool bSuccess, const FString& InSessionName)
{
	// Correlate by session name: a completion for a different in-flight join is not ours —
	// stay bound and wait for our own.
	if (InSessionName != SessionName) return;

	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	Subsystem = Sub;
	Sub->OnLobbyCreated.AddDynamic(this, &UEOSAsyncCreateLobby::HandleComplete);
	if (!Sub->CreateLobby(MaxMembers, bIsPublic, bUseVoiceChat) && !bCompleted)
	{
		Sub->OnLobbyCreated.RemoveDynamic(this, &UEOSAsyncCreateLobby::HandleComplete);
		OnFailure.Broadcast(TEXT("CreateLobby could not be started (a lobby create/destroy is already in flight)"));
		SetReadyToDestroy();
	}
}

void UEOSAsyncCreateLobby::HandleComplete(bool bSuccess, const FString& LobbyId)
{
	// No request key in the delegate payload (LobbyId is a result, not a request input) —
	// first-completion semantics: if two REAL creates are in flight, both nodes complete on
	// whichever completion broadcasts first. (In-flight rejections no longer broadcast, so a
	// rejected duplicate can no longer falsely fail this node — it fails fast in Activate.)
	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	Subsystem = Sub;
	Sub->OnLobbiesFound.AddDynamic(this, &UEOSAsyncFindLobbies::HandleComplete);
	if (!Sub->FindLobbies(MaxResults) && !bCompleted)
	{
		// false without a broadcast = the search was rejected (our own or a sibling
		// session/lobby search is already in flight). The OnFailure doc contract —
		// "fires when the search could not be started" — is exactly this path.
		Sub->OnLobbiesFound.RemoveDynamic(this, &UEOSAsyncFindLobbies::HandleComplete);
		OnFailure.Broadcast(TEXT("FindLobbies could not be started (another session/lobby search is already in flight)"));
		SetReadyToDestroy();
	}
}

void UEOSAsyncFindLobbies::HandleComplete(const TArray<FEEOSSessionSearchResult>& Results)
{
	// No request key in the delegate payload — first-completion semantics: if two REAL
	// searches were somehow in flight, both nodes complete on whichever completion broadcasts
	// first. (In-flight rejections no longer broadcast, so a rejected duplicate can no longer
	// falsely complete this node with empty results — it fails fast in Activate instead.)
	bCompleted = true;
	auto* Sub = Subsystem.Get();
	if (Sub) Sub->OnLobbiesFound.RemoveDynamic(this, &UEOSAsyncFindLobbies::HandleComplete);

	// A search that completed but found nothing is still a success — empty array maps to
	// OnSuccess. (Pre-flight failures that broadcast deliver an empty result here too; only a
	// search that could not be STARTED fires OnFailure, from Activate's false-return path.)
	OnSuccess.Broadcast(Results);
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

	Subsystem = Sub;
	Sub->OnLobbyJoined.AddDynamic(this, &UEOSAsyncJoinLobby::HandleComplete);
	if (!Sub->JoinLobby(SearchResultIndex) && !bCompleted)
	{
		Sub->OnLobbyJoined.RemoveDynamic(this, &UEOSAsyncJoinLobby::HandleComplete);
		OnFailure.Broadcast(TEXT("JoinLobby could not be started (a join-lobby is already in flight)"));
		SetReadyToDestroy();
	}
}

void UEOSAsyncJoinLobby::HandleComplete(bool bSuccess, const FString& LobbyId)
{
	// No request key in the delegate payload (LobbyId is a result; the request input is an
	// index) — first-completion semantics: if two REAL joins are in flight, both nodes
	// complete on whichever completion broadcasts first. (In-flight rejections no longer
	// broadcast, so a rejected duplicate can no longer falsely fail this node.)
	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	Subsystem = Sub;
	Sub->OnMatchFound.AddDynamic(this, &UEOSAsyncStartMatchmaking::HandleMatchFound);
	Sub->OnMatchmakingComplete.AddDynamic(this, &UEOSAsyncStartMatchmaking::HandleComplete);
	// A cancelled cycle broadcasts ONLY OnMatchmakingCancelled — without this binding the
	// node would never complete on cancellation and its stale OnSuccess would fire into the
	// abandoned graph on a LATER cycle. Binding it restores the exactly-one-pin guarantee.
	Sub->OnMatchmakingCancelled.AddDynamic(this, &UEOSAsyncStartMatchmaking::HandleCancelled);

	if (!Sub->StartMatchmaking(QueueName) && !bCompleted)
	{
		// false without a broadcast = rejected (a matchmaking cycle is already in flight);
		// the no-cycle pre-flight failures broadcast OnMatchmakingComplete(false)
		// synchronously and were consumed by the handler above.
		UnbindAll();
		OnFailure.Broadcast(TEXT("StartMatchmaking could not be started (a matchmaking cycle is already in flight)"));
		SetReadyToDestroy();
	}
}

void UEOSAsyncStartMatchmaking::UnbindAll()
{
	if (auto* Sub = Subsystem.Get())
	{
		Sub->OnMatchFound.RemoveDynamic(this, &UEOSAsyncStartMatchmaking::HandleMatchFound);
		Sub->OnMatchmakingComplete.RemoveDynamic(this, &UEOSAsyncStartMatchmaking::HandleComplete);
		Sub->OnMatchmakingCancelled.RemoveDynamic(this, &UEOSAsyncStartMatchmaking::HandleCancelled);
	}
}

void UEOSAsyncStartMatchmaking::HandleMatchFound(const FString& SessionId)
{
	// No request key in the delegate payload — first-completion semantics: the subsystem
	// allows one cycle at a time and duplicate starts are log-only rejected (they fail fast
	// in Activate), so the only remaining overlap is two nodes bound to the SAME real cycle —
	// both then complete on this event.
	bCompleted = true;
	UnbindAll();
	OnSuccess.Broadcast(SessionId);
	SetReadyToDestroy();
}

void UEOSAsyncStartMatchmaking::HandleComplete(bool bSuccess, const FString& ErrorMessage)
{
	// See HandleMatchFound for the first-completion semantics note.
	bCompleted = true;
	UnbindAll();

	// Every branch must fire exactly one pin. A successful completion reaching this handler
	// means matchmaking finished (e.g. match accepted and joined) without this node observing
	// OnMatchFound; this delegate does not carry a SessionId, so OnSuccess fires with an
	// empty one (documented on the OnSuccess pin).
	if (bSuccess)
		OnSuccess.Broadcast(FString());
	else
		OnFailure.Broadcast(ErrorMessage);

	SetReadyToDestroy();
}

void UEOSAsyncStartMatchmaking::HandleCancelled()
{
	// CancelMatchmaking ends the cycle with ONLY this broadcast (no OnMatchmakingComplete) —
	// complete the node on its failure pin so exactly one pin always fires.
	bCompleted = true;
	UnbindAll();
	OnFailure.Broadcast(TEXT("Matchmaking cancelled"));
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

	Subsystem = Sub;
	Sub->OnLeaderboardQueried.AddDynamic(this, &UEOSAsyncQueryLeaderboard::HandleComplete);
	if (!Sub->QueryLeaderboard(LeaderboardId) && !bCompleted)
	{
		// false without a broadcast = rejected (another leaderboard query is already in
		// flight — log-only); pre-flight failures broadcast and were consumed above.
		Sub->OnLeaderboardQueried.RemoveDynamic(this, &UEOSAsyncQueryLeaderboard::HandleComplete);
		OnFailure.Broadcast(FString::Printf(TEXT("QueryLeaderboard '%s' could not be started (another leaderboard query is already in flight)"), *LeaderboardId));
		SetReadyToDestroy();
	}
}

void UEOSAsyncQueryLeaderboard::HandleComplete(bool bSuccess, const TArray<FEEOSLeaderboardEntry>& Entries)
{
	// No request key in the delegate payload — first-completion semantics: if two REAL
	// queries are in flight, both nodes complete on whichever completion broadcasts first.
	// (In-flight rejections no longer broadcast, so a rejected duplicate can no longer
	// falsely fail this node while the real query is about to succeed.)
	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	Subsystem = Sub;
	Sub->OnScoreUploaded.AddDynamic(this, &UEOSAsyncUploadScore::HandleComplete);
	if (!Sub->UploadScore(LeaderboardId, Score) && !bCompleted)
	{
		Sub->OnScoreUploaded.RemoveDynamic(this, &UEOSAsyncUploadScore::HandleComplete);
		OnFailure.Broadcast(FString::Printf(TEXT("UploadScore '%s' could not be started"), *LeaderboardId));
		SetReadyToDestroy();
	}
}

void UEOSAsyncUploadScore::HandleComplete(bool bSuccess, const FString& InLeaderboardId)
{
	// Correlate by leaderboard ID: a completion for a different in-flight upload is not ours —
	// stay bound and wait for our own.
	if (InLeaderboardId != LeaderboardId) return;

	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	Subsystem = Sub;
	Sub->OnFriendsListReady.AddDynamic(this, &UEOSAsyncReadFriendsList::HandleComplete);
	if (!Sub->ReadFriendsList() && !bCompleted)
	{
		Sub->OnFriendsListReady.RemoveDynamic(this, &UEOSAsyncReadFriendsList::HandleComplete);
		OnFailure.Broadcast(TEXT("ReadFriendsList could not be started"));
		SetReadyToDestroy();
	}
}

void UEOSAsyncReadFriendsList::HandleComplete(const TArray<FEEOSFriendInfo>& Friends)
{
	// No request key in the delegate payload — first-completion semantics: if two REAL reads
	// are in flight, both nodes complete on whichever completion broadcasts first.
	bCompleted = true;
	auto* Sub = Subsystem.Get();
	if (Sub) Sub->OnFriendsListReady.RemoveDynamic(this, &UEOSAsyncReadFriendsList::HandleComplete);

	// The subsystem broadcasts on every completion path (empty list on failure), so a started
	// read always completes this node here.
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

	Subsystem = Sub;
	Sub->OnChannelJoined.AddDynamic(this, &UEOSAsyncJoinChatChannel::HandleComplete);
	// NOTE: already-joined channels re-broadcast success synchronously during this call —
	// the handler (bound above) completes the node in that case and the return is true.
	if (!Sub->JoinChannel(ChannelName) && !bCompleted)
	{
		Sub->OnChannelJoined.RemoveDynamic(this, &UEOSAsyncJoinChatChannel::HandleComplete);
		OnFailure.Broadcast(FString::Printf(TEXT("JoinChannel '%s' could not be started"), *ChannelName));
		SetReadyToDestroy();
	}
}

void UEOSAsyncJoinChatChannel::HandleComplete(bool bSuccess, const FString& InChannelName)
{
	// Correlate by channel name: a completion for a different in-flight join is not ours —
	// stay bound and wait for our own.
	if (InChannelName != ChannelName) return;

	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	Subsystem = Sub;
	Sub->OnMessageSent.AddDynamic(this, &UEOSAsyncSendChatMessage::HandleComplete);
	if (!Sub->SendMessage(ChannelName, Message) && !bCompleted)
	{
		Sub->OnMessageSent.RemoveDynamic(this, &UEOSAsyncSendChatMessage::HandleComplete);
		OnFailure.Broadcast(FString::Printf(TEXT("SendMessage to '%s' could not be started"), *ChannelName));
		SetReadyToDestroy();
	}
}

void UEOSAsyncSendChatMessage::HandleComplete(bool bSuccess, const FString& InChannelName)
{
	// Correlate by channel name: a completion for a different channel is not ours — stay bound and
	// wait for our own. (Two in-flight sends to the SAME channel still race; the delegate carries no
	// per-message key.)
	if (InChannelName != ChannelName) return;

	bCompleted = true;
	auto* Sub = Subsystem.Get();
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

	// The subsystem broadcasts the BARE target Product User ID on every completion (empty
	// when the input had no parseable PUID) — extract ours the same way so completions can
	// be correlated exactly, including the pre-flight failure echoes.
	TargetPUID = UEEOSBlueprintLibrary::ExtractProductUserId(TargetUserId);

	Subsystem = Sub;
	Sub->OnSanctionsQueried.AddDynamic(this, &UEOSAsyncQuerySanctions::HandleComplete);
	// QueryActiveSanctions is void (no in-flight rejection exists — concurrent queries for
	// different targets run in parallel and are told apart by the target id). Every pre-flight
	// failure broadcasts synchronously, carrying our target PUID, so the handler always
	// completes this node.
	Sub->QueryActiveSanctions(TargetUserId);
}

void UEOSAsyncQuerySanctions::HandleComplete(bool bSuccess, const FString& CompletedTargetUserId, const TArray<FEEOSSanction>& Sanctions)
{
	// Correlate by bare target PUID: a completion for a DIFFERENT target is not ours — stay
	// bound (without unbinding) and wait for our own. An empty broadcast id matches only when
	// our own target had no parseable PUID (that pre-flight failure echo is ours).
	if (CompletedTargetUserId != TargetPUID) return;

	bCompleted = true;
	auto* Sub = Subsystem.Get();
	if (Sub) Sub->OnSanctionsQueried.RemoveDynamic(this, &UEOSAsyncQuerySanctions::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast(Sanctions);
	else
		OnFailure.Broadcast(FString::Printf(TEXT("QuerySanctions failed for '%s'"), *TargetUserId));

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

	// The subsystem broadcasts the BARE target Product User ID on every completion (empty
	// when the input had no parseable PUID) — extract ours the same way so completions can
	// be correlated exactly, including the pre-flight failure echoes.
	TargetPUID = UEEOSBlueprintLibrary::ExtractProductUserId(TargetUserId);

	Subsystem = Sub;
	Sub->OnPlayerReportSent.AddDynamic(this, &UEOSAsyncReportPlayer::HandleComplete);
	// SendPlayerReport is void (no in-flight rejection exists — concurrent reports against
	// different targets run in parallel and are told apart by the target id). Every pre-flight
	// failure broadcasts synchronously, carrying our target PUID, so the handler always
	// completes this node.
	Sub->SendPlayerReport(TargetUserId, Reason, Message);
}

void UEOSAsyncReportPlayer::HandleComplete(bool bSuccess, const FString& CompletedTargetUserId)
{
	// Correlate by bare target PUID: a completion for a DIFFERENT target is not ours — stay
	// bound (without unbinding) and wait for our own. An empty broadcast id matches only when
	// our own target had no parseable PUID (that pre-flight failure echo is ours).
	if (CompletedTargetUserId != TargetPUID) return;

	bCompleted = true;
	auto* Sub = Subsystem.Get();
	if (Sub) Sub->OnPlayerReportSent.RemoveDynamic(this, &UEOSAsyncReportPlayer::HandleComplete);

	if (bSuccess)
		OnSuccess.Broadcast();
	else
		OnFailure.Broadcast(FString::Printf(TEXT("ReportPlayer failed for '%s'"), *TargetUserId));

	SetReadyToDestroy();
}
