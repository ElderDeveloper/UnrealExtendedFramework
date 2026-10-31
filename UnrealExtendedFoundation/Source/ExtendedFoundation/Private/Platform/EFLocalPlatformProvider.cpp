// Copyright Moon Punch Games. All Rights Reserved.

#include "Platform/EFLocalPlatformProvider.h"

#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "ExtendedFoundationModule.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Platform/EFPlatformSettings.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

const FName UEFLocalPlatformProvider::ProviderName(TEXT("Local"));

namespace EFLocalPlatformPrivate
{
	FString StorageRootOverride;

	FString TimeToString(const FDateTime& Time)
	{
		return Time.GetTicks() > 0 ? Time.ToIso8601() : FString();
	}

	FDateTime TimeFromString(const FString& Text)
	{
		FDateTime Time;
		return (!Text.IsEmpty() && FDateTime::ParseIso8601(*Text, Time)) ? Time : FDateTime();
	}
}

bool UEFLocalPlatformProvider::ShouldCreateSubsystem(UObject* Outer) const
{
	return UEFPlatformSettings::Get()->bEnableLocalProvider && Super::ShouldCreateSubsystem(Outer);
}

void UEFLocalPlatformProvider::Initialize(FSubsystemCollectionBase& Collection)
{
	StorageRoot = GetStorageRoot();
	LoadProfile();
	Super::Initialize(Collection);
}

void UEFLocalPlatformProvider::Deinitialize()
{
	SaveProfile();
	Super::Deinitialize();
}

FString UEFLocalPlatformProvider::GetStorageRoot()
{
	return EFLocalPlatformPrivate::StorageRootOverride.IsEmpty()
		? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExtendedPlatform"))
		: EFLocalPlatformPrivate::StorageRootOverride;
}

void UEFLocalPlatformProvider::SetStorageRootForTests(const FString& Root)
{
	EFLocalPlatformPrivate::StorageRootOverride = Root;
}

FString UEFLocalPlatformProvider::GetProfilePath() const
{
	return FPaths::Combine(StorageRoot, TEXT("Profile.json"));
}

FString UEFLocalPlatformProvider::GetCloudFolder() const
{
	return FPaths::Combine(StorageRoot, TEXT("Cloud"));
}

bool UEFLocalPlatformProvider::IsFeatureAvailable(EEFPlatformFeature Feature) const
{
	return Feature != EEFPlatformFeature::CloudStorage || UEFPlatformSettings::Get()->bLocalProviderCloudStorage;
}

FEFPlatformUser UEFLocalPlatformProvider::GetLocalUser() const
{
	FEFPlatformUser User;
	User.Provider = ProviderName;
	User.UserId = UserId;
	User.DisplayName = FPlatformProcess::UserName();
	User.bSignedIn = true;
	return User;
}

FEFResult UEFLocalPlatformProvider::Ok() const
{
	return FEFResult::Success(ProviderName);
}

FEFResult UEFLocalPlatformProvider::Fail(const FString& Code, const FString& Message) const
{
	return FEFResult::Failure(ProviderName, Code, Message);
}

void UEFLocalPlatformProvider::Later(TFunction<void(UEFLocalPlatformProvider& This)> Work)
{
	TWeakObjectPtr<UEFLocalPlatformProvider> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, Work = MoveTemp(Work)](float)
	{
		if (UEFLocalPlatformProvider* This = WeakThis.Get())
		{
			Work(*This);
		}
		return false;
	}));
}

// Profile storage

void UEFLocalPlatformProvider::LoadProfile()
{
	Achievements.Reset();
	Stats.Reset();
	Leaderboards.Reset();
	UserId.Reset();

	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (FFileHelper::LoadFileToString(Text, *GetProfilePath()))
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogExtendedFoundation, Warning, TEXT("Platform Local: %s is not valid JSON; starting a new profile."), *GetProfilePath());
			Root.Reset();
		}
	}

	if (Root.IsValid())
	{
		Root->TryGetStringField(TEXT("userId"), UserId);

		const TSharedPtr<FJsonObject>* AchievementsJson = nullptr;
		if (Root->TryGetObjectField(TEXT("achievements"), AchievementsJson))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*AchievementsJson)->Values)
			{
				const TSharedPtr<FJsonObject>* Entry = nullptr;
				if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Entry))
				{
					continue;
				}
				FAchievement& Achievement = Achievements.Add(Pair.Key);
				(*Entry)->TryGetBoolField(TEXT("unlocked"), Achievement.bUnlocked);
				double Progress = 0.0;
				if ((*Entry)->TryGetNumberField(TEXT("progress"), Progress))
				{
					Achievement.Progress = FMath::Clamp(static_cast<float>(Progress), 0.f, 1.f);
				}
				FString UnlockTime;
				(*Entry)->TryGetStringField(TEXT("unlockTime"), UnlockTime);
				Achievement.UnlockTimeUtc = EFLocalPlatformPrivate::TimeFromString(UnlockTime);
			}
		}

		const TSharedPtr<FJsonObject>* StatsJson = nullptr;
		if (Root->TryGetObjectField(TEXT("stats"), StatsJson))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*StatsJson)->Values)
			{
				// Stored as text: JSON numbers lose int64 precision above 2^53.
				int64 Value = 0;
				FString ValueText;
				if (Pair.Value.IsValid() && Pair.Value->TryGetString(ValueText))
				{
					LexFromString(Value, *ValueText);
					Stats.Add(Pair.Key, Value);
				}
			}
		}

		const TSharedPtr<FJsonObject>* BoardsJson = nullptr;
		if (Root->TryGetObjectField(TEXT("leaderboards"), BoardsJson))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*BoardsJson)->Values)
			{
				const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
				if (!Pair.Value.IsValid() || !Pair.Value->TryGetArray(Entries))
				{
					continue;
				}
				TArray<FScore>& Scores = Leaderboards.Add(Pair.Key);
				for (const TSharedPtr<FJsonValue>& EntryValue : *Entries)
				{
					const TSharedPtr<FJsonObject>* Entry = nullptr;
					if (!EntryValue.IsValid() || !EntryValue->TryGetObject(Entry))
					{
						continue;
					}
					FScore& Score = Scores.AddDefaulted_GetRef();
					FString ScoreText;
					FString TimeText;
					(*Entry)->TryGetStringField(TEXT("score"), ScoreText);
					(*Entry)->TryGetStringField(TEXT("time"), TimeText);
					LexFromString(Score.Score, *ScoreText);
					Score.TimeUtc = EFLocalPlatformPrivate::TimeFromString(TimeText);
				}
			}
		}
	}

	if (UserId.IsEmpty())
	{
		UserId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
		SaveProfile();
	}
}

bool UEFLocalPlatformProvider::SaveProfile() const
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("userId"), UserId);

	const TSharedRef<FJsonObject> AchievementsJson = MakeShared<FJsonObject>();
	for (const TPair<FString, FAchievement>& Pair : Achievements)
	{
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetBoolField(TEXT("unlocked"), Pair.Value.bUnlocked);
		Entry->SetNumberField(TEXT("progress"), Pair.Value.Progress);
		Entry->SetStringField(TEXT("unlockTime"), EFLocalPlatformPrivate::TimeToString(Pair.Value.UnlockTimeUtc));
		AchievementsJson->SetObjectField(Pair.Key, Entry);
	}
	Root->SetObjectField(TEXT("achievements"), AchievementsJson);

	const TSharedRef<FJsonObject> StatsJson = MakeShared<FJsonObject>();
	for (const TPair<FString, int64>& Pair : Stats)
	{
		StatsJson->SetStringField(Pair.Key, LexToString(Pair.Value));
	}
	Root->SetObjectField(TEXT("stats"), StatsJson);

	const TSharedRef<FJsonObject> BoardsJson = MakeShared<FJsonObject>();
	for (const TPair<FString, TArray<FScore>>& Pair : Leaderboards)
	{
		TArray<TSharedPtr<FJsonValue>> Entries;
		for (const FScore& Score : Pair.Value)
		{
			const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("score"), LexToString(Score.Score));
			Entry->SetStringField(TEXT("time"), EFLocalPlatformPrivate::TimeToString(Score.TimeUtc));
			Entries.Add(MakeShared<FJsonValueObject>(Entry));
		}
		BoardsJson->SetArrayField(Pair.Key, Entries);
	}
	Root->SetObjectField(TEXT("leaderboards"), BoardsJson);

	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return false;
	}

	IFileManager::Get().MakeDirectory(*StorageRoot, /*Tree*/ true);
	if (!FFileHelper::SaveStringToFile(Text, *GetProfilePath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogExtendedFoundation, Warning, TEXT("Platform Local: could not write %s."), *GetProfilePath());
		return false;
	}
	return true;
}

// Achievements

void UEFLocalPlatformProvider::UnlockAchievement(const FString& AchievementId, FOnResult OnDone)
{
	SetAchievementProgress(AchievementId, 1.f, MoveTemp(OnDone));
}

void UEFLocalPlatformProvider::SetAchievementProgress(const FString& AchievementId, float Progress, FOnResult OnDone)
{
	Later([AchievementId, Progress, OnDone = MoveTemp(OnDone)](UEFLocalPlatformProvider& This)
	{
		FAchievement& Achievement = This.Achievements.FindOrAdd(AchievementId);
		if (!Achievement.bUnlocked)
		{
			// Progress only moves forward, like on the stores.
			Achievement.Progress = FMath::Max(Achievement.Progress, FMath::Clamp(Progress, 0.f, 1.f));
			if (Achievement.Progress >= 1.f)
			{
				Achievement.bUnlocked = true;
				Achievement.UnlockTimeUtc = FDateTime::UtcNow();
			}
		}
		const bool bSaved = This.SaveProfile();
		OnDone(bSaved ? This.Ok() : This.Fail(EFResultCodes::Failed, TEXT("could not write the local profile")));
	});
}

void UEFLocalPlatformProvider::QueryAchievements(FOnAchievements OnDone)
{
	Later([OnDone = MoveTemp(OnDone)](UEFLocalPlatformProvider& This)
	{
		TArray<FEFAchievementState> States;
		for (const TPair<FString, FAchievement>& Pair : This.Achievements)
		{
			FEFAchievementState& State = States.AddDefaulted_GetRef();
			State.ProviderId = Pair.Key;
			State.bUnlocked = Pair.Value.bUnlocked;
			State.Progress = Pair.Value.bUnlocked ? 1.f : Pair.Value.Progress;
			State.UnlockTimeUtc = Pair.Value.UnlockTimeUtc;
		}
		States.Sort([](const FEFAchievementState& A, const FEFAchievementState& B) { return A.ProviderId < B.ProviderId; });
		OnDone(This.Ok(), States);
	});
}

// Stats

void UEFLocalPlatformProvider::UpdateStats(const TArray<FEFStatUpdate>& Updates, FOnResult OnDone)
{
	Later([Updates, OnDone = MoveTemp(OnDone)](UEFLocalPlatformProvider& This)
	{
		for (const FEFStatUpdate& Update : Updates)
		{
			int64& Value = This.Stats.FindOrAdd(Update.ProviderId);
			Value = Update.Mode == EEFStatUpdateMode::Increment ? Value + Update.Value : Update.Value;
		}
		const bool bSaved = This.SaveProfile();
		OnDone(bSaved ? This.Ok() : This.Fail(EFResultCodes::Failed, TEXT("could not write the local profile")));
	});
}

void UEFLocalPlatformProvider::QueryStats(const TArray<FString>& StatIds, FOnStats OnDone)
{
	Later([StatIds, OnDone = MoveTemp(OnDone)](UEFLocalPlatformProvider& This)
	{
		TArray<FEFStatValue> Values;
		if (StatIds.Num() == 0)
		{
			for (const TPair<FString, int64>& Pair : This.Stats)
			{
				FEFStatValue& Value = Values.AddDefaulted_GetRef();
				Value.ProviderId = Pair.Key;
				Value.Value = Pair.Value;
			}
		}
		else
		{
			for (const FString& Id : StatIds)
			{
				// A stat never written reads as 0, as on the stores.
				FEFStatValue& Value = Values.AddDefaulted_GetRef();
				Value.ProviderId = Id;
				Value.Value = This.Stats.FindRef(Id);
			}
		}
		OnDone(This.Ok(), Values);
	});
}

// Leaderboards

void UEFLocalPlatformProvider::SubmitScore(const FString& LeaderboardId, int64 Score, FOnResult OnDone)
{
	Later([LeaderboardId, Score, OnDone = MoveTemp(OnDone)](UEFLocalPlatformProvider& This)
	{
		TArray<FScore>& Scores = This.Leaderboards.FindOrAdd(LeaderboardId);
		FScore& Entry = Scores.AddDefaulted_GetRef();
		Entry.Score = Score;
		Entry.TimeUtc = FDateTime::UtcNow();
		Scores.StableSort([](const FScore& A, const FScore& B) { return A.Score > B.Score; });
		if (Scores.Num() > MaxScoresPerLeaderboard)
		{
			Scores.SetNum(MaxScoresPerLeaderboard);
		}
		const bool bSaved = This.SaveProfile();
		OnDone(bSaved ? This.Ok() : This.Fail(EFResultCodes::Failed, TEXT("could not write the local profile")));
	});
}

void UEFLocalPlatformProvider::QueryLeaderboard(const FString& LeaderboardId, EEFLeaderboardRange Range, int32 Count, FOnLeaderboard OnDone)
{
	Later([LeaderboardId, Count, OnDone = MoveTemp(OnDone)](UEFLocalPlatformProvider& This)
	{
		// Every range is the local user's own best scores: there is nobody else on this machine's board.
		TArray<FEFLeaderboardEntry> Entries;
		const FEFPlatformUser User = This.GetLocalUser();
		if (const TArray<FScore>* Scores = This.Leaderboards.Find(LeaderboardId))
		{
			for (int32 Index = 0; Index < Scores->Num() && Index < Count; ++Index)
			{
				FEFLeaderboardEntry& Entry = Entries.AddDefaulted_GetRef();
				Entry.Rank = Index + 1;
				Entry.UserId = User.UserId;
				Entry.DisplayName = User.DisplayName;
				Entry.Score = (*Scores)[Index].Score;
				Entry.bIsLocalUser = true;
			}
		}
		OnDone(This.Ok(), Entries);
	});
}

// Cloud storage

bool UEFLocalPlatformProvider::IsValidCloudFileName(const FString& FileName) const
{
	// One file name, no folders: keeps writes inside the cloud folder.
	return !FileName.IsEmpty() && FileName.Len() <= 128 && !FileName.Contains(TEXT("..")) && !FileName.Contains(TEXT("/")) && !FileName.Contains(TEXT("\\")) && !FileName.Contains(TEXT(":"));
}

void UEFLocalPlatformProvider::WriteCloudFile(const FString& FileName, const TArray<uint8>& Data, FOnResult OnDone)
{
	Later([FileName, Data, OnDone = MoveTemp(OnDone)](UEFLocalPlatformProvider& This)
	{
		if (!This.IsValidCloudFileName(FileName))
		{
			OnDone(FEFResult::InvalidArgument(ProviderName, FString::Printf(TEXT("'%s' is not a valid cloud file name"), *FileName)));
			return;
		}
		IFileManager::Get().MakeDirectory(*This.GetCloudFolder(), /*Tree*/ true);
		const bool bSaved = FFileHelper::SaveArrayToFile(Data, *FPaths::Combine(This.GetCloudFolder(), FileName));
		OnDone(bSaved ? This.Ok() : This.Fail(EFResultCodes::Failed, FString::Printf(TEXT("could not write %s"), *FileName)));
	});
}

void UEFLocalPlatformProvider::ReadCloudFile(const FString& FileName, FOnCloudFile OnDone)
{
	Later([FileName, OnDone = MoveTemp(OnDone)](UEFLocalPlatformProvider& This)
	{
		TArray<uint8> Data;
		if (!This.IsValidCloudFileName(FileName))
		{
			OnDone(FEFResult::InvalidArgument(ProviderName, FString::Printf(TEXT("'%s' is not a valid cloud file name"), *FileName)), Data);
			return;
		}
		const FString Path = FPaths::Combine(This.GetCloudFolder(), FileName);
		if (!IFileManager::Get().FileExists(*Path))
		{
			OnDone(This.Fail(EFPlatformResultCodes::NotFound, FString::Printf(TEXT("%s does not exist"), *FileName)), Data);
			return;
		}
		const bool bLoaded = FFileHelper::LoadFileToArray(Data, *Path);
		OnDone(bLoaded ? This.Ok() : This.Fail(EFResultCodes::Failed, FString::Printf(TEXT("could not read %s"), *FileName)), Data);
	});
}

void UEFLocalPlatformProvider::DeleteCloudFile(const FString& FileName, FOnResult OnDone)
{
	Later([FileName, OnDone = MoveTemp(OnDone)](UEFLocalPlatformProvider& This)
	{
		if (!This.IsValidCloudFileName(FileName))
		{
			OnDone(FEFResult::InvalidArgument(ProviderName, FString::Printf(TEXT("'%s' is not a valid cloud file name"), *FileName)));
			return;
		}
		const FString Path = FPaths::Combine(This.GetCloudFolder(), FileName);
		const bool bGone = !IFileManager::Get().FileExists(*Path) || IFileManager::Get().Delete(*Path);
		OnDone(bGone ? This.Ok() : This.Fail(EFResultCodes::Failed, FString::Printf(TEXT("could not delete %s"), *FileName)));
	});
}

void UEFLocalPlatformProvider::ListCloudFiles(FOnCloudFiles OnDone)
{
	Later([OnDone = MoveTemp(OnDone)](UEFLocalPlatformProvider& This)
	{
		TArray<FString> Names;
		IFileManager::Get().FindFiles(Names, *FPaths::Combine(This.GetCloudFolder(), TEXT("*")), /*Files*/ true, /*Directories*/ false);
		Names.Sort();

		TArray<FEFCloudFileInfo> Files;
		for (const FString& Name : Names)
		{
			const FString Path = FPaths::Combine(This.GetCloudFolder(), Name);
			FEFCloudFileInfo& Info = Files.AddDefaulted_GetRef();
			Info.Name = Name;
			Info.SizeBytes = IFileManager::Get().FileSize(*Path);
			Info.ModifiedUtc = IFileManager::Get().GetTimeStamp(*Path);
		}
		OnDone(This.Ok(), Files);
	});
}

// Presence

void UEFLocalPlatformProvider::SetPresence(const FString& Status, const TMap<FString, FString>& Properties, FOnResult OnDone)
{
	PresenceStatus = Status;
	PresenceProperties = Properties;
	UE_LOG(LogExtendedFoundation, Verbose, TEXT("Platform Local: presence is now '%s'."), *Status);
	Later([OnDone = MoveTemp(OnDone)](UEFLocalPlatformProvider& This)
	{
		OnDone(This.Ok());
	});
}
