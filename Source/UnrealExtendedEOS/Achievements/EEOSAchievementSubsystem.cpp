// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSAchievementSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineAchievementsInterface.h"
#include "UnrealExtendedEOS.h"

void UEEOSAchievementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSAchievementSubsystem::Deinitialize()
{
	CachedAchievements.Empty();
	Super::Deinitialize();
}

void UEEOSAchievementSubsystem::QueryAchievements()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryAchievements"));
		OnAchievementsQueried.Broadcast(false, TArray<FEEOSAchievement>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineAchievementsPtr AchievementsInterface = EOSSub->GetAchievementsInterface();
	if (!AchievementsInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAchievementSubsystem::QueryAchievements — Achievements interface not available"));
		OnAchievementsQueried.Broadcast(false, TArray<FEEOSAchievement>());
		return;
	}

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAchievementSubsystem::QueryAchievements — User not logged in"));
		OnAchievementsQueried.Broadcast(false, TArray<FEEOSAchievement>());
		return;
	}

	AchievementsInterface->QueryAchievements(*UserId,
		FOnQueryAchievementsCompleteDelegate::CreateUObject(this, &UEEOSAchievementSubsystem::HandleQueryAchievementsComplete));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAchievementSubsystem::QueryAchievements — Querying achievements..."));
}

void UEEOSAchievementSubsystem::UnlockAchievement(const FString& AchievementId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("UnlockAchievement"));
		OnAchievementUnlocked.Broadcast(false, AchievementId);
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineAchievementsPtr AchievementsInterface = EOSSub->GetAchievementsInterface();
	if (!AchievementsInterface.IsValid())
	{
		OnAchievementUnlocked.Broadcast(false, AchievementId);
		return;
	}

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		OnAchievementUnlocked.Broadcast(false, AchievementId);
		return;
	}

	FOnlineAchievementsWriteRef WriteObject = MakeShareable(new FOnlineAchievementsWrite());
	WriteObject->SetFloatStat(AchievementId, 100.0f);

	AchievementsInterface->WriteAchievements(*UserId, WriteObject,
		FOnAchievementsWrittenDelegate::CreateLambda([this, AchievementId](const FUniqueNetId& InUserId, bool bWasSuccessful)
		{
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAchievementSubsystem: Unlock '%s' %s"), *AchievementId, bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
			OnAchievementUnlocked.Broadcast(bWasSuccessful, AchievementId);
		}));
}

TArray<FEEOSAchievement> UEEOSAchievementSubsystem::GetAchievements() const
{
	return CachedAchievements;
}

bool UEEOSAchievementSubsystem::GetAchievementById(const FString& AchievementId, FEEOSAchievement& OutAchievement) const
{
	for (const auto& Achievement : CachedAchievements)
	{
		if (Achievement.AchievementId == AchievementId)
		{
			OutAchievement = Achievement;
			return true;
		}
	}
	return false;
}

float UEEOSAchievementSubsystem::GetOverallProgress() const
{
	if (CachedAchievements.Num() == 0) return 0.f;

	int32 Unlocked = 0;
	for (const auto& Achievement : CachedAchievements)
	{
		if (Achievement.bUnlocked) Unlocked++;
	}
	return static_cast<float>(Unlocked) / static_cast<float>(CachedAchievements.Num());
}

int32 UEEOSAchievementSubsystem::GetUnlockedCount() const
{
	int32 Count = 0;
	for (const auto& Achievement : CachedAchievements)
	{
		if (Achievement.bUnlocked) Count++;
	}
	return Count;
}

int32 UEEOSAchievementSubsystem::GetTotalCount() const
{
	return CachedAchievements.Num();
}

bool UEEOSAchievementSubsystem::IsAchievementUnlocked(const FString& AchievementId) const
{
	FEEOSAchievement Ach;
	if (GetAchievementById(AchievementId, Ach))
	{
		return Ach.bUnlocked;
	}
	return false;
}

void UEEOSAchievementSubsystem::SetAchievementProgress(const FString& AchievementId, float Progress)
{
	Progress = FMath::Clamp(Progress, 0.f, 1.f);

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("SetAchievementProgress"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineAchievementsPtr AchievementsInterface = EOSSub->GetAchievementsInterface();
	if (!AchievementsInterface.IsValid()) return;

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid()) return;

	FOnlineAchievementsWriteRef WriteObject = MakeShareable(new FOnlineAchievementsWrite());
	WriteObject->SetFloatStat(AchievementId, Progress * 100.f);

	const bool bUnlocked = Progress >= 1.f;
	AchievementsInterface->WriteAchievements(*UserId, WriteObject,
		FOnAchievementsWrittenDelegate::CreateLambda([this, AchievementId, Progress, bUnlocked](const FUniqueNetId& InUserId, bool bWasSuccessful)
		{
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAchievementSubsystem: SetProgress '%s' to %.1f%% — %s"),
				*AchievementId, Progress * 100.f, bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
			OnAchievementProgressUpdated.Broadcast(AchievementId, Progress, bUnlocked);
		}));
}

void UEEOSAchievementSubsystem::IncrementAchievementProgress(const FString& AchievementId, float IncrementAmount)
{
	FEEOSAchievement Ach;
	float NewProgress = IncrementAmount;
	if (GetAchievementById(AchievementId, Ach))
	{
		NewProgress = FMath::Clamp(Ach.Progress + IncrementAmount, 0.f, 1.f);
	}
	SetAchievementProgress(AchievementId, NewProgress);
}

void UEEOSAchievementSubsystem::ResetAchievement(const FString& AchievementId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ResetAchievement"));
		return;
	}

	// NOTE: EOS achievements are forward-only — the backend does not support decreasing progress
	// via the client SDK. Only the EOS DevPortal or server-side tools can truly reset achievements.
	// This function clears the LOCAL cached state so the UI reflects a reset. A subsequent
	// QueryAchievements call will restore the actual backend state.

	bool bFound = false;
	for (FEEOSAchievement& Ach : CachedAchievements)
	{
		if (Ach.AchievementId == AchievementId)
		{
			Ach.Progress = 0.f;
			Ach.bUnlocked = false;
			bFound = true;
			break;
		}
	}

	if (bFound)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAchievementSubsystem::ResetAchievement — Reset '%s' locally (EOS backend is forward-only, use DevPortal for true reset)"), *AchievementId);
		OnAchievementProgressUpdated.Broadcast(AchievementId, 0.f, false);
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAchievementSubsystem::ResetAchievement — Achievement '%s' not found in cache"), *AchievementId);
	}
}

void UEEOSAchievementSubsystem::HandleQueryAchievementsComplete(const FUniqueNetId& PlayerId, const bool bWasSuccessful)
{
	CachedAchievements.Empty();

	if (bWasSuccessful)
	{
		if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
		{
			IOnlineAchievementsPtr AchievementsInterface = EOSSub->GetAchievementsInterface();
			if (AchievementsInterface.IsValid())
			{
				TArray<FOnlineAchievement> OnlineAchievements;
				if (AchievementsInterface->GetCachedAchievements(PlayerId, OnlineAchievements) == EOnlineCachedResult::Success)
				{
					for (const auto& OnlineAch : OnlineAchievements)
					{
						FEEOSAchievement Ach;
						Ach.AchievementId = OnlineAch.Id;
						Ach.Progress = OnlineAch.Progress / 100.f;
						Ach.bUnlocked = (OnlineAch.Progress >= 100.f);
						CachedAchievements.Add(Ach);
					}
				}
			}
		}
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAchievementSubsystem: Query achievements %s — %d achievements"), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"), CachedAchievements.Num());
	OnAchievementsQueried.Broadcast(bWasSuccessful, CachedAchievements);
}
