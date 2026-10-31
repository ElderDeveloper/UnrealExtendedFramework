// Copyright Moon Punch Games. All Rights Reserved.

#include "EFAnalyticsSinks.h"

#include "Dom/JsonObject.h"
#include "ExtendedFoundationModule.h"
#include "HAL/FileManager.h"
#include "Http/EFHttpClient.h"
#include "Http/EFHttpTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Result/EFResult.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace EFAnalyticsSinksPrivate
{
	FString ToCondensedJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Text;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
		FJsonSerializer::Serialize(Object, Writer);
		return Text;
	}
}

// FEFAnalyticsEvent

TSharedRef<FJsonObject> FEFAnalyticsEvent::ToJson() const
{
	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("name"), Name);
	Json->SetStringField(TEXT("timestamp"), TimestampUtc.ToIso8601());
	Json->SetStringField(TEXT("sessionId"), SessionId);
	Json->SetStringField(TEXT("userId"), UserId);
	Json->SetNumberField(TEXT("sequence"), static_cast<double>(Sequence));

	const TSharedRef<FJsonObject> AttributesJson = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Attribute : Attributes)
	{
		AttributesJson->SetStringField(Attribute.Key, Attribute.Value);
	}
	Json->SetObjectField(TEXT("attributes"), AttributesJson);
	return Json;
}

bool FEFAnalyticsEvent::FromJson(const TSharedPtr<FJsonObject>& Json, FEFAnalyticsEvent& OutEvent)
{
	if (!Json.IsValid() || !Json->TryGetStringField(TEXT("name"), OutEvent.Name))
	{
		return false;
	}

	FString Timestamp;
	if (Json->TryGetStringField(TEXT("timestamp"), Timestamp))
	{
		FDateTime::ParseIso8601(*Timestamp, OutEvent.TimestampUtc);
	}
	Json->TryGetStringField(TEXT("sessionId"), OutEvent.SessionId);
	Json->TryGetStringField(TEXT("userId"), OutEvent.UserId);
	double SequenceNumber = 0.0;
	if (Json->TryGetNumberField(TEXT("sequence"), SequenceNumber))
	{
		OutEvent.Sequence = static_cast<int64>(SequenceNumber);
	}

	OutEvent.Attributes.Reset();
	const TSharedPtr<FJsonObject>* AttributesJson = nullptr;
	if (Json->TryGetObjectField(TEXT("attributes"), AttributesJson))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*AttributesJson)->Values)
		{
			FString Value;
			if (Pair.Value.IsValid() && Pair.Value->TryGetString(Value))
			{
				OutEvent.Attributes.Add(Pair.Key, Value);
			}
		}
	}
	return true;
}

// Log

void FEFAnalyticsLogSink::SendEvents(const TArray<FEFAnalyticsEvent>& Events)
{
	for (const FEFAnalyticsEvent& Event : Events)
	{
		TArray<FString> Pairs;
		for (const TPair<FString, FString>& Attribute : Event.Attributes)
		{
			Pairs.Add(FString::Printf(TEXT("%s=%s"), *Attribute.Key, *Attribute.Value));
		}
		UE_LOG(LogExtendedFoundation, Display, TEXT("Analytics #%lld %s {%s}"), Event.Sequence, *Event.Name, *FString::Join(Pairs, TEXT(", ")));
	}
}

// File

void FEFAnalyticsFileSink::SendEvents(const TArray<FEFAnalyticsEvent>& Events)
{
	FString Lines;
	for (const FEFAnalyticsEvent& Event : Events)
	{
		Lines += EFAnalyticsSinksPrivate::ToCondensedJson(Event.ToJson());
		Lines += TEXT("\n");
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), /*Tree*/ true);
	if (FFileHelper::SaveStringToFile(Lines, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append))
	{
		Written += Events.Num();
	}
	else
	{
		UE_LOG(LogExtendedFoundation, Warning, TEXT("Analytics File sink: could not write %s."), *FilePath);
	}
}

void FEFAnalyticsFileSink::DiscardPending()
{
	// Consent was withdrawn: the file holds this session's events, so it goes too.
	IFileManager::Get().Delete(*FilePath, /*RequireExists*/ false, /*EvenReadOnly*/ true, /*Quiet*/ true);
	Written = 0;
}

FString FEFAnalyticsFileSink::DescribeStatus() const
{
	return FString::Printf(TEXT("%d event(s) written to %s"), Written, *FilePath);
}

// Http

FString FEFAnalyticsHttpSink::GetDefaultPendingFolder()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExtendedAnalytics"), TEXT("Pending"));
}

TArray<FString> FEFAnalyticsHttpSink::FindPersistedFiles() const
{
	TArray<FString> Names;
	IFileManager::Get().FindFiles(Names, *FPaths::Combine(Config.PendingFolder, TEXT("*.json")), /*Files*/ true, /*Directories*/ false);

	TArray<FString> Paths;
	for (const FString& Name : Names)
	{
		Paths.Add(FPaths::Combine(Config.PendingFolder, Name));
	}
	// Oldest first.
	Paths.Sort([](const FString& A, const FString& B) { return IFileManager::Get().GetTimeStamp(*A) < IFileManager::Get().GetTimeStamp(*B); });
	return Paths;
}

void FEFAnalyticsHttpSink::ResendPersisted()
{
	if (bResentPersisted)
	{
		return;
	}
	bResentPersisted = true;

	if (!Config.bPersistUnsent || Config.Endpoint.IsEmpty())
	{
		return;
	}
	for (const FString& Path : FindPersistedFiles())
	{
		FString Body;
		if (FFileHelper::LoadFileToString(Body, *Path) && !Body.IsEmpty())
		{
			Post(Body, /*EventCount*/ 0, Path);
		}
		else
		{
			IFileManager::Get().Delete(*Path, false, true, true);
		}
	}
}

void FEFAnalyticsHttpSink::SendEvents(const TArray<FEFAnalyticsEvent>& Events)
{
	if (Events.Num() == 0 || Config.Endpoint.IsEmpty())
	{
		return;
	}

	// Events only arrive once they may be sent. With consent required and not known when the sink was
	// created, this is the first moment the batches earlier runs saved may go too.
	ResendPersisted();

	TArray<TSharedPtr<FJsonValue>> EventValues;
	for (const FEFAnalyticsEvent& Event : Events)
	{
		EventValues.Add(MakeShared<FJsonValueObject>(Event.ToJson()));
	}
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("events"), EventValues);

	Post(EFAnalyticsSinksPrivate::ToCondensedJson(Root), Events.Num(), FString());
}

void FEFAnalyticsHttpSink::Post(const FString& Body, int32 EventCount, const FString& PersistedFile)
{
	FEFHttpRequest Request;
	Request.Url = Config.Endpoint;
	Request.Verb = EEFHttpVerb::Post;
	Request.Headers = Config.Headers;
	Request.Body = Body;
	Request.ContentType = TEXT("application/json");
	Request.TimeoutSeconds = Config.TimeoutSeconds;
	Request.MaxRetries = Config.MaxRetries;
	// Events carry sessionId + sequence, so a batch delivered twice can be deduplicated by the back end.
	Request.bAllowRetryForNonIdempotentVerbs = true;
	Request.bLogFailuresAsWarnings = false;

	const int64 Id = ++LastId;
	FInFlight& Entry = InFlight.AddDefaulted_GetRef();
	Entry.Id = Id;
	Entry.Body = Body;
	Entry.EventCount = EventCount;
	Entry.PersistedFile = PersistedFile;

	TWeakPtr<FEFAnalyticsHttpSink> WeakThis = AsShared();
	const TSharedRef<FEFHttpOperation> Operation = FEFHttpOperation::Start(Request, [WeakThis, Id](const FEFHttpResponse& Response)
	{
		if (const TSharedPtr<FEFAnalyticsHttpSink> This = WeakThis.Pin())
		{
			This->HandleResponse(Id, Response);
		}
	});

	// Start() never answers from inside itself, so the entry is still here.
	if (FInFlight* Added = InFlight.FindByPredicate([Id](const FInFlight& Candidate) { return Candidate.Id == Id; }))
	{
		Added->Operation = Operation;
	}
}

void FEFAnalyticsHttpSink::HandleResponse(int64 Id, const FEFHttpResponse& Response)
{
	const int32 Index = InFlight.IndexOfByPredicate([Id](const FInFlight& Candidate) { return Candidate.Id == Id; });
	if (Index == INDEX_NONE)
	{
		return;  // cancelled by Flush or DiscardPending
	}
	FInFlight Entry = MoveTemp(InFlight[Index]);
	InFlight.RemoveAt(Index);

	if (Response.IsSuccess())
	{
		SentEvents += Entry.EventCount;
		if (!Entry.PersistedFile.IsEmpty())
		{
			IFileManager::Get().Delete(*Entry.PersistedFile, false, true, true);
		}
		return;
	}

	++FailedBatches;
	const FEFResult Result = FEFResult::FromHttp(Response, TEXT("Analytics"));
	if (Result.bRetryable)
	{
		UE_LOG(LogExtendedFoundation, Log, TEXT("Analytics Http sink: a batch could not be sent (%s); %s"), *Result.ToString(),
			Config.bPersistUnsent ? TEXT("it is saved for the next run.") : TEXT("it is dropped."));
		if (Entry.PersistedFile.IsEmpty())
		{
			Persist(Entry.Body);
		}
	}
	else
	{
		// 4xx: the endpoint rejects this batch and always will; keeping it would only repeat the error.
		UE_LOG(LogExtendedFoundation, Warning, TEXT("Analytics Http sink: the endpoint rejected a batch (%s); it is dropped."), *Result.ToString());
		if (!Entry.PersistedFile.IsEmpty())
		{
			IFileManager::Get().Delete(*Entry.PersistedFile, false, true, true);
		}
	}
}

void FEFAnalyticsHttpSink::Persist(const FString& Body)
{
	if (!Config.bPersistUnsent || Config.MaxPersistedBatches <= 0)
	{
		return;
	}
	IFileManager::Get().MakeDirectory(*Config.PendingFolder, /*Tree*/ true);
	const FString Path = FPaths::Combine(Config.PendingFolder, FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".json"));
	if (!FFileHelper::SaveStringToFile(Body, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogExtendedFoundation, Warning, TEXT("Analytics Http sink: could not save an unsent batch to %s."), *Path);
	}
	TrimPersisted();
}

void FEFAnalyticsHttpSink::TrimPersisted()
{
	TArray<FString> Files = FindPersistedFiles();
	for (int32 Index = 0; Index < Files.Num() - Config.MaxPersistedBatches; ++Index)
	{
		IFileManager::Get().Delete(*Files[Index], false, true, true);
	}
}

void FEFAnalyticsHttpSink::Flush(bool bShuttingDown)
{
	if (!bShuttingDown)
	{
		return;  // batches are sent as they arrive; nothing is buffered here
	}

	// The game is closing: requests still running may never finish. Save them for the next run.
	TArray<FInFlight> Running = MoveTemp(InFlight);
	InFlight.Reset();
	for (FInFlight& Entry : Running)
	{
		if (Entry.PersistedFile.IsEmpty())
		{
			Persist(Entry.Body);
		}
		if (Entry.Operation.IsValid())
		{
			Entry.Operation->Cancel(/*bNotify*/ false);
		}
	}
}

void FEFAnalyticsHttpSink::DiscardPending()
{
	TArray<FInFlight> Running = MoveTemp(InFlight);
	InFlight.Reset();
	for (FInFlight& Entry : Running)
	{
		if (Entry.Operation.IsValid())
		{
			Entry.Operation->Cancel(/*bNotify*/ false);
		}
	}
	for (const FString& Path : FindPersistedFiles())
	{
		IFileManager::Get().Delete(*Path, false, true, true);
	}
}

FString FEFAnalyticsHttpSink::DescribeStatus() const
{
	return FString::Printf(TEXT("%d event(s) delivered, %d batch(es) in flight, %d failed, %d saved for later"),
		SentEvents, InFlight.Num(), FailedBatches, FindPersistedFiles().Num());
}
