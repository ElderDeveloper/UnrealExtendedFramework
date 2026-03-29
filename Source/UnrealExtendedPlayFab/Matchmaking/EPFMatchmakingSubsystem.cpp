// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFMatchmakingSubsystem.h"
#include "Auth/EPFAuthSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

void UEPFMatchmakingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFMatchmakingSubsystem::Deinitialize()
{
	StopPolling();
	Super::Deinitialize();
}


// ── Create Ticket ────────────────────────────────────────────────────────────

void UEPFMatchmakingSubsystem::CreateTicket(const FString& QueueName, const TMap<FString, FString>& Attributes, int32 GiveUpAfterSeconds)
{
	if (QueueName.IsEmpty()) { OnTicketCreated.Broadcast(FEPFResult::Failure(TEXT("QueueName cannot be empty")), TEXT("")); return; }

	UEPFAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UEPFAuthSubsystem>();
	if (!Auth || Auth->GetEntityId().IsEmpty())
	{
		UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFMatchmakingSubsystem::CreateTicket — Not logged in or no entity token"));
		OnTicketCreated.Broadcast(FEPFResult::Failure(TEXT("Not logged in or missing entity token")), TEXT(""));
		return;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("QueueName"), QueueName);
	Body->SetNumberField(TEXT("GiveUpAfterSeconds"), FMath::Clamp(GiveUpAfterSeconds, 10, 600));

	// Creator entity key
	TSharedPtr<FJsonObject> Creator = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> CreatorEntity = MakeShared<FJsonObject>();
	CreatorEntity->SetStringField(TEXT("Id"), Auth->GetEntityId());
	CreatorEntity->SetStringField(TEXT("Type"), Auth->GetEntityType());
	Creator->SetObjectField(TEXT("Entity"), CreatorEntity);

	// Attributes
	if (Attributes.Num() > 0)
	{
		TSharedPtr<FJsonObject> AttrObj = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> DataObj = MakeShared<FJsonObject>();
		for (const auto& Pair : Attributes)
		{
			DataObj->SetStringField(Pair.Key, Pair.Value);
		}
		AttrObj->SetObjectField(TEXT("DataObject"), DataObj);
		Creator->SetObjectField(TEXT("Attributes"), AttrObj);
	}

	TArray<TSharedPtr<FJsonValue>> MembersArr;
	MembersArr.Add(MakeShared<FJsonValueObject>(Creator));
	Body->SetArrayField(TEXT("Members"), MembersArr);

	SendPlayFabRequestDetailed(
		TEXT("/Match/CreateMatchmakingTicket"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			FString TicketId;
			if (Result.bSuccess && Response.IsValid())
			{
				TicketId = Response->GetStringField(TEXT("TicketId"));
				LastResult = FEPFMatchmakingResult();
				LastResult.TicketId = TicketId;
				LastResult.Status = EEPFMatchmakingStatus::WaitingForMatch;
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFMatchmaking — Ticket created: %s"), *TicketId);
			}
			OnTicketCreated.Broadcast(Result, TicketId);
		})
	);
}


// ── Get Ticket Status ────────────────────────────────────────────────────────

void UEPFMatchmakingSubsystem::GetTicketStatus(const FString& QueueName, const FString& TicketId)
{
	if (QueueName.IsEmpty() || TicketId.IsEmpty()) { OnTicketStatusReceived.Broadcast(FEPFResult::Failure(TEXT("QueueName and TicketId are required")), FEPFMatchmakingResult()); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("QueueName"), QueueName);
	Body->SetStringField(TEXT("TicketId"), TicketId);
	Body->SetBoolField(TEXT("EscapeObject"), true);

	SendPlayFabRequestDetailed(
		TEXT("/Match/GetMatchmakingTicket"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			FEPFMatchmakingResult MatchResult;
			if (Result.bSuccess && Response.IsValid())
			{
				MatchResult.TicketId = Response->GetStringField(TEXT("TicketId"));
				FString StatusStr = Response->GetStringField(TEXT("Status"));

				if (StatusStr == TEXT("Matched"))
				{
					MatchResult.Status = EEPFMatchmakingStatus::Matched;
					MatchResult.MatchId = Response->GetStringField(TEXT("MatchId"));

					// Parse connected players
					const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
					if (Response->TryGetArrayField(TEXT("Members"), Members) && Members)
					{
						for (const auto& MemberVal : *Members)
						{
							const TSharedPtr<FJsonObject>* MemberObj = nullptr;
							if (MemberVal->TryGetObject(MemberObj) && MemberObj)
							{
								const TSharedPtr<FJsonObject>* EntityObj = nullptr;
								if ((*MemberObj)->TryGetObjectField(TEXT("Entity"), EntityObj) && EntityObj)
								{
									MatchResult.MatchedPlayerIds.Add((*EntityObj)->GetStringField(TEXT("Id")));
								}
							}
						}
					}

					StopPolling();
					OnMatchFound.Broadcast(MatchResult);
				}
				else if (StatusStr == TEXT("Canceled"))
				{
					MatchResult.Status = EEPFMatchmakingStatus::Canceled;
					StopPolling();
				}
				else if (StatusStr == TEXT("WaitingForMatch") || StatusStr == TEXT("WaitingForPlayers") || StatusStr == TEXT("WaitingForServer"))
				{
					MatchResult.Status = EEPFMatchmakingStatus::WaitingForMatch;
				}
				else
				{
					MatchResult.Status = EEPFMatchmakingStatus::Failed;
					StopPolling();
				}

				LastResult = MatchResult;
			}
			OnTicketStatusReceived.Broadcast(Result, Result.bSuccess ? LastResult : FEPFMatchmakingResult());
		})

	);
}


// ── Cancel ───────────────────────────────────────────────────────────────────

void UEPFMatchmakingSubsystem::CancelTicket(const FString& QueueName, const FString& TicketId)
{
	if (QueueName.IsEmpty() || TicketId.IsEmpty()) { OnMatchmakingCanceled.Broadcast(FEPFResult::Failure(TEXT("QueueName and TicketId are required"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("QueueName"), QueueName);
	Body->SetStringField(TEXT("TicketId"), TicketId);

	SendPlayFabRequestDetailed(
		TEXT("/Match/CancelMatchmakingTicket"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) 			{
				StopPolling();
				LastResult.Status = EEPFMatchmakingStatus::Canceled;
			}
			OnMatchmakingCanceled.Broadcast(Result);
		})
	);
}

void UEPFMatchmakingSubsystem::CancelAllTickets(const FString& QueueName)
{
	if (QueueName.IsEmpty()) { OnMatchmakingCanceled.Broadcast(FEPFResult::Failure(TEXT("QueueName cannot be empty"))); return; }

	UEPFAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UEPFAuthSubsystem>();
	if (!Auth) { OnMatchmakingCanceled.Broadcast(FEPFResult::Failure(TEXT("Auth subsystem not available"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("QueueName"), QueueName);

	TSharedPtr<FJsonObject> EntityObj = MakeShared<FJsonObject>();
	EntityObj->SetStringField(TEXT("Id"), Auth->GetEntityId());
	EntityObj->SetStringField(TEXT("Type"), Auth->GetEntityType());
	Body->SetObjectField(TEXT("Entity"), EntityObj);

	SendPlayFabRequestDetailed(
		TEXT("/Match/CancelAllMatchmakingTicketsForPlayer"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) 			{
				StopPolling();
				LastResult = FEPFMatchmakingResult();
			}
			OnMatchmakingCanceled.Broadcast(Result);
		})
	);
}


// ── Polling ──────────────────────────────────────────────────────────────────

void UEPFMatchmakingSubsystem::StartPolling(const FString& QueueName, const FString& TicketId, float PollIntervalSeconds)
{
	StopPolling();
	PollingQueueName = QueueName;
	PollingTicketId = TicketId;

	if (UWorld* World = GetGameInstance()->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PollTimerHandle,
			this,
			&UEPFMatchmakingSubsystem::PollTick,
			FMath::Max(2.0f, PollIntervalSeconds),
			true,
			0.0f // immediate first tick
		);
		UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFMatchmaking — Polling started (every %.1fs)"), PollIntervalSeconds);
	}
}

void UEPFMatchmakingSubsystem::StopPolling()
{
	if (PollTimerHandle.IsValid())
	{
		if (UWorld* World = GetGameInstance()->GetWorld())
		{
			World->GetTimerManager().ClearTimer(PollTimerHandle);
		}
		PollTimerHandle.Invalidate();
		UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFMatchmaking — Polling stopped"));
	}
	PollingQueueName.Empty();
	PollingTicketId.Empty();
}

bool UEPFMatchmakingSubsystem::IsPolling() const
{
	return PollTimerHandle.IsValid();
}

FEPFMatchmakingResult UEPFMatchmakingSubsystem::GetLastResult() const
{
	return LastResult;
}

void UEPFMatchmakingSubsystem::PollTick()
{
	GetTicketStatus(PollingQueueName, PollingTicketId);
}
