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

// NOTE on the uniform ExecuteAction pattern (see the header's "Completion pattern" block):
// handlers are bound BEFORE the subsystem call so a synchronous pre-flight failure
// broadcast still reaches this node. The call runs inside CallWithContext so the subsystem
// tags its completion broadcast as this node's; handlers return early unless
// IsResultForThisCall(). When the call returns false AND no handler fired (the node is not
// finished), the subsystem will never broadcast for this call — the node unbinds and fires
// OnFailure exactly once. A missing subsystem fails the node through
// RequireGameInstanceSubsystem; cancellation and timeouts call CancelWork, then fail the node.

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Stats — Query
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncQueryStats* UEOSAsyncQueryStats::QueryStats(UObject* WorldContext, const TArray<FString>& StatNames)
{
	UEOSAsyncQueryStats* Action = NewObject<UEOSAsyncQueryStats>();
	Action->StatNames = StatNames;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncQueryStats::ExecuteAction()
{
	UEEOSStatsSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSStatsSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	Sub->OnStatsQueried.AddDynamic(this, &UEOSAsyncQueryStats::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->QueryLocalStats(StatNames); });
	if (!bStarted && !IsFinished())
	{
		CancelWork();
		FailWithError(TEXT("QueryLocalStats could not be started"));
	}
}

void UEOSAsyncQueryStats::CancelWork()
{
	if (UEEOSStatsSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnStatsQueried.RemoveDynamic(this, &UEOSAsyncQueryStats::HandleComplete);
	}
}

void UEOSAsyncQueryStats::HandleComplete(bool bSuccess, const TArray<FEEOSStat>& Stats)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;

	if (bSuccess)
		OnSuccess.Broadcast(Stats);
	else
		OnFailure.Broadcast(TEXT("QueryLocalStats failed"));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Stats — Ingest
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncIngestStat* UEOSAsyncIngestStat::IngestStat(UObject* WorldContext, const FString& StatName, int32 Amount)
{
	UEOSAsyncIngestStat* Action = NewObject<UEOSAsyncIngestStat>();
	Action->StatName = StatName;
	Action->Amount = Amount;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncIngestStat::ExecuteAction()
{
	UEEOSStatsSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSStatsSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	Sub->OnStatIngested.AddDynamic(this, &UEOSAsyncIngestStat::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->IngestStat(StatName, Amount); });
	if (!bStarted && !IsFinished())
	{
		CancelWork();
		FailWithError(FString::Printf(TEXT("IngestStat '%s' could not be started"), *StatName));
	}
}

void UEOSAsyncIngestStat::CancelWork()
{
	if (UEEOSStatsSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnStatIngested.RemoveDynamic(this, &UEOSAsyncIngestStat::HandleComplete);
	}
}

void UEOSAsyncIngestStat::HandleComplete(bool bSuccess)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;

	if (bSuccess)
		OnSuccess.Broadcast();
	else
		OnFailure.Broadcast(TEXT("IngestStat failed"));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Achievements — Query
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncQueryAchievements* UEOSAsyncQueryAchievements::QueryAchievements(UObject* WorldContext)
{
	UEOSAsyncQueryAchievements* Action = NewObject<UEOSAsyncQueryAchievements>();
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncQueryAchievements::ExecuteAction()
{
	UEEOSAchievementSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSAchievementSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	Sub->OnAchievementsQueried.AddDynamic(this, &UEOSAsyncQueryAchievements::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->QueryAchievements(); });
	if (!bStarted && !IsFinished())
	{
		CancelWork();
		FailWithError(TEXT("QueryAchievements could not be started"));
	}
}

void UEOSAsyncQueryAchievements::CancelWork()
{
	if (UEEOSAchievementSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnAchievementsQueried.RemoveDynamic(this, &UEOSAsyncQueryAchievements::HandleComplete);
	}
}

void UEOSAsyncQueryAchievements::HandleComplete(bool bSuccess, const TArray<FEEOSAchievement>& Achievements)
{
	// No request key in the delegate payload: the subsystem tags its completion with the call
	// context of the call that started it, so a concurrent query's result is skipped here.
	// (Untagged broadcasts are still accepted — first-completion semantics for those.)
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;

	if (bSuccess)
		OnSuccess.Broadcast(Achievements);
	else
		OnFailure.Broadcast(TEXT("QueryAchievements failed"));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Achievements — Unlock
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncUnlockAchievement* UEOSAsyncUnlockAchievement::UnlockAchievement(UObject* WorldContext, const FString& AchievementId)
{
	UEOSAsyncUnlockAchievement* Action = NewObject<UEOSAsyncUnlockAchievement>();
	Action->AchievementId = AchievementId;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncUnlockAchievement::ExecuteAction()
{
	UEEOSAchievementSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSAchievementSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	Sub->OnAchievementUnlocked.AddDynamic(this, &UEOSAsyncUnlockAchievement::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->UnlockAchievement(AchievementId); });
	if (!bStarted && !IsFinished())
	{
		CancelWork();
		FailWithError(FString::Printf(TEXT("UnlockAchievement '%s' could not be started"), *AchievementId));
	}
}

void UEOSAsyncUnlockAchievement::CancelWork()
{
	if (UEEOSAchievementSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnAchievementUnlocked.RemoveDynamic(this, &UEOSAsyncUnlockAchievement::HandleComplete);
	}
}

void UEOSAsyncUnlockAchievement::HandleComplete(bool bSuccess, const FString& Id)
{
	if (!IsResultForThisCall()) return;
	// Correlate by achievement ID: a completion for a different in-flight unlock is not ours —
	// stay bound and wait for our own.
	if (Id != AchievementId) return;

	CancelWork();
	if (!FinishAction()) return;

	if (bSuccess)
		OnSuccess.Broadcast(Id);
	else
		OnFailure.Broadcast(FString::Printf(TEXT("UnlockAchievement '%s' failed"), *Id));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Connect Login
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncConnectLogin* UEOSAsyncConnectLogin::ConnectLogin(UObject* WorldContext, EEOSConnectLoginType LoginType, const FString& Token, const FString& DisplayName)
{
	UEOSAsyncConnectLogin* Action = NewObject<UEOSAsyncConnectLogin>();
	Action->LoginType = LoginType;
	Action->Token = Token;
	Action->DisplayName = DisplayName;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncConnectLogin::ExecuteAction()
{
	UEEOSAuthSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSAuthSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	Sub->OnConnectLoginComplete.AddDynamic(this, &UEOSAsyncConnectLogin::HandleComplete);

	bool bStarted = false;
	CallWithContext([&]
	{
		bStarted = (LoginType == EEOSConnectLoginType::DeviceId)
			? Sub->ConnectLoginWithDeviceId(DisplayName)
			: Sub->ConnectLogin(LoginType, Token);
	});

	if (!bStarted && !IsFinished())
	{
		// false without a broadcast = rejected because a Connect login is already in flight
		// (other pre-flight failures broadcast OnConnectLoginComplete(false) synchronously
		// and were consumed by the handler above).
		CancelWork();
		FailWithError(TEXT("Connect login could not be started (a Connect login is already in flight)"));
	}
}

void UEOSAsyncConnectLogin::CancelWork()
{
	if (UEEOSAuthSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnConnectLoginComplete.RemoveDynamic(this, &UEOSAsyncConnectLogin::HandleComplete);
	}
}

void UEOSAsyncConnectLogin::HandleComplete(bool bSuccess, const FString& ProductUserId, const FString& Error)
{
	// No request key in the delegate payload: the subsystem tags the completion with the call
	// context of the login that started it, so a login another node started is skipped here.
	// Untagged broadcasts (a login started outside any node, e.g. auto-login) are still accepted.
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;

	if (bSuccess)
		OnSuccess.Broadcast(ProductUserId);
	else
		OnFailure.Broadcast(Error);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Player Storage — Read SaveGame
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncReadSaveGame* UEOSAsyncReadSaveGame::ReadSaveGame(UObject* WorldContext, const FString& FileName)
{
	UEOSAsyncReadSaveGame* Action = NewObject<UEOSAsyncReadSaveGame>();
	Action->FileName = FileName;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncReadSaveGame::ExecuteAction()
{
	UEEOSPlayerStorageSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSPlayerStorageSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	Sub->OnSaveGameRead.AddDynamic(this, &UEOSAsyncReadSaveGame::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->ReadSaveGame(FileName); });
	if (!bStarted && !IsFinished())
	{
		// false without a broadcast = same-file duplicate rejected log-only (the in-flight
		// operation's completion must not be misattributed to this call).
		CancelWork();
		FailWithError(FString::Printf(TEXT("ReadSaveGame '%s' could not be started (an operation on this file is already in flight)"), *FileName));
	}
}

void UEOSAsyncReadSaveGame::CancelWork()
{
	if (UEEOSPlayerStorageSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnSaveGameRead.RemoveDynamic(this, &UEOSAsyncReadSaveGame::HandleComplete);
	}
}

void UEOSAsyncReadSaveGame::HandleComplete(bool bSuccess, const FString& InFileName, USaveGame* SaveGame)
{
	if (!IsResultForThisCall()) return;
	// Correlate by file name: a completion for a different in-flight read is not ours —
	// stay bound and wait for our own.
	if (InFileName != FileName) return;

	CancelWork();
	if (!FinishAction()) return;

	if (bSuccess && SaveGame)
		OnSuccess.Broadcast(SaveGame);
	else
		OnFailure.Broadcast(FString::Printf(TEXT("ReadSaveGame '%s' failed"), *InFileName));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Player Storage — Write SaveGame
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncWriteSaveGame* UEOSAsyncWriteSaveGame::WriteSaveGame(UObject* WorldContext, const FString& FileName, USaveGame* SaveGameObject)
{
	UEOSAsyncWriteSaveGame* Action = NewObject<UEOSAsyncWriteSaveGame>();
	Action->FileName = FileName;
	Action->SaveGameObject = SaveGameObject;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncWriteSaveGame::ExecuteAction()
{
	UEEOSPlayerStorageSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSPlayerStorageSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	Sub->OnPlayerDataWritten.AddDynamic(this, &UEOSAsyncWriteSaveGame::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->WriteSaveGame(FileName, SaveGameObject); });
	if (!bStarted && !IsFinished())
	{
		// false without a broadcast = same-file duplicate rejected log-only (the in-flight
		// operation's completion must not be misattributed to this call).
		CancelWork();
		FailWithError(FString::Printf(TEXT("WriteSaveGame '%s' could not be started (an operation on this file is already in flight)"), *FileName));
	}
}

void UEOSAsyncWriteSaveGame::CancelWork()
{
	if (UEEOSPlayerStorageSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnPlayerDataWritten.RemoveDynamic(this, &UEOSAsyncWriteSaveGame::HandleComplete);
	}
}

void UEOSAsyncWriteSaveGame::HandleComplete(bool bSuccess, const FString& InFileName)
{
	if (!IsResultForThisCall()) return;
	// Correlate by file name: a completion for a different in-flight write is not ours —
	// stay bound and wait for our own.
	if (InFileName != FileName) return;

	CancelWork();
	if (!FinishAction()) return;

	if (bSuccess)
		OnSuccess.Broadcast();
	else
		OnFailure.Broadcast(FString::Printf(TEXT("WriteSaveGame '%s' failed"), *InFileName));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Ecom — Query Offers
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncQueryOffers* UEOSAsyncQueryOffers::QueryOffers(UObject* WorldContext)
{
	UEOSAsyncQueryOffers* Action = NewObject<UEOSAsyncQueryOffers>();
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncQueryOffers::ExecuteAction()
{
	UEEOSEcomSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSEcomSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	Sub->OnOffersQueried.AddDynamic(this, &UEOSAsyncQueryOffers::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->QueryOffers(); });
	if (!bStarted && !IsFinished())
	{
		CancelWork();
		FailWithError(TEXT("QueryOffers could not be started (an offers query is already in flight)"));
	}
}

void UEOSAsyncQueryOffers::CancelWork()
{
	if (UEEOSEcomSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnOffersQueried.RemoveDynamic(this, &UEOSAsyncQueryOffers::HandleComplete);
	}
}

void UEOSAsyncQueryOffers::HandleComplete(bool bSuccess, const TArray<FEEOSCatalogOffer>& Offers)
{
	// No request key in the delegate payload: the subsystem tags the completion with the call
	// context of the query that started it. Untagged broadcasts are still accepted.
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;

	if (bSuccess)
		OnSuccess.Broadcast(Offers);
	else
		OnFailure.Broadcast(TEXT("QueryOffers failed"));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Ecom — Checkout
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncCheckout* UEOSAsyncCheckout::Checkout(UObject* WorldContext, const FString& OfferId)
{
	UEOSAsyncCheckout* Action = NewObject<UEOSAsyncCheckout>();
	Action->OfferId = OfferId;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncCheckout::ExecuteAction()
{
	UEEOSEcomSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSEcomSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	Sub->OnCheckoutComplete.AddDynamic(this, &UEOSAsyncCheckout::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->Checkout(OfferId); });
	if (!bStarted && !IsFinished())
	{
		CancelWork();
		FailWithError(FString::Printf(TEXT("Checkout '%s' could not be started (a checkout is already in flight)"), *OfferId));
	}
}

void UEOSAsyncCheckout::CancelWork()
{
	if (UEEOSEcomSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnCheckoutComplete.RemoveDynamic(this, &UEOSAsyncCheckout::HandleComplete);
	}
}

void UEOSAsyncCheckout::HandleComplete(bool bSuccess, const FString& TransactionId)
{
	// No request key in the delegate payload (TransactionId is a result, not the requested
	// OfferId): the subsystem tags the completion with the call context of the checkout that
	// started it. Untagged broadcasts are still accepted.
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;

	if (bSuccess)
		OnSuccess.Broadcast(TransactionId);
	else
		OnFailure.Broadcast(TEXT("Checkout failed"));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Sessions — Create
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncCreateSession* UEOSAsyncCreateSession::CreateSession(UObject* WorldContext, int32 MaxPlayers, bool bIsLAN, bool bIsPresence, const FString& SessionName)
{
	UEOSAsyncCreateSession* Action = NewObject<UEOSAsyncCreateSession>();
	Action->MaxPlayers = MaxPlayers;
	Action->bIsLAN = bIsLAN;
	Action->bIsPresence = bIsPresence;
	Action->SessionName = SessionName;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncCreateSession::ExecuteAction()
{
	UEEOSSessionSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSSessionSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	IgnoredRequestId = Sub->GetActiveOperationOutcome(TEXT("CreateSession")).RequestId;
	Sub->OnSessionCreated.AddDynamic(this, &UEOSAsyncCreateSession::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->CreateSession(MaxPlayers, bIsLAN, bIsPresence, SessionName); });
	if (!bStarted && !IsFinished())
	{
		// false without a broadcast = rejected (a create/destroy is already in flight); the
		// pre-flight cases broadcast OnSessionCreated(false) synchronously and were consumed
		// by the handler above.
		CancelWork();
		if (!FinishAction()) return;
		FEEOSOperationOutcome Rejection = Sub->GetLastOperationRejection();
		if (Rejection.Operation != TEXT("CreateSession")) { Rejection = FEEOSOperationOutcome(); Rejection.Operation = TEXT("CreateSession"); Rejection.Code = EEOSOperationCode::NativeStartRejected; Rejection.Message = TEXT("Request could not be started."); }
		OnDetailedCompleted.Broadcast(Rejection);
		OnFailure.Broadcast(Rejection.Operation == TEXT("CreateSession") && !Rejection.Message.IsEmpty() ? Rejection.Message : TEXT("CreateSession could not be started."));
		return;
	}
	if (!IsFinished()) ExpectedRequestId = Sub->GetActiveOperationOutcome(TEXT("CreateSession")).RequestId;
}

void UEOSAsyncCreateSession::CancelWork()
{
	if (UEEOSSessionSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnSessionCreated.RemoveDynamic(this, &UEOSAsyncCreateSession::HandleComplete);
	}
}

void UEOSAsyncCreateSession::BroadcastFailure(const FEFResult& Result)
{
	OnDetailedCompleted.Broadcast(MakeFailureOutcome(TEXT("CreateSession"), Result));
	Super::BroadcastFailure(Result);
}

void UEOSAsyncCreateSession::HandleComplete(bool bSuccess, const FString& InSessionName)
{
	if (!IsResultForThisCall()) return;
	// Match the dispatch-scoped terminal request; ignore the operation active before ExecuteAction.
	UEEOSSessionSubsystem* Sub = Subsystem.Get();
	const FEEOSOperationOutcome Completion = Sub ? Sub->GetLastOperationOutcome(TEXT("CreateSession")) : FEEOSOperationOutcome();
	if (IsFinished() || !Completion.RequestId || Completion.RequestId == IgnoredRequestId
		|| (ExpectedRequestId && Completion.RequestId != ExpectedRequestId)) return;
	CancelWork();
	if (!FinishAction()) return;
	OnDetailedCompleted.Broadcast(Completion);

	if (bSuccess)
		OnSuccess.Broadcast(InSessionName);
	else
		OnFailure.Broadcast(Sub ? Sub->GetLastOperationOutcome(TEXT("CreateSession")).Message : TEXT("CreateSession failed"));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Sessions — Find
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncFindSessions* UEOSAsyncFindSessions::FindSessions(UObject* WorldContext, int32 MaxResults)
{
	UEOSAsyncFindSessions* Action = NewObject<UEOSAsyncFindSessions>();
	Action->MaxResults = MaxResults;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncFindSessions::ExecuteAction()
{
	UEEOSSessionSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSSessionSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	IgnoredRequestId = Sub->GetActiveOperationOutcome(TEXT("FindSessions")).RequestId;
	Sub->OnSessionsFound.AddDynamic(this, &UEOSAsyncFindSessions::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->FindSessions(MaxResults); });
	if (!bStarted && !IsFinished())
	{
		// false without a broadcast = the search was rejected (our own or a sibling
		// session/lobby search is already in flight). The OnFailure doc contract —
		// "fires when the search could not be started" — is exactly this path.
		CancelWork();
		if (!FinishAction()) return;
		FEEOSOperationOutcome Rejection = Sub->GetLastOperationRejection();
		if (Rejection.Operation != TEXT("FindSessions")) { Rejection = FEEOSOperationOutcome(); Rejection.Operation = TEXT("FindSessions"); Rejection.Code = EEOSOperationCode::NativeStartRejected; Rejection.Message = TEXT("Request could not be started."); }
		OnDetailedCompleted.Broadcast(Rejection);
		OnFailure.Broadcast(Rejection.Operation == TEXT("FindSessions") && !Rejection.Message.IsEmpty() ? Rejection.Message : TEXT("FindSessions could not be started."));
		return;
	}
	if (!IsFinished()) ExpectedRequestId = Sub->GetActiveOperationOutcome(TEXT("FindSessions")).RequestId;
}

void UEOSAsyncFindSessions::CancelWork()
{
	if (UEEOSSessionSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnSessionsFound.RemoveDynamic(this, &UEOSAsyncFindSessions::HandleComplete);
	}
}

void UEOSAsyncFindSessions::BroadcastFailure(const FEFResult& Result)
{
	OnDetailedCompleted.Broadcast(MakeFailureOutcome(TEXT("FindSessions"), Result));
	Super::BroadcastFailure(Result);
}

void UEOSAsyncFindSessions::HandleComplete(const TArray<FEEOSSessionSearchResult>& Results)
{
	if (!IsResultForThisCall()) return;
	// Match the dispatch-scoped terminal request; ignore the operation active before ExecuteAction.
	UEEOSSessionSubsystem* Sub = Subsystem.Get();
	const FEEOSOperationOutcome Completion = Sub ? Sub->GetLastOperationOutcome(TEXT("FindSessions")) : FEEOSOperationOutcome();
	if (IsFinished() || !Completion.RequestId || Completion.RequestId == IgnoredRequestId
		|| (ExpectedRequestId && Completion.RequestId != ExpectedRequestId)) return;
	CancelWork();
	if (!FinishAction()) return;
	OnDetailedCompleted.Broadcast(Completion);

	const FEEOSOperationOutcome Outcome = Sub ? Sub->GetLastOperationOutcome(TEXT("FindSessions")) : FEEOSOperationOutcome();
	if (Outcome.bSuccess) OnSuccess.Broadcast(Results);
	else OnFailure.Broadcast(Outcome.Message.IsEmpty() ? TEXT("Native search failed.") : Outcome.Message);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Sessions — Join
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncJoinSession* UEOSAsyncJoinSession::JoinSession(UObject* WorldContext, int32 SearchResultIndex, const FString& SessionName)
{
	UEOSAsyncJoinSession* Action = NewObject<UEOSAsyncJoinSession>();
	Action->SearchResultIndex = SearchResultIndex;
	Action->SessionName = SessionName;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncJoinSession::ExecuteAction()
{
	UEEOSSessionSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSSessionSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	IgnoredRequestId = Sub->GetActiveOperationOutcome(TEXT("JoinSession")).RequestId;
	Sub->OnSessionJoined.AddDynamic(this, &UEOSAsyncJoinSession::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->JoinSession(SearchResultIndex, SessionName); });
	if (!bStarted && !IsFinished())
	{
		CancelWork();
		if (!FinishAction()) return;
		FEEOSOperationOutcome Rejection = Sub->GetLastOperationRejection();
		if (Rejection.Operation != TEXT("JoinSession")) { Rejection = FEEOSOperationOutcome(); Rejection.Operation = TEXT("JoinSession"); Rejection.Code = EEOSOperationCode::NativeStartRejected; Rejection.Message = TEXT("Request could not be started."); }
		OnDetailedCompleted.Broadcast(Rejection);
		OnFailure.Broadcast(Rejection.Operation == TEXT("JoinSession") && !Rejection.Message.IsEmpty() ? Rejection.Message : TEXT("JoinSession could not be started."));
		return;
	}
	if (!IsFinished()) ExpectedRequestId = Sub->GetActiveOperationOutcome(TEXT("JoinSession")).RequestId;
}

void UEOSAsyncJoinSession::CancelWork()
{
	if (UEEOSSessionSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnSessionJoined.RemoveDynamic(this, &UEOSAsyncJoinSession::HandleComplete);
	}
}

void UEOSAsyncJoinSession::BroadcastFailure(const FEFResult& Result)
{
	OnDetailedCompleted.Broadcast(MakeFailureOutcome(TEXT("JoinSession"), Result));
	Super::BroadcastFailure(Result);
}

void UEOSAsyncJoinSession::HandleComplete(bool bSuccess, const FString& InSessionName)
{
	if (!IsResultForThisCall()) return;
	// Match the dispatch-scoped terminal request; ignore the operation active before ExecuteAction.
	UEEOSSessionSubsystem* Sub = Subsystem.Get();
	const FEEOSOperationOutcome Completion = Sub ? Sub->GetLastOperationOutcome(TEXT("JoinSession")) : FEEOSOperationOutcome();
	if (IsFinished() || !Completion.RequestId || Completion.RequestId == IgnoredRequestId
		|| (ExpectedRequestId && Completion.RequestId != ExpectedRequestId)) return;
	CancelWork();
	if (!FinishAction()) return;
	OnDetailedCompleted.Broadcast(Completion);

	if (bSuccess)
		OnSuccess.Broadcast(InSessionName);
	else
		OnFailure.Broadcast(Sub ? Sub->GetLastOperationOutcome(TEXT("JoinSession")).Message : TEXT("JoinSession failed"));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Lobbies — Create
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncCreateLobby* UEOSAsyncCreateLobby::CreateLobby(UObject* WorldContext, int32 MaxMembers, bool bIsPublic, bool bUseVoiceChat)
{
	UEOSAsyncCreateLobby* Action = NewObject<UEOSAsyncCreateLobby>();
	Action->MaxMembers = MaxMembers;
	Action->bIsPublic = bIsPublic;
	Action->bUseVoiceChat = bUseVoiceChat;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncCreateLobby::ExecuteAction()
{
	UEEOSLobbySubsystem* Sub = RequireGameInstanceSubsystem<UEEOSLobbySubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	IgnoredRequestId = Sub->GetActiveOperationOutcome(TEXT("CreateLobby")).RequestId;
	Sub->OnLobbyCreated.AddDynamic(this, &UEOSAsyncCreateLobby::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->CreateLobby(MaxMembers, bIsPublic, bUseVoiceChat); });
	if (!bStarted && !IsFinished())
	{
		CancelWork();
		if (!FinishAction()) return;
		FEEOSOperationOutcome Rejection = Sub->GetLastOperationRejection();
		if (Rejection.Operation != TEXT("CreateLobby")) { Rejection = FEEOSOperationOutcome(); Rejection.Operation = TEXT("CreateLobby"); Rejection.Code = EEOSOperationCode::NativeStartRejected; Rejection.Message = TEXT("Request could not be started."); }
		OnDetailedCompleted.Broadcast(Rejection);
		OnFailure.Broadcast(Rejection.Operation == TEXT("CreateLobby") && !Rejection.Message.IsEmpty() ? Rejection.Message : TEXT("CreateLobby could not be started."));
		return;
	}
	if (!IsFinished()) ExpectedRequestId = Sub->GetActiveOperationOutcome(TEXT("CreateLobby")).RequestId;
}

void UEOSAsyncCreateLobby::CancelWork()
{
	if (UEEOSLobbySubsystem* Sub = Subsystem.Get())
	{
		Sub->OnLobbyCreated.RemoveDynamic(this, &UEOSAsyncCreateLobby::HandleComplete);
	}
}

void UEOSAsyncCreateLobby::BroadcastFailure(const FEFResult& Result)
{
	OnDetailedCompleted.Broadcast(MakeFailureOutcome(TEXT("CreateLobby"), Result));
	Super::BroadcastFailure(Result);
}

void UEOSAsyncCreateLobby::HandleComplete(bool bSuccess, const FString& LobbyId)
{
	if (!IsResultForThisCall()) return;
	// Match the dispatch-scoped terminal request; ignore the operation active before ExecuteAction.
	UEEOSLobbySubsystem* Sub = Subsystem.Get();
	const FEEOSOperationOutcome Completion = Sub ? Sub->GetLastOperationOutcome(TEXT("CreateLobby")) : FEEOSOperationOutcome();
	if (IsFinished() || !Completion.RequestId || Completion.RequestId == IgnoredRequestId
		|| (ExpectedRequestId && Completion.RequestId != ExpectedRequestId)) return;
	CancelWork();
	if (!FinishAction()) return;
	OnDetailedCompleted.Broadcast(Completion);

	if (bSuccess)
		OnSuccess.Broadcast(LobbyId);
	else
		OnFailure.Broadcast(Sub ? Sub->GetLastOperationOutcome(TEXT("CreateLobby")).Message : TEXT("CreateLobby failed"));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Lobbies — Find
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncFindLobbies* UEOSAsyncFindLobbies::FindLobbies(UObject* WorldContext, int32 MaxResults)
{
	UEOSAsyncFindLobbies* Action = NewObject<UEOSAsyncFindLobbies>();
	Action->MaxResults = MaxResults;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncFindLobbies::ExecuteAction()
{
	UEEOSLobbySubsystem* Sub = RequireGameInstanceSubsystem<UEEOSLobbySubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	IgnoredRequestId = Sub->GetActiveOperationOutcome(TEXT("FindLobbies")).RequestId;
	Sub->OnLobbiesFound.AddDynamic(this, &UEOSAsyncFindLobbies::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->FindLobbies(MaxResults); });
	if (!bStarted && !IsFinished())
	{
		// false without a broadcast = the search was rejected (our own or a sibling
		// session/lobby search is already in flight). The OnFailure doc contract —
		// "fires when the search could not be started" — is exactly this path.
		CancelWork();
		if (!FinishAction()) return;
		FEEOSOperationOutcome Rejection = Sub->GetLastOperationRejection();
		if (Rejection.Operation != TEXT("FindLobbies")) { Rejection = FEEOSOperationOutcome(); Rejection.Operation = TEXT("FindLobbies"); Rejection.Code = EEOSOperationCode::NativeStartRejected; Rejection.Message = TEXT("Request could not be started."); }
		OnDetailedCompleted.Broadcast(Rejection);
		OnFailure.Broadcast(Rejection.Operation == TEXT("FindLobbies") && !Rejection.Message.IsEmpty() ? Rejection.Message : TEXT("FindLobbies could not be started."));
		return;
	}
	if (!IsFinished()) ExpectedRequestId = Sub->GetActiveOperationOutcome(TEXT("FindLobbies")).RequestId;
}

void UEOSAsyncFindLobbies::CancelWork()
{
	if (UEEOSLobbySubsystem* Sub = Subsystem.Get())
	{
		Sub->OnLobbiesFound.RemoveDynamic(this, &UEOSAsyncFindLobbies::HandleComplete);
	}
}

void UEOSAsyncFindLobbies::BroadcastFailure(const FEFResult& Result)
{
	OnDetailedCompleted.Broadcast(MakeFailureOutcome(TEXT("FindLobbies"), Result));
	Super::BroadcastFailure(Result);
}

void UEOSAsyncFindLobbies::HandleComplete(const TArray<FEEOSSessionSearchResult>& Results)
{
	if (!IsResultForThisCall()) return;
	// Match the dispatch-scoped terminal request; ignore the operation active before ExecuteAction.
	UEEOSLobbySubsystem* Sub = Subsystem.Get();
	const FEEOSOperationOutcome Completion = Sub ? Sub->GetLastOperationOutcome(TEXT("FindLobbies")) : FEEOSOperationOutcome();
	if (IsFinished() || !Completion.RequestId || Completion.RequestId == IgnoredRequestId
		|| (ExpectedRequestId && Completion.RequestId != ExpectedRequestId)) return;
	CancelWork();
	if (!FinishAction()) return;
	OnDetailedCompleted.Broadcast(Completion);

	const FEEOSOperationOutcome Outcome = Sub ? Sub->GetLastOperationOutcome(TEXT("FindLobbies")) : FEEOSOperationOutcome();
	if (Outcome.bSuccess) OnSuccess.Broadcast(Results);
	else OnFailure.Broadcast(Outcome.Message.IsEmpty() ? TEXT("Native search failed.") : Outcome.Message);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Lobbies — Join
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncJoinLobby* UEOSAsyncJoinLobby::JoinLobby(UObject* WorldContext, int32 SearchResultIndex)
{
	UEOSAsyncJoinLobby* Action = NewObject<UEOSAsyncJoinLobby>();
	Action->SearchResultIndex = SearchResultIndex;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncJoinLobby::ExecuteAction()
{
	UEEOSLobbySubsystem* Sub = RequireGameInstanceSubsystem<UEEOSLobbySubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	IgnoredRequestId = Sub->GetActiveOperationOutcome(TEXT("JoinLobby")).RequestId;
	Sub->OnLobbyJoined.AddDynamic(this, &UEOSAsyncJoinLobby::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->JoinLobby(SearchResultIndex); });
	if (!bStarted && !IsFinished())
	{
		CancelWork();
		if (!FinishAction()) return;
		FEEOSOperationOutcome Rejection = Sub->GetLastOperationRejection();
		if (Rejection.Operation != TEXT("JoinLobby")) { Rejection = FEEOSOperationOutcome(); Rejection.Operation = TEXT("JoinLobby"); Rejection.Code = EEOSOperationCode::NativeStartRejected; Rejection.Message = TEXT("Request could not be started."); }
		OnDetailedCompleted.Broadcast(Rejection);
		OnFailure.Broadcast(Rejection.Operation == TEXT("JoinLobby") && !Rejection.Message.IsEmpty() ? Rejection.Message : TEXT("JoinLobby could not be started."));
		return;
	}
	if (!IsFinished()) ExpectedRequestId = Sub->GetActiveOperationOutcome(TEXT("JoinLobby")).RequestId;
}

void UEOSAsyncJoinLobby::CancelWork()
{
	if (UEEOSLobbySubsystem* Sub = Subsystem.Get())
	{
		Sub->OnLobbyJoined.RemoveDynamic(this, &UEOSAsyncJoinLobby::HandleComplete);
	}
}

void UEOSAsyncJoinLobby::BroadcastFailure(const FEFResult& Result)
{
	OnDetailedCompleted.Broadcast(MakeFailureOutcome(TEXT("JoinLobby"), Result));
	Super::BroadcastFailure(Result);
}

void UEOSAsyncJoinLobby::HandleComplete(bool bSuccess, const FString& LobbyId)
{
	if (!IsResultForThisCall()) return;
	// Match the dispatch-scoped terminal request; ignore the operation active before ExecuteAction.
	UEEOSLobbySubsystem* Sub = Subsystem.Get();
	const FEEOSOperationOutcome Completion = Sub ? Sub->GetLastOperationOutcome(TEXT("JoinLobby")) : FEEOSOperationOutcome();
	if (IsFinished() || !Completion.RequestId || Completion.RequestId == IgnoredRequestId
		|| (ExpectedRequestId && Completion.RequestId != ExpectedRequestId)) return;
	CancelWork();
	if (!FinishAction()) return;
	OnDetailedCompleted.Broadcast(Completion);

	if (bSuccess)
		OnSuccess.Broadcast(LobbyId);
	else
		OnFailure.Broadcast(Sub ? Sub->GetLastOperationOutcome(TEXT("JoinLobby")).Message : TEXT("JoinLobby failed"));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Matchmaking — Start
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncStartMatchmaking* UEOSAsyncStartMatchmaking::StartMatchmaking(UObject* WorldContext, const FString& QueueName)
{
	UEOSAsyncStartMatchmaking* Action = NewObject<UEOSAsyncStartMatchmaking>();
	Action->QueueName = QueueName;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncStartMatchmaking::ExecuteAction()
{
	UEEOSMatchmakingSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSMatchmakingSubsystem>();
	if (!Sub) return;

	Subsystem = Sub; IgnoredCycleId = Sub->GetMatchmakingCycleId();
	Sub->OnMatchFound.AddDynamic(this, &UEOSAsyncStartMatchmaking::HandleMatchFound);
	Sub->OnMatchmakingComplete.AddDynamic(this, &UEOSAsyncStartMatchmaking::HandleComplete);
	// A cancelled cycle broadcasts ONLY OnMatchmakingCancelled — without this binding the
	// node would never complete on cancellation and its stale OnSuccess would fire into the
	// abandoned graph on a LATER cycle. Binding it restores the exactly-one-pin guarantee.
	Sub->OnMatchmakingCancelled.AddDynamic(this, &UEOSAsyncStartMatchmaking::HandleCancelled);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->StartMatchmaking(QueueName); });
	if (!bStarted && !IsFinished())
	{
		// false without a broadcast = rejected (a matchmaking cycle is already in flight);
		// the no-cycle pre-flight failures broadcast OnMatchmakingComplete(false)
		// synchronously and were consumed by the handler above.
		CancelWork();
		if (!FinishAction()) return;
		OnDetailedCompleted.Broadcast(Sub->GetLastOperationRejection());
		OnFailure.Broadcast(TEXT("StartMatchmaking could not be started (a matchmaking cycle is already in flight)"));
		return;
	}
	if (!IsFinished()) ExpectedCycleId = Sub->GetMatchmakingCycleId();
}

void UEOSAsyncStartMatchmaking::CancelWork()
{
	if (UEEOSMatchmakingSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnMatchFound.RemoveDynamic(this, &UEOSAsyncStartMatchmaking::HandleMatchFound);
		Sub->OnMatchmakingComplete.RemoveDynamic(this, &UEOSAsyncStartMatchmaking::HandleComplete);
		Sub->OnMatchmakingCancelled.RemoveDynamic(this, &UEOSAsyncStartMatchmaking::HandleCancelled);
	}
}

void UEOSAsyncStartMatchmaking::BroadcastFailure(const FEFResult& Result)
{
	OnDetailedCompleted.Broadcast(MakeFailureOutcome(TEXT("StartMatchmaking"), Result));
	Super::BroadcastFailure(Result);
}

void UEOSAsyncStartMatchmaking::HandleMatchFound(const FString& SessionId)
{
	if (!IsResultForThisCall()) return;
	UEEOSMatchmakingSubsystem* Sub = Subsystem.Get();
	const FEEOSOperationOutcome Outcome = Sub ? Sub->GetLastOperationOutcome(TEXT("MatchOffer")) : FEEOSOperationOutcome();
	const int64 Cycle = Outcome.ParentRequestId;
	if (IsFinished() || !Cycle || Cycle == IgnoredCycleId || (ExpectedCycleId && Cycle != ExpectedCycleId)) return;

	// The subsystem allows one cycle at a time, duplicate starts are log-only rejected (they
	// fail fast in ExecuteAction) and the cycle's events carry the call context of the node
	// that started it — only that node completes on this event.
	CancelWork();
	if (!FinishAction()) return;
	OnDetailedCompleted.Broadcast(Outcome);
	OnSuccess.Broadcast(SessionId);
}

void UEOSAsyncStartMatchmaking::HandleComplete(bool bSuccess, const FString& ErrorMessage)
{
	if (!IsResultForThisCall()) return;
	UEEOSMatchmakingSubsystem* Sub = Subsystem.Get();
	const FEEOSOperationOutcome Outcome = Sub ? Sub->GetLastOperationOutcome(TEXT("Matchmaking")) : FEEOSOperationOutcome();
	const int64 Cycle = Outcome.RequestId;
	if (IsFinished() || !Cycle || Cycle == IgnoredCycleId || (ExpectedCycleId && Cycle != ExpectedCycleId)) return;

	// See HandleMatchFound for the correlation note.
	CancelWork();
	if (!FinishAction()) return;
	OnDetailedCompleted.Broadcast(Outcome);

	// Every branch must fire exactly one pin. A successful completion reaching this handler
	// means matchmaking finished (e.g. match accepted and joined) without this node observing
	// OnMatchFound; this delegate does not carry a SessionId, so OnSuccess fires with an
	// empty one (documented on the OnSuccess pin).
	if (bSuccess)
		OnSuccess.Broadcast(FString());
	else
		OnFailure.Broadcast(ErrorMessage);
}

void UEOSAsyncStartMatchmaking::HandleCancelled()
{
	if (!IsResultForThisCall()) return;
	UEEOSMatchmakingSubsystem* Sub = Subsystem.Get();
	const FEEOSOperationOutcome Outcome = Sub ? Sub->GetLastOperationOutcome(TEXT("Matchmaking")) : FEEOSOperationOutcome();
	const int64 Cycle = Outcome.RequestId;
	if (IsFinished() || !Cycle || Cycle == IgnoredCycleId || (ExpectedCycleId && Cycle != ExpectedCycleId)) return;

	// CancelMatchmaking ends the cycle with ONLY this broadcast (no OnMatchmakingComplete) —
	// complete the node on its failure pin so exactly one pin always fires.
	CancelWork();
	if (!FinishAction()) return;
	OnDetailedCompleted.Broadcast(Outcome);
	OnFailure.Broadcast(TEXT("Matchmaking cancelled"));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Leaderboards — Query
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncQueryLeaderboard* UEOSAsyncQueryLeaderboard::QueryLeaderboard(UObject* WorldContext, const FString& LeaderboardId)
{
	UEOSAsyncQueryLeaderboard* Action = NewObject<UEOSAsyncQueryLeaderboard>();
	Action->LeaderboardId = LeaderboardId;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncQueryLeaderboard::ExecuteAction()
{
	UEEOSLeaderboardSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSLeaderboardSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	Sub->OnLeaderboardQueried.AddDynamic(this, &UEOSAsyncQueryLeaderboard::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->QueryLeaderboard(LeaderboardId); });
	if (!bStarted && !IsFinished())
	{
		// false without a broadcast = rejected (another leaderboard query is already in
		// flight — log-only); pre-flight failures broadcast and were consumed above.
		CancelWork();
		FailWithError(FString::Printf(TEXT("QueryLeaderboard '%s' could not be started (another leaderboard query is already in flight)"), *LeaderboardId));
	}
}

void UEOSAsyncQueryLeaderboard::CancelWork()
{
	if (UEEOSLeaderboardSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnLeaderboardQueried.RemoveDynamic(this, &UEOSAsyncQueryLeaderboard::HandleComplete);
	}
}

void UEOSAsyncQueryLeaderboard::HandleComplete(bool bSuccess, const TArray<FEEOSLeaderboardEntry>& Entries)
{
	// No request key in the delegate payload: the subsystem tags the completion with the call
	// context of the query that started it. Untagged broadcasts are still accepted.
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;

	if (bSuccess)
		OnSuccess.Broadcast(Entries);
	else
		OnFailure.Broadcast(TEXT("QueryLeaderboard failed"));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Leaderboards — Upload Score
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncUploadScore* UEOSAsyncUploadScore::UploadScore(UObject* WorldContext, const FString& LeaderboardId, int32 Score)
{
	UEOSAsyncUploadScore* Action = NewObject<UEOSAsyncUploadScore>();
	Action->LeaderboardId = LeaderboardId;
	Action->Score = Score;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncUploadScore::ExecuteAction()
{
	UEEOSLeaderboardSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSLeaderboardSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	Sub->OnScoreUploaded.AddDynamic(this, &UEOSAsyncUploadScore::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->UploadScore(LeaderboardId, Score); });
	if (!bStarted && !IsFinished())
	{
		CancelWork();
		FailWithError(FString::Printf(TEXT("UploadScore '%s' could not be started"), *LeaderboardId));
	}
}

void UEOSAsyncUploadScore::CancelWork()
{
	if (UEEOSLeaderboardSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnScoreUploaded.RemoveDynamic(this, &UEOSAsyncUploadScore::HandleComplete);
	}
}

void UEOSAsyncUploadScore::HandleComplete(bool bSuccess, const FString& InLeaderboardId)
{
	if (!IsResultForThisCall()) return;
	// Correlate by leaderboard ID: a completion for a different in-flight upload is not ours —
	// stay bound and wait for our own.
	if (InLeaderboardId != LeaderboardId) return;

	CancelWork();
	if (!FinishAction()) return;

	if (bSuccess)
		OnSuccess.Broadcast(InLeaderboardId);
	else
		OnFailure.Broadcast(TEXT("UploadScore failed"));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Friends — Read List
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncReadFriendsList* UEOSAsyncReadFriendsList::ReadFriendsList(UObject* WorldContext)
{
	UEOSAsyncReadFriendsList* Action = NewObject<UEOSAsyncReadFriendsList>();
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncReadFriendsList::ExecuteAction()
{
	UEEOSFriendsSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSFriendsSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	Sub->OnFriendsListReady.AddDynamic(this, &UEOSAsyncReadFriendsList::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->ReadFriendsList(); });
	if (!bStarted && !IsFinished())
	{
		CancelWork();
		FailWithError(TEXT("ReadFriendsList could not be started"));
	}
}

void UEOSAsyncReadFriendsList::CancelWork()
{
	if (UEEOSFriendsSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnFriendsListReady.RemoveDynamic(this, &UEOSAsyncReadFriendsList::HandleComplete);
	}
}

void UEOSAsyncReadFriendsList::HandleComplete(const TArray<FEEOSFriendInfo>& Friends)
{
	// No request key in the delegate payload: the subsystem tags the completion with the call
	// context of the read that started it. Untagged broadcasts are still accepted.
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;

	// The subsystem broadcasts on every completion path (empty list on failure), so a started
	// read always completes this node here.
	OnSuccess.Broadcast(Friends);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Chat — Join Channel
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncJoinChatChannel* UEOSAsyncJoinChatChannel::JoinChatChannel(UObject* WorldContext, const FString& ChannelName)
{
	UEOSAsyncJoinChatChannel* Action = NewObject<UEOSAsyncJoinChatChannel>();
	Action->ChannelName = ChannelName;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncJoinChatChannel::ExecuteAction()
{
	UEEOSChatSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSChatSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	Sub->OnChannelJoined.AddDynamic(this, &UEOSAsyncJoinChatChannel::HandleComplete);
	// NOTE: already-joined channels re-broadcast success synchronously during this call —
	// the handler (bound above) completes the node in that case and the return is true.
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->JoinChannel(ChannelName); });
	if (!bStarted && !IsFinished())
	{
		CancelWork();
		FailWithError(FString::Printf(TEXT("JoinChannel '%s' could not be started"), *ChannelName));
	}
}

void UEOSAsyncJoinChatChannel::CancelWork()
{
	if (UEEOSChatSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnChannelJoined.RemoveDynamic(this, &UEOSAsyncJoinChatChannel::HandleComplete);
	}
}

void UEOSAsyncJoinChatChannel::HandleComplete(bool bSuccess, const FString& InChannelName)
{
	if (!IsResultForThisCall()) return;
	// Correlate by channel name: a completion for a different in-flight join is not ours —
	// stay bound and wait for our own.
	if (InChannelName != ChannelName) return;

	CancelWork();
	if (!FinishAction()) return;

	if (bSuccess)
		OnSuccess.Broadcast(InChannelName);
	else
		OnFailure.Broadcast(FString::Printf(TEXT("Failed to join channel '%s'"), *InChannelName));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Chat — Send Message
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncSendChatMessage* UEOSAsyncSendChatMessage::SendChatMessage(UObject* WorldContext, const FString& ChannelName, const FString& Message)
{
	UEOSAsyncSendChatMessage* Action = NewObject<UEOSAsyncSendChatMessage>();
	Action->ChannelName = ChannelName;
	Action->Message = Message;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncSendChatMessage::ExecuteAction()
{
	UEEOSChatSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSChatSubsystem>();
	if (!Sub) return;

	Subsystem = Sub;
	Sub->OnMessageSent.AddDynamic(this, &UEOSAsyncSendChatMessage::HandleComplete);
	bool bStarted = false;
	CallWithContext([&] { bStarted = Sub->SendMessage(ChannelName, Message); });
	if (!bStarted && !IsFinished())
	{
		CancelWork();
		FailWithError(FString::Printf(TEXT("SendMessage to '%s' could not be started"), *ChannelName));
	}
}

void UEOSAsyncSendChatMessage::CancelWork()
{
	if (UEEOSChatSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnMessageSent.RemoveDynamic(this, &UEOSAsyncSendChatMessage::HandleComplete);
	}
}

void UEOSAsyncSendChatMessage::HandleComplete(bool bSuccess, const FString& InChannelName)
{
	if (!IsResultForThisCall()) return;
	// Correlate by channel name: a completion for a different channel is not ours — stay bound
	// and wait for our own. (SendMessage reports synchronously, inside this node's call.)
	if (InChannelName != ChannelName) return;

	CancelWork();
	if (!FinishAction()) return;

	if (bSuccess)
		OnSuccess.Broadcast(InChannelName);
	else
		OnFailure.Broadcast(FString::Printf(TEXT("Failed to send message to '%s'"), *InChannelName));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Sanctions — Query
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncQuerySanctions* UEOSAsyncQuerySanctions::QuerySanctions(UObject* WorldContext, const FString& TargetUserId)
{
	UEOSAsyncQuerySanctions* Action = NewObject<UEOSAsyncQuerySanctions>();
	Action->TargetUserId = TargetUserId;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncQuerySanctions::ExecuteAction()
{
	UEEOSSanctionsSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSSanctionsSubsystem>();
	if (!Sub) return;

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
	CallWithContext([&] { Sub->QueryActiveSanctions(TargetUserId); });
}

void UEOSAsyncQuerySanctions::CancelWork()
{
	if (UEEOSSanctionsSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnSanctionsQueried.RemoveDynamic(this, &UEOSAsyncQuerySanctions::HandleComplete);
	}
}

void UEOSAsyncQuerySanctions::HandleComplete(bool bSuccess, const FString& CompletedTargetUserId, const TArray<FEEOSSanction>& Sanctions)
{
	if (!IsResultForThisCall()) return;
	// Correlate by bare target PUID: a completion for a DIFFERENT target is not ours — stay
	// bound (without unbinding) and wait for our own. An empty broadcast id matches only when
	// our own target had no parseable PUID (that pre-flight failure echo is ours).
	if (CompletedTargetUserId != TargetPUID) return;

	CancelWork();
	if (!FinishAction()) return;

	if (bSuccess)
		OnSuccess.Broadcast(Sanctions);
	else
		OnFailure.Broadcast(FString::Printf(TEXT("QuerySanctions failed for '%s'"), *TargetUserId));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Sanctions — Report Player
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

UEOSAsyncReportPlayer* UEOSAsyncReportPlayer::ReportPlayer(UObject* WorldContext, const FString& TargetUserId, const FString& Reason, const FString& Message)
{
	UEOSAsyncReportPlayer* Action = NewObject<UEOSAsyncReportPlayer>();
	Action->TargetUserId = TargetUserId;
	Action->Reason = Reason;
	Action->Message = Message;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEOSAsyncReportPlayer::ExecuteAction()
{
	UEEOSSanctionsSubsystem* Sub = RequireGameInstanceSubsystem<UEEOSSanctionsSubsystem>();
	if (!Sub) return;

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
	CallWithContext([&] { Sub->SendPlayerReport(TargetUserId, Reason, Message); });
}

void UEOSAsyncReportPlayer::CancelWork()
{
	if (UEEOSSanctionsSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnPlayerReportSent.RemoveDynamic(this, &UEOSAsyncReportPlayer::HandleComplete);
	}
}

void UEOSAsyncReportPlayer::HandleComplete(bool bSuccess, const FString& CompletedTargetUserId)
{
	if (!IsResultForThisCall()) return;
	// Correlate by bare target PUID: a completion for a DIFFERENT target is not ours — stay
	// bound (without unbinding) and wait for our own. An empty broadcast id matches only when
	// our own target had no parseable PUID (that pre-flight failure echo is ours).
	if (CompletedTargetUserId != TargetPUID) return;

	CancelWork();
	if (!FinishAction()) return;

	if (bSuccess)
		OnSuccess.Broadcast();
	else
		OnFailure.Broadcast(FString::Printf(TEXT("ReportPlayer failed for '%s'"), *TargetUserId));
}
