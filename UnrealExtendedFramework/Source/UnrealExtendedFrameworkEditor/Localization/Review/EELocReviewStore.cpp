// Copyright Moon Punch Games. All Rights Reserved.

#include "EELocReviewStore.h"

#if WITH_EDITOR

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString FEELocReviewStore::ReviewStateToString(const EEELocReviewState State)
{
	switch (State)
	{
	case EEELocReviewState::Draft: return TEXT("Draft");
	case EEELocReviewState::MachineDraft: return TEXT("MachineDraft");
	case EEELocReviewState::HumanReviewed: return TEXT("HumanReviewed");
	case EEELocReviewState::Locked: return TEXT("Locked");
	case EEELocReviewState::NeedsContext: return TEXT("NeedsContext");
	case EEELocReviewState::NeedsNativeRewrite: return TEXT("NeedsNativeRewrite");
	default: return TEXT("None");
	}
}

EEELocReviewState FEELocReviewStore::ReviewStateFromString(const FString& Text)
{
	if (Text == TEXT("Draft")) { return EEELocReviewState::Draft; }
	if (Text == TEXT("MachineDraft")) { return EEELocReviewState::MachineDraft; }
	if (Text == TEXT("HumanReviewed")) { return EEELocReviewState::HumanReviewed; }
	if (Text == TEXT("Locked")) { return EEELocReviewState::Locked; }
	if (Text == TEXT("NeedsContext")) { return EEELocReviewState::NeedsContext; }
	if (Text == TEXT("NeedsNativeRewrite")) { return EEELocReviewState::NeedsNativeRewrite; }
	return EEELocReviewState::None;
}

FString FEELocReviewStore::GetFilePath() const
{
	return FPaths::ProjectContentDir() / TEXT("Localization") / TargetName / (TargetName + TEXT(".review.json"));
}

bool FEELocReviewStore::Load(const FString& InTargetName)
{
	TargetName = InTargetName;
	Records.Reset();

	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *GetFilePath()))
	{
		// No sidecar yet is a valid state.
		return true;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* Entries;
	if (!Root->TryGetObjectField(TEXT("entries"), Entries))
	{
		return true;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Entries)->Values)
	{
		const TSharedPtr<FJsonObject> RecordObject = Pair.Value->AsObject();
		if (!RecordObject.IsValid())
		{
			continue;
		}

		FEELocReviewRecord Record;
		Record.State = ReviewStateFromString(RecordObject->GetStringField(TEXT("state")));
		RecordObject->TryGetStringField(TEXT("reviewer"), Record.Reviewer);
		RecordObject->TryGetStringField(TEXT("sourceAtReview"), Record.SourceTextAtReview);

		const TArray<TSharedPtr<FJsonValue>>* Comments;
		if (RecordObject->TryGetArrayField(TEXT("comments"), Comments))
		{
			for (const TSharedPtr<FJsonValue>& Comment : *Comments)
			{
				Record.Comments.Add(Comment->AsString());
			}
		}

		Records.Add(Pair.Key, MoveTemp(Record));
	}

	return true;
}

bool FEELocReviewStore::Save(FString& OutError) const
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	const TSharedRef<FJsonObject> Entries = MakeShared<FJsonObject>();

	for (const TPair<FString, FEELocReviewRecord>& Pair : Records)
	{
		if (Pair.Value.State == EEELocReviewState::None && Pair.Value.Comments.Num() == 0)
		{
			continue;
		}

		const TSharedRef<FJsonObject> RecordObject = MakeShared<FJsonObject>();
		RecordObject->SetStringField(TEXT("state"), ReviewStateToString(Pair.Value.State));
		if (!Pair.Value.Reviewer.IsEmpty())
		{
			RecordObject->SetStringField(TEXT("reviewer"), Pair.Value.Reviewer);
		}
		if (!Pair.Value.SourceTextAtReview.IsEmpty())
		{
			RecordObject->SetStringField(TEXT("sourceAtReview"), Pair.Value.SourceTextAtReview);
		}
		if (Pair.Value.Comments.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Comments;
			for (const FString& Comment : Pair.Value.Comments)
			{
				Comments.Add(MakeShared<FJsonValueString>(Comment));
			}
			RecordObject->SetArrayField(TEXT("comments"), Comments);
		}

		Entries->SetObjectField(Pair.Key, RecordObject);
	}

	Root->SetObjectField(TEXT("entries"), Entries);

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);

	if (!FFileHelper::SaveStringToFile(Json, *GetFilePath()))
	{
		OutError = FString::Printf(TEXT("Failed to write %s"), *GetFilePath());
		return false;
	}
	return true;
}

FEELocReviewRecord FEELocReviewStore::GetRecord(const FString& Namespace, const FString& Key, const FString& Culture) const
{
	const FEELocReviewRecord* Record = Records.Find(MakeEntryKey(Namespace, Key, Culture));
	return Record ? *Record : FEELocReviewRecord();
}

void FEELocReviewStore::SetState(const FString& Namespace, const FString& Key, const FString& Culture,
	const EEELocReviewState State, const FString& Reviewer, const FString& SourceTextAtReview)
{
	FEELocReviewRecord& Record = Records.FindOrAdd(MakeEntryKey(Namespace, Key, Culture));
	Record.State = State;
	Record.Reviewer = Reviewer;
	Record.SourceTextAtReview = SourceTextAtReview;
}

void FEELocReviewStore::AddComment(const FString& Namespace, const FString& Key, const FString& Culture, const FString& Comment)
{
	Records.FindOrAdd(MakeEntryKey(Namespace, Key, Culture)).Comments.Add(Comment);
}

#endif // WITH_EDITOR
