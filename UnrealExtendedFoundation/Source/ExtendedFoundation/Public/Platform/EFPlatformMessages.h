// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "EFPlatformMessages.generated.h"

/**
 * Message bus channels the platform subsystem sends on (see UEFMessageSubsystem). Listen with
 * Partial Match on Extended.Platform to hear all of them.
 */
namespace EFPlatformMessages
{
	/** Payload FEFAchievementUnlockedMessage. */
	EXTENDEDFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AchievementUnlocked);

	/** Payload FEFScoreSubmittedMessage. */
	EXTENDEDFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ScoreSubmitted);
}

USTRUCT(BlueprintType)
struct EXTENDEDFOUNDATION_API FEFAchievementUnlockedMessage
{
	GENERATED_BODY()

	/** The game's achievement id. */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FName AchievementId;

	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FName Provider;
};

USTRUCT(BlueprintType)
struct EXTENDEDFOUNDATION_API FEFScoreSubmittedMessage
{
	GENERATED_BODY()

	/** The game's leaderboard id. */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FName LeaderboardId;

	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	int64 Score = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	FName Provider;
};
