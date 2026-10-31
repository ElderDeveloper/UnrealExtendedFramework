// Copyright Moon Punch Games. All Rights Reserved.

#include "Platform/EFPlatformAsyncActions.h"

#include "Platform/EFPlatformSubsystem.h"

// Write base

void UEFPlatformAsyncWriteBase::FinishWithResult(const FEFResult& Result)
{
	if (!FinishAction())
	{
		return;
	}
	if (Result.IsSuccess())
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast(Result);
	}
}

TFunction<void(const FEFResult&)> UEFPlatformAsyncWriteBase::MakeResultCallback()
{
	return MakeWeakCallback<const FEFResult&>([this](const FEFResult& Result) { FinishWithResult(Result); });
}

// Unlock Achievement

UEFAsyncUnlockAchievement* UEFAsyncUnlockAchievement::UnlockAchievement(UObject* WorldContextObject, FName AchievementId, float TimeoutSeconds)
{
	UEFAsyncUnlockAchievement* Action = NewObject<UEFAsyncUnlockAchievement>();
	Action->RequestedAchievementId = AchievementId;
	Action->InitAsyncAction(WorldContextObject, TimeoutSeconds);
	return Action;
}

void UEFAsyncUnlockAchievement::ExecuteAction()
{
	if (UEFPlatformSubsystem* Platform = RequireGameInstanceSubsystem<UEFPlatformSubsystem>())
	{
		Platform->UnlockAchievement(RequestedAchievementId, MakeResultCallback());
	}
}

// Set Achievement Progress

UEFAsyncSetAchievementProgress* UEFAsyncSetAchievementProgress::SetAchievementProgress(UObject* WorldContextObject, FName AchievementId, float Progress, float TimeoutSeconds)
{
	UEFAsyncSetAchievementProgress* Action = NewObject<UEFAsyncSetAchievementProgress>();
	Action->RequestedAchievementId = AchievementId;
	Action->RequestedProgress = Progress;
	Action->InitAsyncAction(WorldContextObject, TimeoutSeconds);
	return Action;
}

void UEFAsyncSetAchievementProgress::ExecuteAction()
{
	if (UEFPlatformSubsystem* Platform = RequireGameInstanceSubsystem<UEFPlatformSubsystem>())
	{
		Platform->SetAchievementProgress(RequestedAchievementId, RequestedProgress, MakeResultCallback());
	}
}

// Update Stat

UEFAsyncUpdateStat* UEFAsyncUpdateStat::UpdateStat(UObject* WorldContextObject, FName StatId, int64 Value, EEFStatUpdateMode Mode, float TimeoutSeconds)
{
	UEFAsyncUpdateStat* Action = NewObject<UEFAsyncUpdateStat>();
	Action->RequestedStatId = StatId;
	Action->RequestedValue = Value;
	Action->RequestedMode = Mode;
	Action->InitAsyncAction(WorldContextObject, TimeoutSeconds);
	return Action;
}

void UEFAsyncUpdateStat::ExecuteAction()
{
	if (UEFPlatformSubsystem* Platform = RequireGameInstanceSubsystem<UEFPlatformSubsystem>())
	{
		Platform->UpdateStat(RequestedStatId, RequestedValue, RequestedMode, MakeResultCallback());
	}
}

// Submit Score

UEFAsyncSubmitScore* UEFAsyncSubmitScore::SubmitScore(UObject* WorldContextObject, FName LeaderboardId, int64 Score, float TimeoutSeconds)
{
	UEFAsyncSubmitScore* Action = NewObject<UEFAsyncSubmitScore>();
	Action->RequestedLeaderboardId = LeaderboardId;
	Action->RequestedScore = Score;
	Action->InitAsyncAction(WorldContextObject, TimeoutSeconds);
	return Action;
}

void UEFAsyncSubmitScore::ExecuteAction()
{
	if (UEFPlatformSubsystem* Platform = RequireGameInstanceSubsystem<UEFPlatformSubsystem>())
	{
		Platform->SubmitScore(RequestedLeaderboardId, RequestedScore, MakeResultCallback());
	}
}

// Write / Delete Cloud File

UEFAsyncWriteCloudFile* UEFAsyncWriteCloudFile::WriteCloudFile(UObject* WorldContextObject, const FString& FileName, const TArray<uint8>& Data, float TimeoutSeconds)
{
	UEFAsyncWriteCloudFile* Action = NewObject<UEFAsyncWriteCloudFile>();
	Action->RequestedFileName = FileName;
	Action->RequestedData = Data;
	Action->InitAsyncAction(WorldContextObject, TimeoutSeconds);
	return Action;
}

void UEFAsyncWriteCloudFile::ExecuteAction()
{
	if (UEFPlatformSubsystem* Platform = RequireGameInstanceSubsystem<UEFPlatformSubsystem>())
	{
		Platform->WriteCloudFile(RequestedFileName, RequestedData, MakeResultCallback());
	}
}

UEFAsyncDeleteCloudFile* UEFAsyncDeleteCloudFile::DeleteCloudFile(UObject* WorldContextObject, const FString& FileName, float TimeoutSeconds)
{
	UEFAsyncDeleteCloudFile* Action = NewObject<UEFAsyncDeleteCloudFile>();
	Action->RequestedFileName = FileName;
	Action->InitAsyncAction(WorldContextObject, TimeoutSeconds);
	return Action;
}

void UEFAsyncDeleteCloudFile::ExecuteAction()
{
	if (UEFPlatformSubsystem* Platform = RequireGameInstanceSubsystem<UEFPlatformSubsystem>())
	{
		Platform->DeleteCloudFile(RequestedFileName, MakeResultCallback());
	}
}

// Queries

UEFAsyncQueryAchievements* UEFAsyncQueryAchievements::QueryAchievements(UObject* WorldContextObject, float TimeoutSeconds)
{
	UEFAsyncQueryAchievements* Action = NewObject<UEFAsyncQueryAchievements>();
	Action->InitAsyncAction(WorldContextObject, TimeoutSeconds);
	return Action;
}

void UEFAsyncQueryAchievements::ExecuteAction()
{
	UEFPlatformSubsystem* Platform = RequireGameInstanceSubsystem<UEFPlatformSubsystem>();
	if (!Platform)
	{
		return;
	}
	Platform->QueryAchievements(MakeWeakCallback<const FEFResult&, const TArray<FEFAchievementState>&>(
		[this](const FEFResult& Result, const TArray<FEFAchievementState>& Achievements)
		{
			if (FinishAction())
			{
				Result.IsSuccess() ? OnSuccess.Broadcast(Achievements) : OnFailure.Broadcast(Result);
			}
		}));
}

UEFAsyncQueryStats* UEFAsyncQueryStats::QueryStats(UObject* WorldContextObject, const TArray<FName>& StatIds, float TimeoutSeconds)
{
	UEFAsyncQueryStats* Action = NewObject<UEFAsyncQueryStats>();
	Action->RequestedStatIds = StatIds;
	Action->InitAsyncAction(WorldContextObject, TimeoutSeconds);
	return Action;
}

void UEFAsyncQueryStats::ExecuteAction()
{
	UEFPlatformSubsystem* Platform = RequireGameInstanceSubsystem<UEFPlatformSubsystem>();
	if (!Platform)
	{
		return;
	}
	Platform->QueryStats(RequestedStatIds, MakeWeakCallback<const FEFResult&, const TArray<FEFStatValue>&>(
		[this](const FEFResult& Result, const TArray<FEFStatValue>& Stats)
		{
			if (FinishAction())
			{
				Result.IsSuccess() ? OnSuccess.Broadcast(Stats) : OnFailure.Broadcast(Result);
			}
		}));
}

UEFAsyncQueryLeaderboard* UEFAsyncQueryLeaderboard::QueryLeaderboard(UObject* WorldContextObject, FName LeaderboardId, EEFLeaderboardRange Range, int32 Count, float TimeoutSeconds)
{
	UEFAsyncQueryLeaderboard* Action = NewObject<UEFAsyncQueryLeaderboard>();
	Action->RequestedLeaderboardId = LeaderboardId;
	Action->RequestedRange = Range;
	Action->RequestedCount = Count;
	Action->InitAsyncAction(WorldContextObject, TimeoutSeconds);
	return Action;
}

void UEFAsyncQueryLeaderboard::ExecuteAction()
{
	UEFPlatformSubsystem* Platform = RequireGameInstanceSubsystem<UEFPlatformSubsystem>();
	if (!Platform)
	{
		return;
	}
	Platform->QueryLeaderboard(RequestedLeaderboardId, RequestedRange, RequestedCount, MakeWeakCallback<const FEFResult&, const TArray<FEFLeaderboardEntry>&>(
		[this](const FEFResult& Result, const TArray<FEFLeaderboardEntry>& Entries)
		{
			if (FinishAction())
			{
				Result.IsSuccess() ? OnSuccess.Broadcast(Entries) : OnFailure.Broadcast(Result);
			}
		}));
}

UEFAsyncReadCloudFile* UEFAsyncReadCloudFile::ReadCloudFile(UObject* WorldContextObject, const FString& FileName, float TimeoutSeconds)
{
	UEFAsyncReadCloudFile* Action = NewObject<UEFAsyncReadCloudFile>();
	Action->RequestedFileName = FileName;
	Action->InitAsyncAction(WorldContextObject, TimeoutSeconds);
	return Action;
}

void UEFAsyncReadCloudFile::ExecuteAction()
{
	UEFPlatformSubsystem* Platform = RequireGameInstanceSubsystem<UEFPlatformSubsystem>();
	if (!Platform)
	{
		return;
	}
	Platform->ReadCloudFile(RequestedFileName, MakeWeakCallback<const FEFResult&, const TArray<uint8>&>(
		[this](const FEFResult& Result, const TArray<uint8>& Data)
		{
			if (FinishAction())
			{
				Result.IsSuccess() ? OnSuccess.Broadcast(Data) : OnFailure.Broadcast(Result);
			}
		}));
}

UEFAsyncListCloudFiles* UEFAsyncListCloudFiles::ListCloudFiles(UObject* WorldContextObject, float TimeoutSeconds)
{
	UEFAsyncListCloudFiles* Action = NewObject<UEFAsyncListCloudFiles>();
	Action->InitAsyncAction(WorldContextObject, TimeoutSeconds);
	return Action;
}

void UEFAsyncListCloudFiles::ExecuteAction()
{
	UEFPlatformSubsystem* Platform = RequireGameInstanceSubsystem<UEFPlatformSubsystem>();
	if (!Platform)
	{
		return;
	}
	Platform->ListCloudFiles(MakeWeakCallback<const FEFResult&, const TArray<FEFCloudFileInfo>&>(
		[this](const FEFResult& Result, const TArray<FEFCloudFileInfo>& Files)
		{
			if (FinishAction())
			{
				Result.IsSuccess() ? OnSuccess.Broadcast(Files) : OnFailure.Broadcast(Result);
			}
		}));
}

// Library

FEFPlatformUser UEFPlatformLibrary::GetPlatformUser(const UObject* WorldContextObject)
{
	const UEFPlatformSubsystem* Platform = UEFPlatformSubsystem::Get(WorldContextObject);
	return Platform ? Platform->GetLocalUser() : FEFPlatformUser();
}

FName UEFPlatformLibrary::GetPlatformProviderName(const UObject* WorldContextObject, EEFPlatformFeature Feature)
{
	const UEFPlatformSubsystem* Platform = UEFPlatformSubsystem::Get(WorldContextObject);
	return Platform ? Platform->GetProviderName(Feature) : NAME_None;
}

bool UEFPlatformLibrary::IsPlatformFeatureAvailable(const UObject* WorldContextObject, EEFPlatformFeature Feature)
{
	const UEFPlatformSubsystem* Platform = UEFPlatformSubsystem::Get(WorldContextObject);
	return Platform && Platform->IsFeatureAvailable(Feature);
}

void UEFPlatformLibrary::SetPlatformPresence(const UObject* WorldContextObject, const FString& Status, const TMap<FString, FString>& Properties)
{
	if (UEFPlatformSubsystem* Platform = UEFPlatformSubsystem::Get(WorldContextObject))
	{
		Platform->SetPresence(Status, Properties);
	}
}

int32 UEFPlatformLibrary::GetQueuedPlatformWriteCount(const UObject* WorldContextObject)
{
	const UEFPlatformSubsystem* Platform = UEFPlatformSubsystem::Get(WorldContextObject);
	return Platform ? Platform->GetQueuedWriteCount() : 0;
}
