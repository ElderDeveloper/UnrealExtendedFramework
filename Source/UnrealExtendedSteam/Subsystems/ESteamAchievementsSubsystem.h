// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OnlineStats.h"
#include "ESteamAchievementsSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamAchievementsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Get all the available achievements and cache them */
	void QueryAchievements();
	
	/** Called when the cache finishes */
	void OnQueryAchievementsComplete(const FUniqueNetId& PlayerId, const bool bWasSuccessful);

	/** Updates the achievement progress */
	void UpdateAchievementProgress(const FString& Id, float Percent);

protected:
	
	/** The object we're going to use in order to progress any achievement */
	FOnlineAchievementsWritePtr AchievementsWriteObjectPtr;

	/** Implement this for initialization of instances of the system */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
};
