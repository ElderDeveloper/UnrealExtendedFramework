// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFMatchmakingSubsystem.generated.h"


UENUM(BlueprintType)
enum class EEPFMatchmakingStatus : uint8
{
	/** Waiting in queue for a match */
	WaitingForMatch,
	/** A match has been found */
	Matched,
	/** Matchmaking was canceled */
	Canceled,
	/** Matchmaking timed out or failed */
	Failed
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFMatchmakingResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Matchmaking")
	FString TicketId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Matchmaking")
	EEPFMatchmakingStatus Status = EEPFMatchmakingStatus::WaitingForMatch;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Matchmaking")
	FString MatchId;

	/** Server connection string (IP:Port) if a match was found */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Matchmaking")
	FString ServerAddress;

	/**
	 * Entity IDs of players in the matched game.
	 * NOTE: These are PlayFab Entity IDs (type "title_player_account"), NOT PlayFab IDs.
	 * To get a displayable PlayFab ID for each member, call GetPlayerProfile with the Entity ID.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Matchmaking")
	TArray<FString> MatchedPlayerIds;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFTicketCreated, const FEPFResult&, Result, const FString&, TicketId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFTicketStatusReceived, const FEPFResult&, Result, const FEPFMatchmakingResult&, MatchmakingResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFMatchmakingCanceled, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFMatchFound, const FEPFMatchmakingResult&, Result);

/**
 * PlayFab Matchmaking subsystem — create tickets, poll status, cancel.
 * For a full dedicated-server matchmaking solution, prefer EOS Matchmaking.
 * This subsystem covers PlayFab's smart-match queue system.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFMatchmakingSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/**
	 * Create a matchmaking ticket and place the player in a queue.
	 * @param QueueName      The matchmaking queue (configured in PlayFab dashboard).
	 * @param Attributes     Key-value attributes for match criteria.
	 * @param GiveUpAfterSeconds  Timeout in seconds (default 120).
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Matchmaking")
	void CreateTicket(const FString& QueueName, const TMap<FString, FString>& Attributes, int32 GiveUpAfterSeconds = 120);

	/**
	 * Poll the status of a matchmaking ticket. Call periodically or use StartPolling.
	 * @param QueueName  The queue the ticket was created in.
	 * @param TicketId   The ticket ID from CreateTicket.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Matchmaking")
	void GetTicketStatus(const FString& QueueName, const FString& TicketId);

	/**
	 * Cancel a matchmaking ticket.
	 * @param QueueName  The queue the ticket was created in.
	 * @param TicketId   The ticket ID to cancel.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Matchmaking")
	void CancelTicket(const FString& QueueName, const FString& TicketId);

	/** Cancel ALL matchmaking tickets for the current player */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Matchmaking")
	void CancelAllTickets(const FString& QueueName);

	/**
	 * Start automatic polling for ticket status.
	 * @param QueueName         The queue.
	 * @param TicketId          The ticket to poll.
	 * @param PollIntervalSeconds  How often to check (default 6s).
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Matchmaking")
	void StartPolling(const FString& QueueName, const FString& TicketId, float PollIntervalSeconds = 6.0f);

	/** Stop automatic polling */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Matchmaking")
	void StopPolling();

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Check if currently polling */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Matchmaking")
	bool IsPolling() const;

	/** Get the last result */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Matchmaking")
	FEPFMatchmakingResult GetLastResult() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Matchmaking")
	FOnEPFTicketCreated OnTicketCreated;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Matchmaking")
	FOnEPFTicketStatusReceived OnTicketStatusReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Matchmaking")
	FOnEPFMatchmakingCanceled OnMatchmakingCanceled;

	/** Fires automatically when polling detects a match, so you don't need to check status yourself */
	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Matchmaking")
	FOnEPFMatchFound OnMatchFound;

private:

	FEPFMatchmakingResult LastResult;
	FTimerHandle PollTimerHandle;
	FString PollingQueueName;
	FString PollingTicketId;

	void PollTick();
};
