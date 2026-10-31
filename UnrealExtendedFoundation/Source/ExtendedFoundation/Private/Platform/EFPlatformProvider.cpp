// Copyright Moon Punch Games. All Rights Reserved.

#include "Platform/EFPlatformProvider.h"

#include "Platform/EFPlatformCatalog.h"
#include "Platform/EFPlatformMessages.h"
#include "Platform/EFPlatformSettings.h"
#include "Platform/EFPlatformSubsystem.h"

namespace EFPlatformMessages
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AchievementUnlocked, "Extended.Platform.AchievementUnlocked", "An achievement was unlocked through the Extended platform layer.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ScoreSubmitted, "Extended.Platform.ScoreSubmitted", "A leaderboard score was submitted through the Extended platform layer.");
}

const TCHAR* EFPlatform::FeatureToString(EEFPlatformFeature Feature)
{
	switch (Feature)
	{
	case EEFPlatformFeature::Achievements: return TEXT("Achievements");
	case EEFPlatformFeature::Stats:        return TEXT("Stats");
	case EEFPlatformFeature::Leaderboards: return TEXT("Leaderboards");
	case EEFPlatformFeature::CloudStorage: return TEXT("CloudStorage");
	case EEFPlatformFeature::Presence:     return TEXT("Presence");
	default:                               return TEXT("Unknown");
	}
}

TConstArrayView<EEFPlatformFeature> EFPlatform::AllFeatures()
{
	static const EEFPlatformFeature Features[] =
	{
		EEFPlatformFeature::Achievements,
		EEFPlatformFeature::Stats,
		EEFPlatformFeature::Leaderboards,
		EEFPlatformFeature::CloudStorage,
		EEFPlatformFeature::Presence
	};
	return MakeArrayView(Features);
}

// UEFPlatformProviderSubsystem

void UEFPlatformProviderSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UEFPlatformSubsystem* Platform = Collection.InitializeDependency<UEFPlatformSubsystem>())
	{
		Platform->RegisterProvider(this);
	}
}

void UEFPlatformProviderSubsystem::Deinitialize()
{
	if (UEFPlatformSubsystem* Platform = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEFPlatformSubsystem>() : nullptr)
	{
		Platform->UnregisterProvider(this);
	}
	Super::Deinitialize();
}

FEFPlatformUser UEFPlatformProviderSubsystem::GetLocalUser() const
{
	FEFPlatformUser User;
	User.Provider = GetProviderName();
	return User;
}

bool UEFPlatformProviderSubsystem::IsAnyFeatureAvailable() const
{
	for (const EEFPlatformFeature Feature : EFPlatform::AllFeatures())
	{
		if (IsFeatureAvailable(Feature))
		{
			return true;
		}
	}
	return false;
}

FEFResult UEFPlatformProviderSubsystem::NotSupported(const TCHAR* Operation) const
{
	return FEFResult::NotAvailable(GetProviderName(), FString::Printf(TEXT("%s is not supported by the %s provider"), Operation, *GetProviderName().ToString()));
}

void UEFPlatformProviderSubsystem::UnlockAchievement(const FString& AchievementId, FOnResult OnDone)
{
	OnDone(NotSupported(TEXT("UnlockAchievement")));
}

void UEFPlatformProviderSubsystem::SetAchievementProgress(const FString& AchievementId, float Progress, FOnResult OnDone)
{
	if (Progress >= 1.f)
	{
		UnlockAchievement(AchievementId, MoveTemp(OnDone));
		return;
	}
	OnDone(NotSupported(TEXT("Partial achievement progress")));
}

void UEFPlatformProviderSubsystem::QueryAchievements(FOnAchievements OnDone)
{
	OnDone(NotSupported(TEXT("QueryAchievements")), TArray<FEFAchievementState>());
}

void UEFPlatformProviderSubsystem::UpdateStats(const TArray<FEFStatUpdate>& Updates, FOnResult OnDone)
{
	OnDone(NotSupported(TEXT("UpdateStats")));
}

void UEFPlatformProviderSubsystem::QueryStats(const TArray<FString>& StatIds, FOnStats OnDone)
{
	OnDone(NotSupported(TEXT("QueryStats")), TArray<FEFStatValue>());
}

void UEFPlatformProviderSubsystem::SubmitScore(const FString& LeaderboardId, int64 Score, FOnResult OnDone)
{
	OnDone(NotSupported(TEXT("SubmitScore")));
}

void UEFPlatformProviderSubsystem::QueryLeaderboard(const FString& LeaderboardId, EEFLeaderboardRange Range, int32 Count, FOnLeaderboard OnDone)
{
	OnDone(NotSupported(TEXT("QueryLeaderboard")), TArray<FEFLeaderboardEntry>());
}

void UEFPlatformProviderSubsystem::WriteCloudFile(const FString& FileName, const TArray<uint8>& Data, FOnResult OnDone)
{
	OnDone(NotSupported(TEXT("WriteCloudFile")));
}

void UEFPlatformProviderSubsystem::ReadCloudFile(const FString& FileName, FOnCloudFile OnDone)
{
	OnDone(NotSupported(TEXT("ReadCloudFile")), TArray<uint8>());
}

void UEFPlatformProviderSubsystem::DeleteCloudFile(const FString& FileName, FOnResult OnDone)
{
	OnDone(NotSupported(TEXT("DeleteCloudFile")));
}

void UEFPlatformProviderSubsystem::ListCloudFiles(FOnCloudFiles OnDone)
{
	OnDone(NotSupported(TEXT("ListCloudFiles")), TArray<FEFCloudFileInfo>());
}

void UEFPlatformProviderSubsystem::SetPresence(const FString& Status, const TMap<FString, FString>& Properties, FOnResult OnDone)
{
	OnDone(NotSupported(TEXT("SetPresence")));
}

// UEFPlatformCatalog

const TArray<FEFPlatformCatalogEntry>& UEFPlatformCatalog::GetEntries(EEFPlatformCatalogKind Kind) const
{
	switch (Kind)
	{
	case EEFPlatformCatalogKind::Stat:        return Stats;
	case EEFPlatformCatalogKind::Leaderboard: return Leaderboards;
	case EEFPlatformCatalogKind::Achievement:
	default:                                  return Achievements;
	}
}

const FEFPlatformCatalogEntry* UEFPlatformCatalog::FindEntry(EEFPlatformCatalogKind Kind, FName Id) const
{
	return GetEntries(Kind).FindByPredicate([Id](const FEFPlatformCatalogEntry& Entry) { return Entry.Id == Id; });
}

FString UEFPlatformCatalog::ToProviderId(EEFPlatformCatalogKind Kind, FName Id, FName Provider) const
{
	if (const FEFPlatformCatalogEntry* Entry = FindEntry(Kind, Id))
	{
		if (const FString* Override = Entry->ProviderIds.Find(Provider))
		{
			if (!Override->IsEmpty())
			{
				return *Override;
			}
		}
	}
	return Id.ToString();
}

FName UEFPlatformCatalog::FromProviderId(EEFPlatformCatalogKind Kind, const FString& ProviderId, FName Provider) const
{
	for (const FEFPlatformCatalogEntry& Entry : GetEntries(Kind))
	{
		const FString* Override = Entry.ProviderIds.Find(Provider);
		const FString& EntryProviderId = (Override && !Override->IsEmpty()) ? *Override : Entry.Id.ToString();
		if (EntryProviderId.Equals(ProviderId, ESearchCase::IgnoreCase))
		{
			return Entry.Id;
		}
	}
	return FName(*ProviderId);
}

TArray<FString> UEFPlatformCatalog::FindProblems() const
{
	TArray<FString> Problems;
	const EEFPlatformCatalogKind Kinds[] = { EEFPlatformCatalogKind::Achievement, EEFPlatformCatalogKind::Stat, EEFPlatformCatalogKind::Leaderboard };
	for (const EEFPlatformCatalogKind Kind : Kinds)
	{
		const TCHAR* KindName = Kind == EEFPlatformCatalogKind::Achievement ? TEXT("achievement") : Kind == EEFPlatformCatalogKind::Stat ? TEXT("stat") : TEXT("leaderboard");
		TSet<FName> SeenIds;
		TMap<FString, FName> SeenProviderIds;  // "Provider|id" -> game id
		for (const FEFPlatformCatalogEntry& Entry : GetEntries(Kind))
		{
			if (Entry.Id.IsNone())
			{
				Problems.Add(FString::Printf(TEXT("A %s has no Id."), KindName));
				continue;
			}
			if (SeenIds.Contains(Entry.Id))
			{
				Problems.Add(FString::Printf(TEXT("The %s id %s is listed twice."), KindName, *Entry.Id.ToString()));
			}
			SeenIds.Add(Entry.Id);

			for (const TPair<FName, FString>& Pair : Entry.ProviderIds)
			{
				const FString Key = Pair.Key.ToString() + TEXT("|") + Pair.Value.ToLower();
				if (const FName* Other = SeenProviderIds.Find(Key))
				{
					Problems.Add(FString::Printf(TEXT("The %s ids %s and %s both map to '%s' on %s."), KindName, *Other->ToString(), *Entry.Id.ToString(), *Pair.Value, *Pair.Key.ToString()));
				}
				SeenProviderIds.Add(Key, Entry.Id);
			}
		}
	}
	return Problems;
}

// UEFPlatformSettings

UEFPlatformSettings::UEFPlatformSettings()
{
	ProviderOrder = { FName(TEXT("Steam")), FName(TEXT("EOS")), FName(TEXT("PlayFab")), FName(TEXT("Local")) };
}
