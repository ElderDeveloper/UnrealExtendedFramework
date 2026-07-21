// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFSegmentsSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

void UEPFSegmentsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFSegmentsSubsystem::Deinitialize()
{
	CachedSegments.Empty();
	CachedTags.Empty();
	Super::Deinitialize();
}


// ── Get Player Segments ──────────────────────────────────────────────────────

void UEPFSegmentsSubsystem::GetPlayerSegments()
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetPlayerSegments"),
		Body,
		true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				CachedSegments.Empty();
				const TArray<TSharedPtr<FJsonValue>>* SegmentsArr = nullptr;
				if (Response->TryGetArrayField(TEXT("Segments"), SegmentsArr) && SegmentsArr)
				{
					for (const auto& SegVal : *SegmentsArr)
					{
						const TSharedPtr<FJsonObject>* SegObj = nullptr;
						if (SegVal->TryGetObject(SegObj) && SegObj)
						{
							FEPFPlayerSegment Seg;
							Seg.SegmentId = (*SegObj)->GetStringField(TEXT("Id"));
							Seg.SegmentName = (*SegObj)->GetStringField(TEXT("Name"));
							CachedSegments.Add(Seg);
						}
					}
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFSegments — Found %d segments"), CachedSegments.Num());
			}
			OnSegmentsReceived.Broadcast(Result, CachedSegments);
		})
	);
}


// ── Get Player Tags ──────────────────────────────────────────────────────────

void UEPFSegmentsSubsystem::GetPlayerTags(const FString& Namespace)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("PlayFabId"), GetPlayFabId());
	if (!Namespace.IsEmpty())
	{
		Body->SetStringField(TEXT("Namespace"), Namespace);
	}

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetPlayerTags"),
		Body,
		true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				CachedTags.Empty();
				const TArray<TSharedPtr<FJsonValue>>* TagsArr = nullptr;
				if (Response->TryGetArrayField(TEXT("Tags"), TagsArr) && TagsArr)
				{
					for (const auto& TagVal : *TagsArr)
					{
						FString TagStr;
						if (TagVal->TryGetString(TagStr))
						{
							CachedTags.Add(TagStr);
						}
					}
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFSegments — Found %d tags"), CachedTags.Num());
			}
			OnPlayerTagsReceived.Broadcast(Result, CachedTags);
		})
	);
}


// ── Add / Remove Tags ────────────────────────────────────────────────────────

void UEPFSegmentsSubsystem::AddPlayerTag(const FString& TagName, const FString& Namespace)
{
	if (TagName.IsEmpty()) { OnPlayerTagModified.Broadcast(FEPFResult::Failure(TEXT("TagName cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("PlayFabId"), GetPlayFabId());
	Body->SetStringField(TEXT("TagName"), TagName);
	if (!Namespace.IsEmpty()) Body->SetStringField(TEXT("Namespace"), Namespace);

	SendPlayFabRequestDetailed(TEXT("/Client/AddPlayerTag"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this, TagName](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess && !CachedTags.Contains(TagName)) CachedTags.Add(TagName);
			OnPlayerTagModified.Broadcast(Result);
		}));
}

void UEPFSegmentsSubsystem::RemovePlayerTag(const FString& TagName, const FString& Namespace)
{
	if (TagName.IsEmpty()) { OnPlayerTagModified.Broadcast(FEPFResult::Failure(TEXT("TagName cannot be empty"))); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("PlayFabId"), GetPlayFabId());
	Body->SetStringField(TEXT("TagName"), TagName);
	if (!Namespace.IsEmpty()) Body->SetStringField(TEXT("Namespace"), Namespace);

	SendPlayFabRequestDetailed(TEXT("/Client/RemovePlayerTag"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this, TagName](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) CachedTags.Remove(TagName);
			OnPlayerTagModified.Broadcast(Result);
		}));
}


// ── Queries ──────────────────────────────────────────────────────────────────

bool UEPFSegmentsSubsystem::IsInSegment(const FString& SegmentName) const
{
	for (const auto& S : CachedSegments)
	{
		if (S.SegmentName.Equals(SegmentName, ESearchCase::IgnoreCase)) return true;
	}
	return false;
}

bool UEPFSegmentsSubsystem::IsInSegmentById(const FString& SegmentId) const
{
	for (const auto& S : CachedSegments)
	{
		if (S.SegmentId == SegmentId) return true;
	}
	return false;
}

TArray<FEPFPlayerSegment> UEPFSegmentsSubsystem::GetCachedSegments() const
{
	return CachedSegments;
}

bool UEPFSegmentsSubsystem::HasTag(const FString& TagName) const
{
	return CachedTags.Contains(TagName);
}

TArray<FString> UEPFSegmentsSubsystem::GetCachedTags() const
{
	return CachedTags;
}
