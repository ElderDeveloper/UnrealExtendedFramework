// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "EEOSSessionSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSSessionCreated, bool, bSuccess, const FString&, SessionName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSSessionsFound, const TArray<FEEOSSessionSearchResult>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSSessionJoined, bool, bSuccess, const FString&, SessionName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSSessionDestroyed, bool, bSuccess, const FString&, SessionName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSSessionInviteAccepted, bool, bSuccess, const FString&, SessionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSSessionStarted, bool, bSuccess);

/**
 * Manages EOS game sessions — create, find, join, leave, and destroy.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSSessionSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Create a new game session (basic) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	void CreateSession(int32 MaxPlayers = 4, bool bIsLAN = false, bool bIsPresence = true, const FString& SessionName = TEXT("GameSession"));

	/** Create a session with full configuration */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	void CreateSessionAdvanced(const FEEOSSessionSettings& Settings, const FString& SessionName = TEXT("GameSession"));

	/** Search for available sessions */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	void FindSessions(int32 MaxResults = 20);

	/** Search for sessions with custom attribute filters */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	void FindSessionsFiltered(int32 MaxResults, const TMap<FString, FString>& SearchFilters);

	/** Join a session from search results by index */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	void JoinSession(int32 SearchResultIndex, const FString& SessionName = TEXT("GameSession"));

	/** Destroy the current session */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	void DestroySession(const FString& SessionName = TEXT("GameSession"));

	/** Start the session (marks it InProgress, disables join if not AllowJoinInProgress) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	void StartSession(const FString& SessionName = TEXT("GameSession"));

	/** End the session (marks it Ended) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	void EndSession(const FString& SessionName = TEXT("GameSession"));

	/** Register a player into the session */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	void RegisterPlayer(const FString& SessionName, const FString& PlayerId, bool bWasInvited = false);

	/** Unregister a player from the session */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions")
	void UnRegisterPlayer(const FString& SessionName, const FString& PlayerId);

	// ── Travel Helpers ───────────────────────────────────────────────────────

	/** Trigger server travel to the given map (appends ?listen for listen server) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions", meta = (WorldContext = "WorldContextObject"))
	void ServerTravel(const UObject* WorldContextObject, const FString& MapPath);

	/** Resolve the connection string and travel the local player to the session host */
	UFUNCTION(BlueprintCallable, Category = "EOS|Sessions", meta = (WorldContext = "WorldContextObject"))
	void ClientTravel(const UObject* WorldContextObject, const FString& SessionName = TEXT("GameSession"));

	/** Get the resolved connection string for a session */
	UFUNCTION(BlueprintPure, Category = "EOS|Sessions")
	FString GetResolvedConnectString(const FString& SessionName = TEXT("GameSession")) const;

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the cached search results */
	UFUNCTION(BlueprintPure, Category = "EOS|Sessions")
	TArray<FEEOSSessionSearchResult> GetSearchResults() const;

	/** Check if currently in a session */
	UFUNCTION(BlueprintPure, Category = "EOS|Sessions")
	bool IsInSession() const;

	/** Get the current session state */
	UFUNCTION(BlueprintPure, Category = "EOS|Sessions")
	EEOSSessionState GetSessionState(const FString& SessionName = TEXT("GameSession")) const;

	/** Generate a random alphanumeric session code */
	UFUNCTION(BlueprintPure, Category = "EOS|Sessions")
	static FString GenerateSessionCode(int32 CodeLength = 6);

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|Sessions")
	FOnEOSSessionCreated OnSessionCreated;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Sessions")
	FOnEOSSessionsFound OnSessionsFound;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Sessions")
	FOnEOSSessionJoined OnSessionJoined;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Sessions")
	FOnEOSSessionDestroyed OnSessionDestroyed;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Sessions")
	FOnEOSSessionInviteAccepted OnSessionInviteAccepted;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Sessions")
	FOnEOSSessionStarted OnSessionStarted;

private:

	TArray<FEEOSSessionSearchResult> CachedSearchResults;
	TSharedPtr<class FOnlineSessionSearch> SessionSearch;
	bool bInSession = false;

	void HandleCreateSessionComplete(FName InSessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName InSessionName, bool bWasSuccessful);
	void HandleStartSessionComplete(FName InSessionName, bool bWasSuccessful);
	void HandleSessionInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
};
