// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFTitleNewsSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"

void UEPFTitleNewsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFTitleNewsSubsystem::Deinitialize()
{
	CachedNews.Empty();
	Super::Deinitialize();
}

void UEPFTitleNewsSubsystem::GetTitleNews(int32 Count)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("Count"), FMath::Clamp(Count, 1, 100));

	SendPlayFabRequestDetailed(
		TEXT("/Client/GetTitleNews"),
		Body,
		true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				CachedNews.Empty();
				const TArray<TSharedPtr<FJsonValue>>* NewsArray = nullptr;
				if (Response->TryGetArrayField(TEXT("News"), NewsArray) && NewsArray)
				{
					for (const auto& NewsValue : *NewsArray)
					{
						const TSharedPtr<FJsonObject>* NewsObj = nullptr;
						if (NewsValue->TryGetObject(NewsObj) && NewsObj)
						{
							FEPFTitleNewsItem Item;
							Item.NewsId = (*NewsObj)->GetStringField(TEXT("NewsId"));
							Item.Title = (*NewsObj)->GetStringField(TEXT("Title"));
							Item.Body = (*NewsObj)->GetStringField(TEXT("Body"));

							FString TimestampStr = (*NewsObj)->GetStringField(TEXT("Timestamp"));
							FDateTime::ParseIso8601(*TimestampStr, Item.Timestamp);

							CachedNews.Add(Item);
						}
					}
				}
				UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFTitleNewsSubsystem — Received %d news items"), CachedNews.Num());
				OnTitleNewsReceived.Broadcast(Result, CachedNews);
			}
			else
			{
				UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFTitleNewsSubsystem — Failed to fetch title news"));
				OnTitleNewsReceived.Broadcast(Result, CachedNews);
			}
		})
	);
}

TArray<FEPFTitleNewsItem> UEPFTitleNewsSubsystem::GetCachedNews() const
{
	return CachedNews;
}

int32 UEPFTitleNewsSubsystem::GetNewsCount() const
{
	return CachedNews.Num();
}

bool UEPFTitleNewsSubsystem::HasNews() const
{
	return CachedNews.Num() > 0;
}
