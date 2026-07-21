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
	LocalPartialProgress.Empty();
	Super::Deinitialize();
}

bool UEEOSAchievementSubsystem::QueryAchievements()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryAchievements"));
		OnAchievementsQueried.Broadcast(false, TArray<FEEOSAchievement>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineAchievementsPtr AchievementsInterface = EOSSub->GetAchievementsInterface();
	if (!AchievementsInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAchievementSubsystem::QueryAchievements — Achievements interface not available"));
		OnAchievementsQueried.Broadcast(false, TArray<FEEOSAchievement>());
		return false;
	}

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAchievementSubsystem::QueryAchievements — User not logged in"));
		OnAchievementsQueried.Broadcast(false, TArray<FEEOSAchievement>());
		return false;
	}

	AchievementsInterface->QueryAchievements(*UserId,
		FOnQueryAchievementsCompleteDelegate::CreateUObject(this, &UEEOSAchievementSubsystem::HandleQueryAchievementsComplete));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAchievementSubsystem::QueryAchievements — Querying achievements..."));
	return true;
}

bool UEEOSAchievementSubsystem::UnlockAchievement(const FString& AchievementId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("UnlockAchievement"));
		OnAchievementUnlocked.Broadcast(false, AchievementId);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineAchievementsPtr AchievementsInterface = EOSSub->GetAchievementsInterface();
	if (!AchievementsInterface.IsValid())
	{
		OnAchievementUnlocked.Broadcast(false, AchievementId);
		return false;
	}

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		OnAchievementUnlocked.Broadcast(false, AchievementId);
		return false;
	}

	FOnlineAchievementsWriteRef WriteObject = MakeShareable(new FOnlineAchievementsWrite());
	WriteObject->SetFloatStat(AchievementId, 100.0f);

	AchievementsInterface->WriteAchievements(*UserId, WriteObject,
		FOnAchievementsWrittenDelegate::CreateWeakLambda(this, [this, AchievementId](const FUniqueNetId& InUserId, bool bWasSuccessful)
		{
			if (bWasSuccessful)
			{
				// Reflect the confirmed unlock in the cache immediately —
				// IsAchievementUnlocked() must not report false until a re-query
				MarkAchievementUnlockedInCache(AchievementId);
			}
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAchievementSubsystem: Unlock '%s' %s"), *AchievementId, bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
			OnAchievementUnlocked.Broadcast(bWasSuccessful, AchievementId);
		}));
	return true;
}

void UEEOSAchievementSubsystem::MarkAchievementUnlockedInCache(const FString& AchievementId)
{
	bool bFound = false;
	for (FEEOSAchievement& Ach : CachedAchievements)
	{
		if (Ach.AchievementId == AchievementId)
		{
			Ach.Progress = 1.f;
			Ach.bUnlocked = true;
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		FEEOSAchievement NewAch;
		NewAch.AchievementId = AchievementId;
		NewAch.Progress = 1.f;
		NewAch.bUnlocked = true;
		CachedAchievements.Add(NewAch);
	}

	// An unlocked achievement has no meaningful partial any more
	LocalPartialProgress.Remove(AchievementId);
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

bool UEEOSAchievementSubsystem::SetAchievementProgress(const FString& AchievementId, float Progress)
{
	Progress = FMath::Clamp(Progress, 0.f, 1.f);

	// NOTE: The EOS OSS implements WriteAchievements as UnlockAchievements — the stat value is
	// ignored and the achievement is PERMANENTLY unlocked on the forward-only backend. Partial
	// progress must therefore never reach WriteAchievements. On EOS, real partial progress is
	// driven by stat thresholds configured in the Dev Portal (ingest via EEOSStatsSubsystem);
	// this function tracks progress locally and only performs a backend write at 100%.
	if (Progress < 1.f)
	{
		bool bFound = false;
		for (FEEOSAchievement& Ach : CachedAchievements)
		{
			if (Ach.AchievementId == AchievementId)
			{
				Ach.Progress = Progress;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			FEEOSAchievement NewAch;
			NewAch.AchievementId = AchievementId;
			NewAch.Progress = Progress;
			CachedAchievements.Add(NewAch);
		}

		// Remember the partial separately so a QueryAchievements cache rebuild can merge it
		// back — the backend knows nothing about locally-tracked progress
		LocalPartialProgress.Add(AchievementId, Progress);

		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAchievementSubsystem::SetAchievementProgress — '%s' at %.1f%% (local only; backend progress comes from Dev Portal stat thresholds)"),
			*AchievementId, Progress * 100.f);
		OnAchievementProgressUpdated.Broadcast(AchievementId, Progress, false);
		return true;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("SetAchievementProgress"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineAchievementsPtr AchievementsInterface = EOSSub->GetAchievementsInterface();
	if (!AchievementsInterface.IsValid()) return false;

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid()) return false;

	FOnlineAchievementsWriteRef WriteObject = MakeShareable(new FOnlineAchievementsWrite());
	WriteObject->SetFloatStat(AchievementId, 100.0f);

	AchievementsInterface->WriteAchievements(*UserId, WriteObject,
		FOnAchievementsWrittenDelegate::CreateWeakLambda(this, [this, AchievementId](const FUniqueNetId& InUserId, bool bWasSuccessful)
		{
			if (bWasSuccessful)
			{
				// Reflect the confirmed unlock in the cache immediately —
				// IsAchievementUnlocked() must not report false until a re-query
				MarkAchievementUnlockedInCache(AchievementId);
			}
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAchievementSubsystem: SetProgress '%s' to 100%% (unlock) — %s"),
				*AchievementId, bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
			OnAchievementProgressUpdated.Broadcast(AchievementId, 1.f, bWasSuccessful);
		}));
	return true;
}

bool UEEOSAchievementSubsystem::IncrementAchievementProgress(const FString& AchievementId, float IncrementAmount)
{
	FEEOSAchievement Ach;
	float NewProgress = IncrementAmount;
	if (GetAchievementById(AchievementId, Ach))
	{
		NewProgress = FMath::Clamp(Ach.Progress + IncrementAmount, 0.f, 1.f);
	}
	return SetAchievementProgress(AchievementId, NewProgress);
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
	LocalPartialProgress.Remove(AchievementId);

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
	// Only rebuild the cache on success — a transient query failure must not wipe the
	// last-known-good state (including locally tracked partial progress).
	if (bWasSuccessful)
	{
		CachedAchievements.Empty();

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
						// The EOS OSS caches achievement progress on a 0–1 scale (1.0 = unlocked)
						Ach.Progress = OnlineAch.Progress;
						Ach.bUnlocked = (OnlineAch.Progress >= 1.f);

						// Merge local-only partial progress back into the rebuilt cache: the
						// backend knows nothing about locally tracked progress, so a rebuild
						// would silently erase it. Locked achievements take
						// max(backend progress, local partial); unlocked ones stay at 1.0
						// (their stale partial is dropped).
						if (const float* LocalProgress = LocalPartialProgress.Find(Ach.AchievementId))
						{
							if (Ach.bUnlocked)
							{
								LocalPartialProgress.Remove(Ach.AchievementId);
							}
							else
							{
								Ach.Progress = FMath::Max(Ach.Progress, *LocalProgress);
							}
						}

						CachedAchievements.Add(Ach);
					}
				}
			}
		}
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAchievementSubsystem: Query achievements %s — %d achievements"), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"), CachedAchievements.Num());
	OnAchievementsQueried.Broadcast(bWasSuccessful, CachedAchievements);
}
