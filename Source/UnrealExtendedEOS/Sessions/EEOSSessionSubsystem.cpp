// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSSessionSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "UnrealExtendedEOS.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UEEOSSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Listen for session invite acceptance
	if (IsEOSAvailable())
	{
		if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
		{
			IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
			if (SessionInterface.IsValid())
			{
				SessionInterface->OnSessionUserInviteAcceptedDelegates.AddUObject(this, &UEEOSSessionSubsystem::HandleSessionInviteAccepted);
			}
		}
	}
}

void UEEOSSessionSubsystem::Deinitialize()
{
	if (IsEOSAvailable())
	{
		if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
		{
			IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
			if (SessionInterface.IsValid())
			{
				SessionInterface->OnSessionUserInviteAcceptedDelegates.RemoveAll(this);
			}
		}
	}

	CachedSearchResults.Empty();
	SessionSearch.Reset();
	Super::Deinitialize();
}

void UEEOSSessionSubsystem::CreateSession(int32 MaxPlayers, bool bIsLAN, bool bIsPresence, const FString& SessionName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("CreateSession"));
		OnSessionCreated.Broadcast(false, SessionName);
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSessionSubsystem::CreateSession — Session interface not available"));
		OnSessionCreated.Broadcast(false, SessionName);
		return;
	}

	SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UEEOSSessionSubsystem::HandleCreateSessionComplete);

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = MaxPlayers;
	Settings.bIsLANMatch = bIsLAN;
	Settings.bShouldAdvertise = true;
	Settings.bUsesPresence = bIsPresence;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowJoinViaPresence = bIsPresence;

	SessionInterface->CreateSession(0, FName(*SessionName), Settings);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::CreateSession — Creating session '%s' with %d max players"), *SessionName, MaxPlayers);
}

void UEEOSSessionSubsystem::CreateSessionAdvanced(const FEEOSSessionSettings& Settings, const FString& SessionName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("CreateSessionAdvanced"));
		OnSessionCreated.Broadcast(false, SessionName);
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSessionSubsystem::CreateSessionAdvanced — Session interface not available"));
		OnSessionCreated.Broadcast(false, SessionName);
		return;
	}

	// Destroy existing session first if one exists, then create in the callback
	auto ExistingSession = SessionInterface->GetNamedSession(FName(*SessionName));
	if (ExistingSession != nullptr)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::CreateSessionAdvanced — Session '%s' already exists, destroying first"), *SessionName);

		// Build session settings now so we can use them in the callback
		FOnlineSessionSettings SessionSettings;
		SessionSettings.NumPublicConnections = Settings.MaxPlayers;
		SessionSettings.NumPrivateConnections = Settings.NumPrivateConnections;
		SessionSettings.bIsDedicated = Settings.bIsDedicatedServer;
		SessionSettings.bIsLANMatch = Settings.bIsLANMatch;
		SessionSettings.bShouldAdvertise = Settings.bShouldAdvertise;
		SessionSettings.bAllowJoinInProgress = Settings.bAllowJoinInProgress;
		SessionSettings.bUsesPresence = Settings.bUsesPresence;
		SessionSettings.bAllowJoinViaPresence = Settings.bAllowJoinViaPresence;
		SessionSettings.bAllowJoinViaPresenceFriendsOnly = Settings.bAllowJoinViaPresenceFriendsOnly;
		SessionSettings.bAllowInvites = Settings.bAllowInvites;
		SessionSettings.bUseLobbiesIfAvailable = Settings.bUseLobbiesIfAvailable;
		SessionSettings.bUseLobbiesVoiceChatIfAvailable = Settings.bUseLobbiesVoiceChatIfAvailable;
		SessionSettings.bUsesStats = Settings.bUsesStats;

		for (const auto& Pair : Settings.CustomSettings)
		{
			SessionSettings.Set(FName(*Pair.Key), Pair.Value, EOnlineDataAdvertisementType::ViaOnlineService);
		}

		// Chain: destroy → create in the completion callback
		FDelegateHandle DestroyHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateLambda(
				[this, SessionInterface, SessionName, SessionSettings](FName DestroyedName, bool bDestroySuccess)
				{
					if (bDestroySuccess)
					{
						UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::CreateSessionAdvanced — Old session destroyed, now creating '%s'"), *SessionName);
						SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UEEOSSessionSubsystem::HandleCreateSessionComplete);
						SessionInterface->CreateSession(0, FName(*SessionName), SessionSettings);
					}
					else
					{
						UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSessionSubsystem::CreateSessionAdvanced — Failed to destroy existing session '%s'"), *SessionName);
						OnSessionCreated.Broadcast(false, SessionName);
					}

					// Clean up this one-shot destroy delegate
					SessionInterface->OnDestroySessionCompleteDelegates.RemoveAll(this);
				}));

		SessionInterface->DestroySession(FName(*SessionName));
		return;
	}

	SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UEEOSSessionSubsystem::HandleCreateSessionComplete);

	FOnlineSessionSettings SessionSettings;
	SessionSettings.NumPublicConnections = Settings.MaxPlayers;
	SessionSettings.NumPrivateConnections = Settings.NumPrivateConnections;
	SessionSettings.bIsDedicated = Settings.bIsDedicatedServer;
	SessionSettings.bIsLANMatch = Settings.bIsLANMatch;
	SessionSettings.bShouldAdvertise = Settings.bShouldAdvertise;
	SessionSettings.bAllowJoinInProgress = Settings.bAllowJoinInProgress;
	SessionSettings.bUsesPresence = Settings.bUsesPresence;
	SessionSettings.bAllowJoinViaPresence = Settings.bAllowJoinViaPresence;
	SessionSettings.bAllowJoinViaPresenceFriendsOnly = Settings.bAllowJoinViaPresenceFriendsOnly;
	SessionSettings.bAllowInvites = Settings.bAllowInvites;
	SessionSettings.bUseLobbiesIfAvailable = Settings.bUseLobbiesIfAvailable;
	SessionSettings.bUseLobbiesVoiceChatIfAvailable = Settings.bUseLobbiesVoiceChatIfAvailable;
	SessionSettings.bUsesStats = Settings.bUsesStats;

	// Apply custom attributes (advertised for search filtering)
	for (const auto& Pair : Settings.CustomSettings)
	{
		SessionSettings.Set(FName(*Pair.Key), Pair.Value, EOnlineDataAdvertisementType::ViaOnlineService);
	}

	SessionInterface->CreateSession(0, FName(*SessionName), SessionSettings);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::CreateSessionAdvanced — Creating advanced session '%s' with %d public + %d private slots"),
		*SessionName, Settings.MaxPlayers, Settings.NumPrivateConnections);
}

void UEEOSSessionSubsystem::FindSessions(int32 MaxResults)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("FindSessions"));
		OnSessionsFound.Broadcast(TArray<FEEOSSessionSearchResult>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnSessionsFound.Broadcast(TArray<FEEOSSessionSearchResult>());
		return;
	}

	SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &UEEOSSessionSubsystem::HandleFindSessionsComplete);

	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->MaxSearchResults = MaxResults;
	SessionSearch->bIsLanQuery = false;
	SessionSearch->QuerySettings.Set(FName(TEXT("PRESENCESEARCH")), true, EOnlineComparisonOp::Equals);

	SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::FindSessions — Searching for sessions (max %d)..."), MaxResults);
}

void UEEOSSessionSubsystem::FindSessionsFiltered(int32 MaxResults, const TMap<FString, FString>& SearchFilters)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("FindSessionsFiltered"));
		OnSessionsFound.Broadcast(TArray<FEEOSSessionSearchResult>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnSessionsFound.Broadcast(TArray<FEEOSSessionSearchResult>());
		return;
	}

	SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &UEEOSSessionSubsystem::HandleFindSessionsComplete);

	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->MaxSearchResults = MaxResults;
	SessionSearch->bIsLanQuery = false;

	// Apply custom search filters
	for (const auto& Filter : SearchFilters)
	{
		SessionSearch->QuerySettings.Set(FName(*Filter.Key), Filter.Value, EOnlineComparisonOp::Equals);
	}

	SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::FindSessionsFiltered — Searching with %d filters (max %d results)..."), SearchFilters.Num(), MaxResults);
}

void UEEOSSessionSubsystem::JoinSession(int32 SearchResultIndex, const FString& SessionName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("JoinSession"));
		OnSessionJoined.Broadcast(false, SessionName);
		return;
	}

	if (!SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(SearchResultIndex))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::JoinSession — Invalid search result index: %d"), SearchResultIndex);
		OnSessionJoined.Broadcast(false, SessionName);
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnSessionJoined.Broadcast(false, SessionName);
		return;
	}

	SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UEEOSSessionSubsystem::HandleJoinSessionComplete);
	SessionInterface->JoinSession(0, FName(*SessionName), SessionSearch->SearchResults[SearchResultIndex]);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::JoinSession — Joining session at index %d..."), SearchResultIndex);
}

void UEEOSSessionSubsystem::DestroySession(const FString& SessionName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("DestroySession"));
		OnSessionDestroyed.Broadcast(false, SessionName);
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnSessionDestroyed.Broadcast(false, SessionName);
		return;
	}

	SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UEEOSSessionSubsystem::HandleDestroySessionComplete);
	SessionInterface->DestroySession(FName(*SessionName));
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::DestroySession — Destroying session '%s'..."), *SessionName);
}

void UEEOSSessionSubsystem::StartSession(const FString& SessionName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("StartSession"));
		OnSessionStarted.Broadcast(false);
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnSessionStarted.Broadcast(false);
		return;
	}

	SessionInterface->OnStartSessionCompleteDelegates.AddUObject(this, &UEEOSSessionSubsystem::HandleStartSessionComplete);
	SessionInterface->StartSession(FName(*SessionName));
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::StartSession — Starting session '%s'..."), *SessionName);
}

void UEEOSSessionSubsystem::EndSession(const FString& SessionName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("EndSession"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return;

	SessionInterface->EndSession(FName(*SessionName));
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::EndSession — Ending session '%s'"), *SessionName);
}

void UEEOSSessionSubsystem::RegisterPlayer(const FString& SessionName, const FString& PlayerId, bool bWasInvited)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("RegisterPlayer"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return;

	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid()) return;

	FUniqueNetIdPtr UniqueId = IdentityInterface->CreateUniquePlayerId(PlayerId);
	if (UniqueId.IsValid())
	{
		SessionInterface->RegisterPlayer(FName(*SessionName), *UniqueId, bWasInvited);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::RegisterPlayer — Registered '%s' in session '%s'"), *PlayerId, *SessionName);
	}
}

void UEEOSSessionSubsystem::UnRegisterPlayer(const FString& SessionName, const FString& PlayerId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("UnRegisterPlayer"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return;

	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid()) return;

	FUniqueNetIdPtr UniqueId = IdentityInterface->CreateUniquePlayerId(PlayerId);
	if (UniqueId.IsValid())
	{
		SessionInterface->UnregisterPlayer(FName(*SessionName), *UniqueId);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::UnRegisterPlayer — Unregistered '%s' from session '%s'"), *PlayerId, *SessionName);
	}
}

void UEEOSSessionSubsystem::ServerTravel(const UObject* WorldContextObject, const FString& MapPath)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSessionSubsystem::ServerTravel — Invalid world context"));
		return;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (World)
	{
		const FString TravelURL = MapPath + TEXT("?listen");
		World->ServerTravel(TravelURL);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::ServerTravel — Travelling to '%s'"), *TravelURL);
	}
}

void UEEOSSessionSubsystem::ClientTravel(const UObject* WorldContextObject, const FString& SessionName)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSessionSubsystem::ClientTravel — Invalid world context"));
		return;
	}

	FString ConnectionInfo = GetResolvedConnectString(SessionName);
	if (ConnectionInfo.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSessionSubsystem::ClientTravel — Could not resolve connection string for session '%s'"), *SessionName);
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (PC)
	{
		PC->ClientTravel(ConnectionInfo, TRAVEL_Absolute);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::ClientTravel — Travelling to '%s'"), *ConnectionInfo);
	}
}

FString UEEOSSessionSubsystem::GetResolvedConnectString(const FString& SessionName) const
{
	if (!IsEOSAvailable()) return FString();

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return FString();

	FString ConnectionInfo;
	SessionInterface->GetResolvedConnectString(FName(*SessionName), ConnectionInfo);
	return ConnectionInfo;
}

TArray<FEEOSSessionSearchResult> UEEOSSessionSubsystem::GetSearchResults() const
{
	return CachedSearchResults;
}

bool UEEOSSessionSubsystem::IsInSession() const
{
	return bInSession;
}

EEOSSessionState UEEOSSessionSubsystem::GetSessionState(const FString& SessionName) const
{
	if (!IsEOSAvailable()) return EEOSSessionState::NoSession;

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return EEOSSessionState::NoSession;

	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(FName(*SessionName));
	if (!Session) return EEOSSessionState::NoSession;

	switch (Session->SessionState)
	{
	case EOnlineSessionState::Creating:		return EEOSSessionState::Creating;
	case EOnlineSessionState::Pending:		return EEOSSessionState::Pending;
	case EOnlineSessionState::Starting:		return EEOSSessionState::Starting;
	case EOnlineSessionState::InProgress:	return EEOSSessionState::InProgress;
	case EOnlineSessionState::Ending:		return EEOSSessionState::Ending;
	case EOnlineSessionState::Ended:		return EEOSSessionState::Ended;
	case EOnlineSessionState::Destroying:	return EEOSSessionState::Destroying;
	default:								return EEOSSessionState::NoSession;
	}
}

FString UEEOSSessionSubsystem::GenerateSessionCode(int32 CodeLength)
{
	CodeLength = FMath::Clamp(CodeLength, 1, 20);
	const FString Chars = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
	FString Code;
	Code.Reserve(CodeLength);
	for (int32 i = 0; i < CodeLength; ++i)
	{
		Code.AppendChar(Chars[FMath::RandRange(0, Chars.Len() - 1)]);
	}
	return Code;
}

// ── Callbacks ────────────────────────────────────────────────────────────────

void UEEOSSessionSubsystem::HandleCreateSessionComplete(FName InSessionName, bool bWasSuccessful)
{
	bInSession = bWasSuccessful;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem: Create session '%s' %s"), *InSessionName.ToString(), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	OnSessionCreated.Broadcast(bWasSuccessful, InSessionName.ToString());

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		EOSSub->GetSessionInterface()->OnCreateSessionCompleteDelegates.RemoveAll(this);
	}
}

void UEEOSSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	CachedSearchResults.Empty();

	if (bWasSuccessful && SessionSearch.IsValid())
	{
		for (const auto& SearchResult : SessionSearch->SearchResults)
		{
			FEEOSSessionSearchResult Result;
			Result.SessionId = SearchResult.GetSessionIdStr();
			Result.CurrentPlayers = SearchResult.Session.SessionSettings.NumPublicConnections - SearchResult.Session.NumOpenPublicConnections;
			Result.MaxPlayers = SearchResult.Session.SessionSettings.NumPublicConnections;
			Result.Ping = SearchResult.PingInMs;
			Result.bIsDedicatedServer = SearchResult.Session.SessionSettings.bIsDedicated;
			CachedSearchResults.Add(Result);
		}
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem: Found %d sessions"), CachedSearchResults.Num());
	OnSessionsFound.Broadcast(CachedSearchResults);

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		EOSSub->GetSessionInterface()->OnFindSessionsCompleteDelegates.RemoveAll(this);
	}
}

void UEEOSSessionSubsystem::HandleJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result)
{
	const bool bSuccess = (Result == EOnJoinSessionCompleteResult::Success);
	bInSession = bSuccess;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem: Join session '%s' %s"), *InSessionName.ToString(), bSuccess ? TEXT("succeeded") : TEXT("failed"));
	OnSessionJoined.Broadcast(bSuccess, InSessionName.ToString());

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		EOSSub->GetSessionInterface()->OnJoinSessionCompleteDelegates.RemoveAll(this);
	}
}

void UEEOSSessionSubsystem::HandleDestroySessionComplete(FName InSessionName, bool bWasSuccessful)
{
	if (bWasSuccessful) bInSession = false;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem: Destroy session '%s' %s"), *InSessionName.ToString(), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	OnSessionDestroyed.Broadcast(bWasSuccessful, InSessionName.ToString());

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		EOSSub->GetSessionInterface()->OnDestroySessionCompleteDelegates.RemoveAll(this);
	}
}

void UEEOSSessionSubsystem::HandleStartSessionComplete(FName InSessionName, bool bWasSuccessful)
{
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem: Start session '%s' %s"), *InSessionName.ToString(), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	OnSessionStarted.Broadcast(bWasSuccessful);

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		EOSSub->GetSessionInterface()->OnStartSessionCompleteDelegates.RemoveAll(this);
	}
}

void UEEOSSessionSubsystem::HandleSessionInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	FString SessionId = InviteResult.GetSessionIdStr();
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem: Session invite accepted for session '%s' — %s"), *SessionId, bWasSuccessful ? TEXT("success") : TEXT("failed"));
	OnSessionInviteAccepted.Broadcast(bWasSuccessful, SessionId);
}
