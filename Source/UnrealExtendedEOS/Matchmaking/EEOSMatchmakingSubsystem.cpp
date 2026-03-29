// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSMatchmakingSubsystem.h"
#include "UnrealExtendedEOS.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"

void UEEOSMatchmakingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSMatchmakingSubsystem::Deinitialize()
{
	if (bIsMatchmaking)
	{
		CancelMatchmaking();
	}
	bIsMatchmaking = false;
	CurrentQueueName.Empty();
	Super::Deinitialize();
}

void UEEOSMatchmakingSubsystem::StartMatchmaking(const FString& QueueName)
{
	StartMatchmakingWithAttributes(QueueName, TMap<FString, FString>());
}

void UEEOSMatchmakingSubsystem::StartMatchmakingWithAttributes(const FString& QueueName, const TMap<FString, FString>& Attributes)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("StartMatchmaking"));
		OnMatchmakingComplete.Broadcast(false, TEXT("EOS not available"));
		return;
	}

	if (bIsMatchmaking)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMatchmakingSubsystem::StartMatchmaking — Already matchmaking"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSMatchmakingSubsystem::StartMatchmaking — Session interface not available"));
		OnMatchmakingComplete.Broadcast(false, TEXT("Session interface not available"));
		return;
	}

	bIsMatchmaking = true;
	CurrentQueueName = QueueName;
	MatchmakingStartTime = FPlatformTime::Seconds();

	OnMatchmakingStatusChanged.Broadcast(FString::Printf(TEXT("Searching for match in queue '%s'..."), *QueueName));

	// EOS matchmaking uses session search with a "MATCHMAKINGPOOL" bucket attribute
	// Create a session search with the queue name as the bucket
	TSharedRef<FOnlineSessionSearch> SearchSettings = MakeShared<FOnlineSessionSearch>();
	SearchSettings->MaxSearchResults = 1;
	SearchSettings->bIsLanQuery = false;
	SearchSettings->TimeoutInSeconds = 60.0f;

	// Set the matchmaking bucket/pool (this is how EOS routes matchmaking)
	SearchSettings->QuerySettings.Set(TEXT("MATCHMAKINGPOOL"), QueueName, EOnlineComparisonOp::Equals);

	// Add custom attributes for filtering
	for (const auto& Attr : Attributes)
	{
		SearchSettings->QuerySettings.Set(FName(*Attr.Key), Attr.Value, EOnlineComparisonOp::Equals);
		UE_LOG(LogExtendedEOS, Log, TEXT("  Matchmaking Attribute: '%s' = '%s'"), *Attr.Key, *Attr.Value);
	}

	// Store the search ref for later use (accept/reject)
	CachedSearchSettings = SearchSettings;

	// Register the completion delegate
	MatchmakingCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UEEOSMatchmakingSubsystem::HandleFindSessionsComplete));

	// Start the session search (this is the EOS matchmaking call)
	SessionInterface->FindSessions(0, SearchSettings);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::StartMatchmaking — Queue='%s', Attributes=%d — Searching..."), *QueueName, Attributes.Num());
}

void UEEOSMatchmakingSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	if (EOSSub)
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(MatchmakingCompleteDelegateHandle);
		}
	}

	if (!bIsMatchmaking)
	{
		// Was cancelled
		return;
	}

	if (bWasSuccessful && CachedSearchSettings.IsValid() && CachedSearchSettings->SearchResults.Num() > 0)
	{
		// Match found
		const FOnlineSessionSearchResult& Result = CachedSearchSettings->SearchResults[0];
		FString SessionId = Result.Session.GetSessionIdStr();

		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem: Match found — Session ID: %s"), *SessionId);
		OnMatchmakingStatusChanged.Broadcast(TEXT("Match found!"));
		OnMatchFound.Broadcast(SessionId);
	}
	else
	{
		// No match found or search failed
		bIsMatchmaking = false;
		CurrentQueueName.Empty();

		FString ErrorMsg = bWasSuccessful ? TEXT("No sessions found matching criteria") : TEXT("Session search failed");
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMatchmakingSubsystem: Matchmaking failed — %s"), *ErrorMsg);
		OnMatchmakingComplete.Broadcast(false, ErrorMsg);
	}
}

void UEEOSMatchmakingSubsystem::CancelMatchmaking()
{
	if (!bIsMatchmaking)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMatchmakingSubsystem::CancelMatchmaking — Not currently matchmaking"));
		return;
	}

	const float ElapsedTime = GetMatchmakingElapsedTime();

	// Cancel the ongoing session search if possible
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	if (EOSSub)
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(MatchmakingCompleteDelegateHandle);
			SessionInterface->CancelFindSessions();
		}
	}

	bIsMatchmaking = false;
	CurrentQueueName.Empty();
	CachedSearchSettings.Reset();

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::CancelMatchmaking — Cancelled after %.1f seconds"), ElapsedTime);
	OnMatchmakingCancelled.Broadcast();
}

void UEEOSMatchmakingSubsystem::AcceptMatch()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("AcceptMatch"));
		return;
	}

	if (!CachedSearchSettings.IsValid() || CachedSearchSettings->SearchResults.Num() == 0)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMatchmakingSubsystem::AcceptMatch — No match available to accept"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSMatchmakingSubsystem::AcceptMatch — Session interface not available"));
		return;
	}

	// Join the found session
	const FOnlineSessionSearchResult& Result = CachedSearchSettings->SearchResults[0];
	FString SessionId = Result.Session.GetSessionIdStr();

	// Clear any previous join delegate
	if (JoinSessionDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
	}

	JoinSessionDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateLambda(
			[this, SessionId, SessionInterface](FName InSessionName, EOnJoinSessionCompleteResult::Type JoinResult)
			{
				// Clean up this delegate immediately
				if (JoinSessionDelegateHandle.IsValid())
				{
					SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
					JoinSessionDelegateHandle.Reset();
				}

				const bool bJoinSuccess = (JoinResult == EOnJoinSessionCompleteResult::Success);

				bIsMatchmaking = false;
				CurrentQueueName.Empty();
				CachedSearchSettings.Reset();

				if (bJoinSuccess)
				{
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::AcceptMatch — Joined session '%s' successfully"), *SessionId);
					OnMatchmakingStatusChanged.Broadcast(TEXT("Match accepted, joined session"));
					OnMatchmakingComplete.Broadcast(true, TEXT(""));
				}
				else
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSMatchmakingSubsystem::AcceptMatch — Failed to join session '%s'"), *SessionId);
					OnMatchmakingComplete.Broadcast(false, TEXT("Failed to join session"));
				}
			}));

	SessionInterface->JoinSession(0, TEXT("GameSession"), Result);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::AcceptMatch — Joining session '%s'..."), *SessionId);
	OnMatchmakingStatusChanged.Broadcast(TEXT("Match accepted, joining session..."));
}

void UEEOSMatchmakingSubsystem::RejectMatch()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("RejectMatch"));
		return;
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSMatchmakingSubsystem::RejectMatch — Rejecting match, re-queuing"));

	// Clear the found match and re-queue with the same settings
	FString PreviousQueue = CurrentQueueName;
	CachedSearchSettings.Reset();
	bIsMatchmaking = false;

	if (!PreviousQueue.IsEmpty())
	{
		OnMatchmakingStatusChanged.Broadcast(TEXT("Match rejected, searching again..."));
		StartMatchmaking(PreviousQueue);
	}
	else
	{
		OnMatchmakingStatusChanged.Broadcast(TEXT("Match rejected"));
		OnMatchmakingComplete.Broadcast(false, TEXT("Match rejected by user"));
	}
}

bool UEEOSMatchmakingSubsystem::IsMatchmaking() const
{
	return bIsMatchmaking;
}

FString UEEOSMatchmakingSubsystem::GetCurrentQueueName() const
{
	return CurrentQueueName;
}

float UEEOSMatchmakingSubsystem::GetMatchmakingElapsedTime() const
{
	if (!bIsMatchmaking) return 0.f;
	return static_cast<float>(FPlatformTime::Seconds() - MatchmakingStartTime);
}

float UEEOSMatchmakingSubsystem::GetEstimatedWaitTime() const
{
	return CachedEstimatedWait;
}
