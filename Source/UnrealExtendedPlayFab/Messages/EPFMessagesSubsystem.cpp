// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFMessagesSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

const FString UEPFMessagesSubsystem::MessageKeyPrefix = TEXT("msg_");

void UEPFMessagesSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFMessagesSubsystem::Deinitialize()
{
	StopPolling();
	CachedMessages.Empty();
	Super::Deinitialize();
}


// ── Fetch Messages ───────────────────────────────────────────────────────────

void UEPFMessagesSubsystem::FetchMessages()
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetUserReadOnlyData"),
		Body,
		true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				TMap<FString, FString> RawData;
				const TSharedPtr<FJsonObject>* DataObj = nullptr;
				if (Response->TryGetObjectField(TEXT("Data"), DataObj) && DataObj)
				{
					for (const auto& Pair : (*DataObj)->Values)
					{
						const FString DataKey(Pair.Key.Len(), *Pair.Key);
						if (DataKey.StartsWith(MessageKeyPrefix))
						{
							const TSharedPtr<FJsonObject>* ValueObj = nullptr;
							if (Pair.Value->TryGetObject(ValueObj) && ValueObj)
							{
								RawData.Add(DataKey, (*ValueObj)->GetStringField(TEXT("Value")));
							}
						}
					}
				}
				ParseMessages(RawData);

				// Check for new messages
				int32 CurrentUnread = GetUnreadCount();
				if (CurrentUnread > PreviousUnreadCount)
				{
					OnNewMessagesAvailable.Broadcast(CurrentUnread);
				}
				PreviousUnreadCount = CurrentUnread;

				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFMessages — Fetched %d messages (%d unread)"), CachedMessages.Num(), CurrentUnread);
			}
			OnMessagesReceived.Broadcast(Result, CachedMessages);
		})
	);
}


// ── Parse Messages ───────────────────────────────────────────────────────────

void UEPFMessagesSubsystem::ParseMessages(const TMap<FString, FString>& RawData)
{
	CachedMessages.Empty();

	for (const auto& Pair : RawData)
	{
		// Each message is stored as JSON: {"subject":"...", "body":"...", "sender":"...", "timestamp":"...", "read":false}
		TSharedPtr<FJsonObject> MsgJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Pair.Value);
		if (FJsonSerializer::Deserialize(Reader, MsgJson) && MsgJson.IsValid())
		{
			FEPFPlayerMessage Msg;
			Msg.MessageId = FString(Pair.Key.Len(), *Pair.Key).RightChop(MessageKeyPrefix.Len()); // remove "msg_" prefix
			Msg.Subject = MsgJson->GetStringField(TEXT("subject"));
			Msg.Body = MsgJson->GetStringField(TEXT("body"));
			Msg.Sender = MsgJson->GetStringField(TEXT("sender"));
			Msg.Timestamp = MsgJson->GetStringField(TEXT("timestamp"));
			Msg.bRead = MsgJson->GetBoolField(TEXT("read"));
			CachedMessages.Add(Msg);
		}
	}

	// Sort by timestamp descending (newest first)
	CachedMessages.Sort([](const FEPFPlayerMessage& A, const FEPFPlayerMessage& B)
	{
		return A.Timestamp > B.Timestamp;
	});
}


// ── Mark As Read ─────────────────────────────────────────────────────────────

void UEPFMessagesSubsystem::MarkAsRead(const FString& MessageId)
{
	if (MessageId.IsEmpty()) { OnMessageMarkedRead.Broadcast(FEPFResult::Failure(TEXT("MessageId cannot be empty"))); return; }

	// Find and update cached message
	FEPFPlayerMessage* Found = CachedMessages.FindByPredicate([&](const FEPFPlayerMessage& M) { return M.MessageId == MessageId; });
	if (!Found) { OnMessageMarkedRead.Broadcast(FEPFResult::Failure(TEXT("Message not found"))); return; }

	// Build updated JSON with read=true
	Found->bRead = true;

	TSharedRef<FJsonObject> MsgJson = MakeShared<FJsonObject>();
	MsgJson->SetStringField(TEXT("subject"), Found->Subject);
	MsgJson->SetStringField(TEXT("body"), Found->Body);
	MsgJson->SetStringField(TEXT("sender"), Found->Sender);
	MsgJson->SetStringField(TEXT("timestamp"), Found->Timestamp);
	MsgJson->SetBoolField(TEXT("read"), true);

	FString MsgJsonStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MsgJsonStr, 0);
	FJsonSerializer::Serialize(MsgJson, Writer);

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> DataMap = MakeShared<FJsonObject>();
	DataMap->SetStringField(MessageKeyPrefix + MessageId, MsgJsonStr);
	Body->SetObjectField(TEXT("Data"), DataMap);

	SendPlayFabRequestDetailed(TEXT("/Client/UpdateUserData"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			OnMessageMarkedRead.Broadcast(Result);
		}));
}


// ── Delete Message ───────────────────────────────────────────────────────────

void UEPFMessagesSubsystem::DeleteMessage(const FString& MessageId)
{
	if (MessageId.IsEmpty()) { OnMessageMarkedRead.Broadcast(FEPFResult::Failure(TEXT("MessageId cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> KeysArr;
	KeysArr.Add(MakeShared<FJsonValueString>(MessageKeyPrefix + MessageId));
	Body->SetArrayField(TEXT("KeysToRemove"), KeysArr);

	SendPlayFabRequestDetailed(TEXT("/Client/UpdateUserData"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this, MessageId](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess)
			{
				CachedMessages.RemoveAll([&](const FEPFPlayerMessage& M) { return M.MessageId == MessageId; });
			}
			OnMessageMarkedRead.Broadcast(Result);
		}));
}


// ── Polling ──────────────────────────────────────────────────────────────────

void UEPFMessagesSubsystem::StartPolling(float IntervalSeconds)
{
	StopPolling();

	float Interval = FMath::Max(30.0f, IntervalSeconds);

	if (UWorld* World = GetGameInstance()->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PollTimerHandle,
			this,
			&UEPFMessagesSubsystem::PollTick,
			Interval,
			true,
			0.0f
		);
		UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFMessages — Polling started (every %.0fs)"), Interval);
	}
}

void UEPFMessagesSubsystem::StopPolling()
{
	if (PollTimerHandle.IsValid())
	{
		if (UWorld* World = GetGameInstance()->GetWorld())
		{
			World->GetTimerManager().ClearTimer(PollTimerHandle);
		}
		PollTimerHandle.Invalidate();
	}
}

bool UEPFMessagesSubsystem::IsPolling() const
{
	return PollTimerHandle.IsValid();
}

void UEPFMessagesSubsystem::PollTick()
{
	FetchMessages();
}


// ── Queries ──────────────────────────────────────────────────────────────────

TArray<FEPFPlayerMessage> UEPFMessagesSubsystem::GetCachedMessages() const
{
	return CachedMessages;
}

int32 UEPFMessagesSubsystem::GetUnreadCount() const
{
	int32 Count = 0;
	for (const auto& M : CachedMessages) { if (!M.bRead) Count++; }
	return Count;
}
