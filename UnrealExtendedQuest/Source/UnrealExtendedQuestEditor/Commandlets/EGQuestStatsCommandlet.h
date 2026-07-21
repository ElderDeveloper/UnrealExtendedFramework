// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Commandlets/Commandlet.h"

#include "EGQuestStatsCommandlet.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogQuestStatsCommandlet, All, All);


class UEGQuestGraph;
class UEGQuestNode;


struct FEGQuestStatsQuest
{
public:
	int32 WordCount = 0;

	FEGQuestStatsQuest& operator+=(const FEGQuestStatsQuest& Other)
	{
		WordCount += Other.WordCount;
		return *this;
	}

};


UCLASS()
class UEGQuestStatsCommandlet: public UCommandlet
{
	GENERATED_BODY()

public:
	UEGQuestStatsCommandlet();

public:

	//~ UCommandlet interface
	int32 Main(const FString& Params) override;

	bool GetStatsForQuest(const UEGQuestGraph& Quest, FEGQuestStatsQuest& OutStats);
	int32 GetNodeWordCount(const UEGQuestNode& Node) const;

	int32 GetStringWordCount(const FString& String) const;
	int32 GetFNameWordCount(const FName Name) const { return GetStringWordCount(Name.ToString()); }
	int32 GetTextWordCount(const FText& Text) const { return GetStringWordCount(Text.ToString()); }
};
