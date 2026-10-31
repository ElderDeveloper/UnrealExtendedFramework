// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESaveSubsystem.h"

#include "Assets/EFAssetLoading.h"
#include "Components/ActorComponent.h"
#include "ESaveableComponent.h"
#include "ESaveableInterface.h"
#include "ESaveMigration.h"
#include "ESaveSerialization.h"
#include "ESaveSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "ExtendedSaveModule.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Messages/EFMessageSubsystem.h"
#include "Misc/App.h"
#include "Platform/EFPlatformProvider.h"
#include "Platform/EFPlatformSubsystem.h"
#include "Status/EFServiceStatus.h"
#include "Subsystems/EFSubsystems.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace ESaveMessages
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Saving, "Extended.Save.Saving", "A save is about to capture the world.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Saved, "Extended.Save.Saved", "A save slot was written (payload FESaveMessage).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Loaded, "Extended.Save.Loaded", "A save slot was loaded (payload FESaveMessage).");
}

namespace ESavePrivate
{
	const TCHAR* const Source = TEXT("Save");
	const TCHAR* const BackupSuffix = TEXT("__backup");

	/** Runs Work on the next tick, so callers never see a callback before their call returns. */
	void NextTick(TFunction<void()> Work)
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Work = MoveTemp(Work)](float)
		{
			Work();
			return false;
		}));
	}

	void NotifyBeforeSave(UObject* Object)
	{
		if (Object && Object->GetClass()->ImplementsInterface(UESaveable::StaticClass()))
		{
			IESaveable::Execute_OnBeforeSave(Object);
		}
	}

	void NotifyAfterLoad(UObject* Object)
	{
		if (Object && Object->GetClass()->ImplementsInterface(UESaveable::StaticClass()))
		{
			IESaveable::Execute_OnAfterLoad(Object);
		}
	}

	bool ShouldSaveComponent(const UActorComponent* Component)
	{
		return Component && !Component->IsA<UESaveableComponent>()
			&& (ESaveSerialization::HasSaveGameProperties(Component->GetClass()) || Component->GetClass()->ImplementsInterface(UESaveable::StaticClass()));
	}

	FName GetObjectSaveId(UObject* Object)
	{
		return (Object && Object->GetClass()->ImplementsInterface(UESaveable::StaticClass())) ? IESaveable::Execute_GetSaveId(Object) : NAME_None;
	}
}

// Index

FESaveSlotInfo* UESaveIndex::FindSlot(const FString& SlotName)
{
	return Slots.FindByPredicate([&SlotName](const FESaveSlotInfo& Info) { return Info.SlotName == SlotName; });
}

const FESaveSlotInfo* UESaveIndex::FindSlot(const FString& SlotName) const
{
	return Slots.FindByPredicate([&SlotName](const FESaveSlotInfo& Info) { return Info.SlotName == SlotName; });
}

int32 UESaveGameData::RenameSavedProperty(FName OldName, FName NewName)
{
	int32 Renamed = 0;
	auto RenameIn = [OldName, NewName, &Renamed](TMap<FName, FString>& Properties)
	{
		FString Value;
		if (Properties.RemoveAndCopyValue(OldName, Value))
		{
			Properties.Add(NewName, MoveTemp(Value));
			++Renamed;
		}
	};
	for (TPair<FName, FESaveRecord>& Pair : Records)
	{
		RenameIn(Pair.Value.Properties);
		for (FESaveComponentRecord& Component : Pair.Value.Components)
		{
			RenameIn(Component.Properties);
		}
	}
	return Renamed;
}

TArray<FName> UESaveGameData::GetRecordIds() const
{
	TArray<FName> Ids;
	Records.GetKeys(Ids);
	return Ids;
}

// Subsystem

UESaveSubsystem* UESaveSubsystem::Get(const UObject* WorldContextObject)
{
	return EF::GetGameInstanceSubsystem<UESaveSubsystem>(WorldContextObject);
}

void UESaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UEFMessageSubsystem>();
	Collection.InitializeDependency<UEFPlatformSubsystem>();

	ActiveData = NewObject<UESaveGameData>(this);
	PlayTimeStartSeconds = FPlatformTime::Seconds();
	LoadIndex();

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UESaveSubsystem::HandlePostLoadMap);

	const UESaveSettings* Settings = UESaveSettings::Get();
	TArray<FSoftObjectPath> MigrationPaths;
	for (const TSoftClassPtr<UESaveMigration>& MigrationClass : Settings->Migrations)
	{
		if (!MigrationClass.IsNull() && !MigrationClass.Get())
		{
			MigrationPaths.Add(MigrationClass.ToSoftObjectPath());
		}
	}
	MigrationPreloadHandle = EFAssets::Preload(MoveTemp(MigrationPaths));

	if (Settings->AutosaveIntervalSeconds > 0.f)
	{
		AutosaveTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UESaveSubsystem::TickAutosave),
			FMath::Max(Settings->AutosaveIntervalSeconds, 1.f));
	}

	TWeakObjectPtr<UESaveSubsystem> WeakThis(this);
	StatusHandle = FEFServiceStatusRegistry::Register(TEXT("Save"), [WeakThis]
	{
		FEFServiceStatus Status;
		const UESaveSubsystem* This = WeakThis.Get();
		if (!This)
		{
			Status.State = EEFServiceState::Unavailable;
			return Status;
		}
		Status.State = EEFServiceState::Ready;
		Status.Summary = FString::Printf(TEXT("active slot %s, %d slot(s), %d saveable actor(s), %d object(s)%s"),
			This->ActiveSlotName.IsEmpty() ? TEXT("(none)") : *This->ActiveSlotName,
			This->SlotIndex ? This->SlotIndex->Slots.Num() : 0, This->LiveComponents.Num(), This->LiveSaveables.Num(),
			This->bOperationRunning ? TEXT(", busy") : TEXT(""));
		if (This->PendingOperations.Num() > 0)
		{
			Status.Details.Add(TEXT("Waiting"), FString::Printf(TEXT("%d save/load request(s)"), This->PendingOperations.Num()));
		}
		if (This->AutosaveBlockers.Num() > 0)
		{
			TArray<FString> Reasons;
			for (const FName& Reason : This->AutosaveBlockers)
			{
				Reasons.Add(Reason.ToString());
			}
			Status.Details.Add(TEXT("Autosave blocked"), FString::Join(Reasons, TEXT(", ")));
		}
		Status.Details.Add(TEXT("Play time"), FString::Printf(TEXT("%.0f s"), This->GetPlayTimeSeconds()));
		return Status;
	});
}

void UESaveSubsystem::Deinitialize()
{
	FEFServiceStatusRegistry::Unregister(StatusHandle);
	StatusHandle = 0;
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	PostLoadMapHandle.Reset();
	if (AutosaveTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(AutosaveTickerHandle);
		AutosaveTickerHandle.Reset();
	}

	TArray<FOperation> Waiting = MoveTemp(PendingOperations);
	PendingOperations.Reset();
	for (FOperation& Operation : Waiting)
	{
		if (Operation.Cancel)
		{
			Operation.Cancel(FEFResult::Cancelled(ESavePrivate::Source));
		}
	}

	LiveComponents.Reset();
	LiveSaveables.Reset();
	MigrationPreloadHandle.Reset();
	Super::Deinitialize();
}

// Names and keys

bool UESaveSubsystem::IsValidSlotName(const FString& SlotName)
{
	if (SlotName.IsEmpty() || SlotName.Len() > 64 || SlotName.Contains(TEXT("__")) || SlotName.TrimStartAndEnd() != SlotName)
	{
		return false;
	}
	for (const TCHAR Character : SlotName)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT(' ') && Character != TEXT('-') && Character != TEXT('_'))
		{
			return false;
		}
	}
	return true;
}

FString UESaveSubsystem::GetCopyFileName(const FString& SlotName, int32 Copy)
{
	return Copy == 0 ? SlotName : SlotName + ESavePrivate::BackupSuffix;
}

FString UESaveSubsystem::GetWorldKey(const UWorld* World)
{
	return World ? UWorld::RemovePIEPrefix(World->GetPackage()->GetName()) : FString();
}

FString UESaveSubsystem::GetLevelKey(const AActor* Actor)
{
	if (!Actor)
	{
		return FString();
	}
	const UWorld* World = Actor->GetWorld();
	const ULevel* Level = Actor->GetLevel();
	// World Partition streams actors through generated cell levels whose names are not stable;
	// actor names are unique across the whole partitioned world, so the world is the key there.
	if (!Level || (World && World->GetWorldPartition() != nullptr))
	{
		return GetWorldKey(World);
	}
	return UWorld::RemovePIEPrefix(Level->GetPackage()->GetName());
}

FString UESaveSubsystem::GetCloudFileName(const FString& SlotName) const
{
	return UESaveSettings::Get()->CloudFilePrefix + SlotName + TEXT(".sav");
}

// Operation queue

void UESaveSubsystem::RunOrQueue(FOperation&& Operation)
{
	if (bOperationRunning)
	{
		UE_LOG(LogExtendedSave, Verbose, TEXT("%s waits for the running save/load."), *Operation.Description);
		PendingOperations.Add(MoveTemp(Operation));
		return;
	}
	bOperationRunning = true;
	Operation.Run();
}

void UESaveSubsystem::FinishOperation()
{
	bOperationRunning = false;
	if (PendingOperations.Num() > 0)
	{
		FOperation Next = MoveTemp(PendingOperations[0]);
		PendingOperations.RemoveAt(0);
		bOperationRunning = true;
		Next.Run();
	}
}

// Index

void UESaveSubsystem::LoadIndex()
{
	const UESaveSettings* Settings = UESaveSettings::Get();
	SlotIndex = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(Settings->IndexSlotName, Settings->UserIndex))
	{
		SlotIndex = Cast<UESaveIndex>(UGameplayStatics::LoadGameFromSlot(Settings->IndexSlotName, Settings->UserIndex));
		if (!SlotIndex)
		{
			UE_LOG(LogExtendedSave, Warning, TEXT("The slot index %s could not be read; slots are listed again as they are saved."), *Settings->IndexSlotName);
		}
	}
	if (!SlotIndex)
	{
		SlotIndex = NewObject<UESaveIndex>(this);
	}

	// Slots deleted outside the game (or never finished) leave the list.
	SlotIndex->Slots.RemoveAll([this](const FESaveSlotInfo& Info) { return !DoesSlotExist(Info.SlotName); });
}

void UESaveSubsystem::WriteIndexAsync(TFunction<void(bool bSuccess)> OnDone)
{
	const UESaveSettings* Settings = UESaveSettings::Get();
	UGameplayStatics::AsyncSaveGameToSlot(SlotIndex, Settings->IndexSlotName, Settings->UserIndex,
		FAsyncSaveGameToSlotDelegate::CreateLambda([OnDone = MoveTemp(OnDone)](const FString&, const int32, bool bSuccess)
		{
			if (OnDone)
			{
				OnDone(bSuccess);
			}
		}));
}

// Slots

TArray<FESaveSlotInfo> UESaveSubsystem::GetSlots() const
{
	TArray<FESaveSlotInfo> Slots = SlotIndex ? SlotIndex->Slots : TArray<FESaveSlotInfo>();
	Slots.Sort([](const FESaveSlotInfo& A, const FESaveSlotInfo& B) { return A.SaveTimeUtc > B.SaveTimeUtc; });
	return Slots;
}

bool UESaveSubsystem::DoesSlotExist(const FString& SlotName) const
{
	if (!IsValidSlotName(SlotName))
	{
		return false;
	}
	const int32 UserIndex = UESaveSettings::Get()->UserIndex;
	return UGameplayStatics::DoesSaveGameExist(GetCopyFileName(SlotName, 0), UserIndex)
		|| UGameplayStatics::DoesSaveGameExist(GetCopyFileName(SlotName, 1), UserIndex);
}

bool UESaveSubsystem::DeleteSlot(const FString& SlotName)
{
	if (!IsValidSlotName(SlotName))
	{
		return false;
	}

	const UESaveSettings* Settings = UESaveSettings::Get();
	bool bDeleted = false;
	for (int32 Copy = 0; Copy < 2; ++Copy)
	{
		const FString FileName = GetCopyFileName(SlotName, Copy);
		if (UGameplayStatics::DoesSaveGameExist(FileName, Settings->UserIndex))
		{
			bDeleted |= UGameplayStatics::DeleteGameInSlot(FileName, Settings->UserIndex);
		}
	}

	SlotIndex->Slots.RemoveAll([&SlotName](const FESaveSlotInfo& Info) { return Info.SlotName == SlotName; });
	SlotIndex->NewestCopy.Remove(SlotName);
	UGameplayStatics::SaveGameToSlot(SlotIndex, Settings->IndexSlotName, Settings->UserIndex);

	if (ActiveSlotName == SlotName)
	{
		ActiveSlotName.Reset();
	}
	return bDeleted;
}

void UESaveSubsystem::StartNewGame()
{
	ActiveData = NewObject<UESaveGameData>(this);
	ActiveSlotName.Reset();
	NextThumbnail.Reset();
	PlayTimeBaseSeconds = 0.0;
	PlayTimeStartSeconds = FPlatformTime::Seconds();
}

double UESaveSubsystem::GetPlayTimeSeconds() const
{
	return PlayTimeBaseSeconds + (FPlatformTime::Seconds() - PlayTimeStartSeconds);
}

// Save

void UESaveSubsystem::SaveSlot(const FString& SlotName, FOnSaved OnDone, const FString& DisplayName)
{
	if (!IsValidSlotName(SlotName))
	{
		ESavePrivate::NextTick([OnDone = MoveTemp(OnDone), SlotName]()
		{
			if (OnDone)
			{
				OnDone(FEFResult::InvalidArgument(ESavePrivate::Source, FString::Printf(TEXT("'%s' is not a valid slot name"), *SlotName)));
			}
		});
		return;
	}

	const TSharedRef<FOnSaved> Callback = MakeShared<FOnSaved>(MoveTemp(OnDone));
	TWeakObjectPtr<UESaveSubsystem> WeakThis(this);

	FOperation Operation;
	Operation.Description = FString::Printf(TEXT("Save %s"), *SlotName);
	Operation.Run = [WeakThis, SlotName, DisplayName, Callback]()
	{
		if (UESaveSubsystem* This = WeakThis.Get())
		{
			This->DoSave(SlotName, DisplayName, *Callback);
		}
	};
	Operation.Cancel = [Callback](const FEFResult& Result)
	{
		if (*Callback)
		{
			(*Callback)(Result);
		}
	};
	RunOrQueue(MoveTemp(Operation));
}

void UESaveSubsystem::DoSave(const FString& SlotName, const FString& DisplayName, FOnSaved OnDone)
{
	Broadcast(ESaveMessages::Saving, SlotName, true, false);
	CaptureWorldState();

	const UESaveSettings* Settings = UESaveSettings::Get();
	FESaveSlotInfo& Info = ActiveData->Info;
	Info.SlotName = SlotName;
	if (!DisplayName.IsEmpty())
	{
		Info.DisplayName = DisplayName;
	}
	else if (Info.DisplayName.IsEmpty() || ActiveSlotName != SlotName)
	{
		// The active save may carry another slot's name: keep the name this slot already has.
		const FESaveSlotInfo* Previous = SlotIndex->FindSlot(SlotName);
		Info.DisplayName = Previous && !Previous->DisplayName.IsEmpty() ? Previous->DisplayName : SlotName;
	}
	Info.SaveTimeUtc = FDateTime::UtcNow();
	Info.PlayTimeSeconds = GetPlayTimeSeconds();
	Info.LevelName = GetWorldKey(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	Info.SchemaVersion = Settings->CurrentSchemaVersion;
	Info.BuildVersion = FApp::GetBuildVersion();
	Info.SaveSequence = ++SlotIndex->LastSaveSequence;
	if (NextThumbnail.Num() > 0)
	{
		Info.Thumbnail = MoveTemp(NextThumbnail);
		NextThumbnail.Reset();
	}

	// Write over the older copy; the newer one stays intact until this write has succeeded.
	const int32* Newest = SlotIndex->NewestCopy.Find(SlotName);
	const int32 Copy = Settings->bKeepBackup ? (Newest ? 1 - *Newest : 0) : 0;

	TWeakObjectPtr<UESaveSubsystem> WeakThis(this);
	UGameplayStatics::AsyncSaveGameToSlot(ActiveData, GetCopyFileName(SlotName, Copy), Settings->UserIndex,
		FAsyncSaveGameToSlotDelegate::CreateLambda([WeakThis, SlotName, Copy, WrittenInfo = Info, OnDone](const FString&, const int32, bool bSuccess)
		{
			if (UESaveSubsystem* This = WeakThis.Get())
			{
				This->HandleSaveWritten(SlotName, Copy, WrittenInfo, bSuccess, OnDone);
			}
			else if (OnDone)
			{
				OnDone(FEFResult::Cancelled(ESavePrivate::Source));
			}
		}));
}

void UESaveSubsystem::HandleSaveWritten(const FString& SlotName, int32 Copy, const FESaveSlotInfo& Info, bool bSuccess, FOnSaved OnDone)
{
	if (!bSuccess)
	{
		UE_LOG(LogExtendedSave, Warning, TEXT("Saving %s failed: the save game system could not write it."), *SlotName);
		if (OnDone)
		{
			OnDone(FEFResult::Failure(ESavePrivate::Source, ESaveResultCodes::WriteFailed, FString::Printf(TEXT("%s could not be written"), *SlotName)));
		}
		OnSlotSaved.Broadcast(SlotName, false);
		Broadcast(ESaveMessages::Saved, SlotName, false, false);
		FinishOperation();
		return;
	}

	ActiveSlotName = SlotName;
	SlotIndex->NewestCopy.Add(SlotName, Copy);
	if (FESaveSlotInfo* Existing = SlotIndex->FindSlot(SlotName))
	{
		*Existing = Info;
	}
	else
	{
		SlotIndex->Slots.Add(Info);
	}

	TWeakObjectPtr<UESaveSubsystem> WeakThis(this);
	WriteIndexAsync([WeakThis, SlotName, OnDone](bool bIndexWritten)
	{
		FEFResult Result = FEFResult::Success(ESavePrivate::Source);
		if (bIndexWritten)
		{
			UE_LOG(LogExtendedSave, Log, TEXT("Saved %s."), *SlotName);
		}
		else
		{
			// The index on disk names the newest copy. This session already uses the new one, but
			// until a later save rewrites the index, a restart loads the previous save of this slot.
			UE_LOG(LogExtendedSave, Warning, TEXT("%s was written, but the slot index could not be updated; after a restart the previous save would load."), *SlotName);
			Result = FEFResult::Failure(ESavePrivate::Source, ESaveResultCodes::WriteFailed, FString::Printf(TEXT("the slot index could not be updated after writing %s"), *SlotName));
		}
		if (OnDone)
		{
			OnDone(Result);
		}

		UESaveSubsystem* This = WeakThis.Get();
		if (!This)
		{
			return;
		}
		This->OnSlotSaved.Broadcast(SlotName, Result.IsSuccess());
		This->Broadcast(ESaveMessages::Saved, SlotName, Result.IsSuccess(), false);
		if (UESaveSettings::Get()->bUploadToCloud)
		{
			This->UploadSlotToCloud(SlotName);
		}
		This->FinishOperation();
	});
}

// Load

void UESaveSubsystem::LoadSlot(const FString& SlotName, EESaveLoadMode Mode, FOnLoaded OnDone)
{
	if (!IsValidSlotName(SlotName))
	{
		ESavePrivate::NextTick([OnDone = MoveTemp(OnDone), SlotName]()
		{
			if (OnDone)
			{
				OnDone(FEFResult::InvalidArgument(ESavePrivate::Source, FString::Printf(TEXT("'%s' is not a valid slot name"), *SlotName)), false);
			}
		});
		return;
	}

	const TSharedRef<FOnLoaded> Callback = MakeShared<FOnLoaded>(MoveTemp(OnDone));
	TWeakObjectPtr<UESaveSubsystem> WeakThis(this);

	FOperation Operation;
	Operation.Description = FString::Printf(TEXT("Load %s"), *SlotName);
	Operation.Run = [WeakThis, SlotName, Mode, Callback]()
	{
		if (UESaveSubsystem* This = WeakThis.Get())
		{
			This->DoLoad(SlotName, Mode, *Callback);
		}
	};
	Operation.Cancel = [Callback](const FEFResult& Result)
	{
		if (*Callback)
		{
			(*Callback)(Result, false);
		}
	};
	RunOrQueue(MoveTemp(Operation));
}

void UESaveSubsystem::DoLoad(const FString& SlotName, EESaveLoadMode Mode, FOnLoaded OnDone)
{
	const UESaveSettings* Settings = UESaveSettings::Get();
	const int32* Newest = SlotIndex->NewestCopy.Find(SlotName);

	TArray<int32> Copies;
	if (Settings->bKeepBackup)
	{
		Copies = Newest ? TArray<int32>({ *Newest, 1 - *Newest }) : TArray<int32>({ 0, 1 });
	}
	else
	{
		// Only copy 0 is written now, but the last save made while backups were on may be copy 1.
		Copies = Newest && *Newest == 1 ? TArray<int32>({ 1, 0 }) : TArray<int32>({ 0 });
	}
	Copies.RemoveAll([&SlotName, Settings](int32 Copy) { return !UGameplayStatics::DoesSaveGameExist(GetCopyFileName(SlotName, Copy), Settings->UserIndex); });

	if (Copies.Num() == 0)
	{
		if (OnDone)
		{
			OnDone(FEFResult::Failure(ESavePrivate::Source, ESaveResultCodes::NotFound, FString::Printf(TEXT("there is no save in slot %s"), *SlotName)), false);
		}
		OnSlotLoaded.Broadcast(SlotName, false);
		Broadcast(ESaveMessages::Loaded, SlotName, false, false);
		FinishOperation();
		return;
	}

	// Without an index entry neither copy is known to be newer, so both are read and compared.
	LoadCandidate = nullptr;
	LoadCopy(SlotName, Mode, Copies, /*bPickNewest*/ Newest == nullptr && Copies.Num() > 1, /*bFallback*/ false, MoveTemp(OnDone));
}

void UESaveSubsystem::LoadCopy(const FString& SlotName, EESaveLoadMode Mode, TArray<int32> Copies, bool bPickNewest, bool bFallback, FOnLoaded OnDone)
{
	const int32 Copy = Copies[0];
	TWeakObjectPtr<UESaveSubsystem> WeakThis(this);

	UGameplayStatics::AsyncLoadGameFromSlot(GetCopyFileName(SlotName, Copy), UESaveSettings::Get()->UserIndex,
		FAsyncLoadGameFromSlotDelegate::CreateLambda([WeakThis, SlotName, Mode, Copies, bPickNewest, bFallback, OnDone](const FString&, const int32, USaveGame* Loaded)
		{
			UESaveSubsystem* This = WeakThis.Get();
			if (!This)
			{
				if (OnDone)
				{
					OnDone(FEFResult::Cancelled(ESavePrivate::Source), false);
				}
				return;
			}

			UESaveGameData* Data = Cast<UESaveGameData>(Loaded);
			if (!Data)
			{
				UE_LOG(LogExtendedSave, Warning, TEXT("Copy %d of %s could not be read."), Copies[0], *SlotName);
			}

			TArray<int32> Remaining = Copies;
			Remaining.RemoveAt(0);

			if (bPickNewest && Remaining.Num() > 0)
			{
				// Keep this copy (if it was readable) and compare it with the next one.
				This->LoadCandidate = Data;
				This->LoadCopy(SlotName, Mode, Remaining, /*bPickNewest*/ true, /*bFallback*/ false, OnDone);
				return;
			}

			UESaveGameData* Earlier = This->LoadCandidate;
			This->LoadCandidate = nullptr;
			if (bPickNewest || Earlier)
			{
				// Two copies read without knowing which is newer: the higher save sequence wins.
				UESaveGameData* Chosen = (Data && (!Earlier || Data->Info.SaveSequence > Earlier->Info.SaveSequence)) ? Data : Earlier;
				if (Chosen)
				{
					// The copy that holds this save is the newest: the next save writes over the other one.
					This->SlotIndex->NewestCopy.Add(SlotName, Chosen == Data ? Copies[0] : 1 - Copies[0]);
					This->FinishLoad(SlotName, Mode, Chosen, /*bRestoredFromBackup*/ !Data || !Earlier, OnDone);
					return;
				}
			}
			else if (Data)
			{
				// The next save writes over the other copy: after a fallback that is the damaged one,
				// never the backup that now holds the only good save.
				This->SlotIndex->NewestCopy.Add(SlotName, Copies[0]);
				This->FinishLoad(SlotName, Mode, Data, /*bRestoredFromBackup*/ bFallback, OnDone);
				return;
			}
			else if (Remaining.Num() > 0)
			{
				// The newer copy is damaged: fall back to the backup.
				This->LoadCopy(SlotName, Mode, Remaining, /*bPickNewest*/ false, /*bFallback*/ true, OnDone);
				return;
			}

			if (OnDone)
			{
				OnDone(FEFResult::Failure(ESavePrivate::Source, ESaveResultCodes::Corrupt, FString::Printf(TEXT("no copy of %s could be read"), *SlotName)), false);
			}
			This->OnSlotLoaded.Broadcast(SlotName, false);
			This->Broadcast(ESaveMessages::Loaded, SlotName, false, false);
			This->FinishOperation();
		}));
}

bool UESaveSubsystem::RunMigrations(UESaveGameData* Data, FEFResult& OutFailure) const
{
	const UESaveSettings* Settings = UESaveSettings::Get();
	const int32 Current = Settings->CurrentSchemaVersion;
	int32 Version = FMath::Max(Data->Info.SchemaVersion, 1);

	if (Version > Current)
	{
		OutFailure = FEFResult::Failure(ESavePrivate::Source, ESaveResultCodes::NewerVersion,
			FString::Printf(TEXT("the save has schema version %d; this build reads up to %d"), Version, Current));
		return false;
	}

	TArray<UESaveMigration*> Steps;
	for (const TSoftClassPtr<UESaveMigration>& MigrationClass : Settings->Migrations)
	{
		if (UClass* Class = EFAssets::Resolve(MigrationClass, TEXT("UESaveSubsystem::RunMigrations")))
		{
			if (UESaveMigration* Step = Class->GetDefaultObject<UESaveMigration>())
			{
				Steps.Add(Step);
			}
		}
		else if (!MigrationClass.IsNull())
		{
			UE_LOG(LogExtendedSave, Warning, TEXT("The save migration %s could not be loaded."), *MigrationClass.ToString());
		}
	}

	while (Version < Current)
	{
		UESaveMigration** Step = Steps.FindByPredicate([Version](const UESaveMigration* Candidate) { return Candidate->FromVersion == Version; });
		if (Step)
		{
			UE_LOG(LogExtendedSave, Log, TEXT("Migrating %s from schema version %d with %s."), *Data->Info.SlotName, Version, *(*Step)->GetClass()->GetName());
			if (!(*Step)->Migrate(Data))
			{
				OutFailure = FEFResult::Failure(ESavePrivate::Source, ESaveResultCodes::MigrationFailed,
					FString::Printf(TEXT("%s could not upgrade the save from schema version %d"), *(*Step)->GetClass()->GetName(), Version));
				return false;
			}
		}
		++Version;
	}
	Data->Info.SchemaVersion = Current;
	return true;
}

void UESaveSubsystem::FinishLoad(const FString& SlotName, EESaveLoadMode Mode, UESaveGameData* Data, bool bRestoredFromBackup, FOnLoaded OnDone)
{
	FEFResult Failure;
	if (!RunMigrations(Data, Failure))
	{
		UE_LOG(LogExtendedSave, Warning, TEXT("Loading %s failed: %s"), *SlotName, *Failure.ToString());
		if (OnDone)
		{
			OnDone(Failure, false);
		}
		OnSlotLoaded.Broadcast(SlotName, false);
		Broadcast(ESaveMessages::Loaded, SlotName, false, false);
		FinishOperation();
		return;
	}

	if (bRestoredFromBackup)
	{
		UE_LOG(LogExtendedSave, Warning, TEXT("The newest copy of %s could not be read; the backup from %s was loaded."), *SlotName, *Data->Info.SaveTimeUtc.ToString());
	}

	ActiveData = Data;
	ActiveSlotName = SlotName;
	PlayTimeBaseSeconds = Data->Info.PlayTimeSeconds;
	PlayTimeStartSeconds = FPlatformTime::Seconds();

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const bool bOpenLevel = Mode == EESaveLoadMode::OpenSavedLevel && !Data->Info.LevelName.IsEmpty() && Data->Info.LevelName != GetWorldKey(World);

	// Objects that are not actors are restored in every mode. A copy: OnAfterLoad may register or
	// unregister saveables.
	const TArray<TWeakObjectPtr<UObject>> Saveables = LiveSaveables;
	for (const TWeakObjectPtr<UObject>& Weak : Saveables)
	{
		if (UObject* Object = Weak.Get())
		{
			if (const FESaveRecord* Record = ActiveData->Records.Find(ESavePrivate::GetObjectSaveId(Object)))
			{
				ApplyRecordToObject(Object, *Record);
			}
		}
	}

	if (bOpenLevel)
	{
		// The current level's actors must not write their state over the save as they unload.
		bIgnoreUnloads = true;
		UE_LOG(LogExtendedSave, Log, TEXT("Loaded %s; opening %s."), *SlotName, *Data->Info.LevelName);
		UGameplayStatics::OpenLevel(GetGameInstance(), FName(*Data->Info.LevelName));
	}
	else
	{
		ApplyToCurrentWorld();
		UE_LOG(LogExtendedSave, Log, TEXT("Loaded %s."), *SlotName);
	}

	if (OnDone)
	{
		OnDone(FEFResult::Success(ESavePrivate::Source), bRestoredFromBackup);
	}
	OnSlotLoaded.Broadcast(SlotName, true);
	Broadcast(ESaveMessages::Loaded, SlotName, true, bRestoredFromBackup);
	FinishOperation();
}

// Capture and apply

TArray<UESaveableComponent*> UESaveSubsystem::GetLiveComponents() const
{
	TArray<UESaveableComponent*> Result;
	for (const TWeakObjectPtr<UESaveableComponent>& Weak : LiveComponents)
	{
		UESaveableComponent* Component = Weak.Get();
		if (Component && Component->GetOwner() && Component->GetWorld() && Component->GetWorld()->IsGameWorld())
		{
			Result.Add(Component);
		}
	}
	return Result;
}

void UESaveSubsystem::CaptureWorldState()
{
	// OnSaving and OnBeforeSave run game code that may destroy saveables, register or unregister them
	// and add or remove records: records are captured into a local, never through a reference into
	// Records, and the lists walked are copies.
	for (UESaveableComponent* Component : GetLiveComponents())
	{
		if (!IsValid(Component) || !IsValid(Component->GetOwner()))
		{
			continue;  // destroyed by an earlier callback; its destruction is recorded
		}
		const FName Id = Component->GetResolvedSaveId();
		if (!Id.IsNone())
		{
			FESaveRecord Record;
			CaptureComponent(Component, Record);
			if (IsValid(Component))  // an actor that destroyed itself while saving is recorded already
			{
				ActiveData->Records.Add(Id, MoveTemp(Record));
			}
		}
	}

	const TArray<TWeakObjectPtr<UObject>> Saveables = LiveSaveables;
	for (const TWeakObjectPtr<UObject>& Weak : Saveables)
	{
		if (UObject* Object = Weak.Get())
		{
			const FName Id = ESavePrivate::GetObjectSaveId(Object);
			if (!Id.IsNone())
			{
				FESaveRecord Record;
				CaptureObject(Object, Record);
				ActiveData->Records.Add(Id, MoveTemp(Record));
			}
		}
	}
}

void UESaveSubsystem::CaptureComponent(UESaveableComponent* Component, FESaveRecord& OutRecord)
{
	AActor* Owner = Component->GetOwner();
	Component->OnSaving.Broadcast();
	ESavePrivate::NotifyBeforeSave(Owner);

	OutRecord.Id = Component->GetResolvedSaveId();
	OutRecord.World = GetWorldKey(Owner->GetWorld());
	OutRecord.bSpawned = !Component->IsPlacedInLevel();
	OutRecord.ActorClass = OutRecord.bSpawned ? FSoftClassPath(Owner->GetClass()) : FSoftClassPath();
	OutRecord.bDestroyed = false;
	OutRecord.bHasTransform = Component->bSaveTransform;
	OutRecord.Transform = Owner->GetActorTransform();
	ESaveSerialization::Capture(Owner, OutRecord.Properties);

	OutRecord.Components.Reset();
	if (Component->bSaveComponents)
	{
		TInlineComponentArray<UActorComponent*> OwnerComponents(Owner);
		for (UActorComponent* Other : OwnerComponents)
		{
			if (ESavePrivate::ShouldSaveComponent(Other))
			{
				ESavePrivate::NotifyBeforeSave(Other);
				FESaveComponentRecord& ComponentRecord = OutRecord.Components.AddDefaulted_GetRef();
				ComponentRecord.Name = Other->GetFName();
				ESaveSerialization::Capture(Other, ComponentRecord.Properties);
			}
		}
	}
}

void UESaveSubsystem::CaptureObject(UObject* Object, FESaveRecord& OutRecord)
{
	ESavePrivate::NotifyBeforeSave(Object);
	OutRecord.Id = ESavePrivate::GetObjectSaveId(Object);
	OutRecord.World.Reset();
	OutRecord.bSpawned = false;
	OutRecord.bDestroyed = false;
	OutRecord.bHasTransform = false;
	OutRecord.Components.Reset();
	ESaveSerialization::Capture(Object, OutRecord.Properties);
}

void UESaveSubsystem::ApplyRecordToComponent(UESaveableComponent* Component, const FESaveRecord& Record)
{
	AActor* Owner = Component->GetOwner();
	if (!Owner)
	{
		return;
	}

	if (Record.bDestroyed)
	{
		if (Component->bRememberDestruction)
		{
			Owner->Destroy();
		}
		return;
	}

	if (Record.bHasTransform && Component->bSaveTransform)
	{
		Owner->SetActorTransform(Record.Transform, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
	}
	ESaveSerialization::Restore(Owner, Record.Properties);

	if (Component->bSaveComponents && Record.Components.Num() > 0)
	{
		TInlineComponentArray<UActorComponent*> OwnerComponents(Owner);
		for (const FESaveComponentRecord& ComponentRecord : Record.Components)
		{
			UActorComponent* const* Match = OwnerComponents.FindByPredicate([&ComponentRecord](const UActorComponent* Candidate)
			{
				return Candidate && Candidate->GetFName() == ComponentRecord.Name;
			});
			if (Match)
			{
				ESaveSerialization::Restore(*Match, ComponentRecord.Properties);
				ESavePrivate::NotifyAfterLoad(*Match);
			}
		}
	}

	ESavePrivate::NotifyAfterLoad(Owner);
	Component->OnLoaded.Broadcast();
}

void UESaveSubsystem::ApplyRecordToObject(UObject* Object, const FESaveRecord& Record)
{
	ESaveSerialization::Restore(Object, Record.Properties);
	ESavePrivate::NotifyAfterLoad(Object);
}

void UESaveSubsystem::ApplyToCurrentWorld()
{
	TGuardValue<bool> ApplyingGuard(bApplying, true);

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	for (UESaveableComponent* Component : GetLiveComponents())
	{
		if (!IsValid(Component) || !IsValid(Component->GetOwner()))
		{
			continue;  // destroyed by an earlier record
		}
		if (const FESaveRecord* Record = ActiveData->Records.Find(Component->GetResolvedSaveId()))
		{
			const FESaveRecord RecordCopy = *Record;  // OnAfterLoad may change the records
			ApplyRecordToComponent(Component, RecordCopy);
		}
		else if (!Component->IsPlacedInLevel())
		{
			// Spawned after the save was made: it was not there then.
			Component->GetOwner()->Destroy();
		}
	}
	RespawnMissingActors(World);
}

void UESaveSubsystem::RespawnMissingActors(UWorld* World)
{
	if (!World || !ActiveData)
	{
		return;
	}

	const FString WorldKey = GetWorldKey(World);
	TSet<FName> LiveIds;
	for (UESaveableComponent* Component : GetLiveComponents())
	{
		LiveIds.Add(Component->GetResolvedSaveId());
	}

	TArray<FESaveRecord> ToSpawn;
	for (const TPair<FName, FESaveRecord>& Pair : ActiveData->Records)
	{
		const FESaveRecord& Record = Pair.Value;
		if (Record.bSpawned && !Record.bDestroyed && Record.World == WorldKey && !LiveIds.Contains(Record.Id))
		{
			ToSpawn.Add(Record);
		}
	}

	for (const FESaveRecord& Record : ToSpawn)
	{
		UClass* Class = Record.ActorClass.TryLoadClass<AActor>();
		if (!Class)
		{
			UE_LOG(LogExtendedSave, Warning, TEXT("The saved actor %s (%s) could not be spawned: its class is gone."), *Record.Id.ToString(), *Record.ActorClass.ToString());
			continue;
		}

		AActor* Actor = World->SpawnActorDeferred<AActor>(Class, Record.Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Actor)
		{
			continue;
		}
		UESaveableComponent* Saveable = Actor->FindComponentByClass<UESaveableComponent>();
		if (Saveable)
		{
			Saveable->SetRuntimeSaveId(Record.Id);
		}
		Actor->FinishSpawning(Record.Transform);
		if (!IsValid(Actor))
		{
			continue;
		}

		if (!Saveable)
		{
			// A Saveable component added in the actor's Blueprint is only created by FinishSpawning,
			// and may already have begun play under a generated id: give it the saved id and restore it.
			Saveable = Actor->FindComponentByClass<UESaveableComponent>();
			if (Saveable)
			{
				Saveable->SetRuntimeSaveId(Record.Id);
				ApplyToComponent(Saveable);
			}
			else
			{
				UE_LOG(LogExtendedSave, Warning, TEXT("%s was spawned from a save but has no Saveable component; its state cannot be restored."), *Class->GetName());
			}
			continue;
		}

		// A world that has begun play restores it in BeginPlay; otherwise do it now.
		if (!Saveable->HasBegunPlay())
		{
			ApplyToComponent(Saveable);
		}
	}
}

// Component and object registration

void UESaveSubsystem::RegisterComponent(UESaveableComponent* Component)
{
	if (Component)
	{
		LiveComponents.Add(Component);
	}
}

void UESaveSubsystem::UnregisterComponent(UESaveableComponent* Component)
{
	LiveComponents.Remove(Component);
}

void UESaveSubsystem::ApplyToComponent(UESaveableComponent* Component)
{
	if (!Component || !ActiveData)
	{
		return;
	}
	if (const FESaveRecord* Record = ActiveData->Records.Find(Component->GetResolvedSaveId()))
	{
		const FESaveRecord RecordCopy = *Record;  // applying may change the records (a destroyed actor)
		ApplyRecordToComponent(Component, RecordCopy);
	}
}

void UESaveSubsystem::NotifyComponentDestroyed(UESaveableComponent* Component)
{
	UnregisterComponent(Component);
	if (!Component || !ActiveData || bApplying)
	{
		return;
	}

	const FName Id = Component->GetResolvedSaveId();
	if (Id.IsNone())
	{
		return;
	}

	if (!Component->IsPlacedInLevel())
	{
		// A spawned actor that is gone is simply not spawned again.
		ActiveData->Records.Remove(Id);
	}
	else if (Component->bRememberDestruction)
	{
		FESaveRecord& Record = ActiveData->Records.FindOrAdd(Id);
		Record = FESaveRecord();
		Record.Id = Id;
		Record.World = GetWorldKey(Component->GetWorld());
		Record.bDestroyed = true;
	}
}

void UESaveSubsystem::NotifyComponentUnloaded(UESaveableComponent* Component)
{
	if (Component && ActiveData && !bIgnoreUnloads && UESaveSettings::Get()->bKeepStateOfUnloadedLevels && Component->GetOwner())
	{
		const FName Id = Component->GetResolvedSaveId();
		if (!Id.IsNone())
		{
			FESaveRecord Record;  // not a reference into Records: OnSaving and OnBeforeSave may change them
			CaptureComponent(Component, Record);
			ActiveData->Records.Add(Id, MoveTemp(Record));
		}
	}
	UnregisterComponent(Component);
}

void UESaveSubsystem::RegisterSaveable(UObject* Object)
{
	if (!Object)
	{
		return;
	}
	if (!Object->GetClass()->ImplementsInterface(UESaveable::StaticClass()))
	{
		UE_LOG(LogExtendedSave, Warning, TEXT("RegisterSaveable: %s does not implement the Saveable interface."), *Object->GetName());
		return;
	}
	const FName Id = ESavePrivate::GetObjectSaveId(Object);
	if (Id.IsNone())
	{
		UE_LOG(LogExtendedSave, Warning, TEXT("RegisterSaveable: %s returns no save id; it is not saved."), *Object->GetName());
		return;
	}

	LiveSaveables.RemoveAll([Object](const TWeakObjectPtr<UObject>& Weak) { return !Weak.IsValid() || Weak.Get() == Object; });
	LiveSaveables.Add(Object);

	if (const FESaveRecord* Record = ActiveData->Records.Find(Id))
	{
		ApplyRecordToObject(Object, *Record);
	}
}

void UESaveSubsystem::UnregisterSaveable(UObject* Object)
{
	if (Object && ActiveData)
	{
		const FName Id = ESavePrivate::GetObjectSaveId(Object);
		if (!Id.IsNone())
		{
			FESaveRecord Record;  // not a reference into Records: OnBeforeSave may change them
			CaptureObject(Object, Record);
			ActiveData->Records.Add(Id, MoveTemp(Record));
		}
	}
	LiveSaveables.RemoveAll([Object](const TWeakObjectPtr<UObject>& Weak) { return !Weak.IsValid() || Weak.Get() == Object; });
}

// Values and metadata

void UESaveSubsystem::SetValue(FName Key, const FString& Value)
{
	if (!Key.IsNone())
	{
		ActiveData->Values.Add(Key, Value);
	}
}

bool UESaveSubsystem::GetValue(FName Key, FString& OutValue) const
{
	if (const FString* Found = ActiveData ? ActiveData->Values.Find(Key) : nullptr)
	{
		OutValue = *Found;
		return true;
	}
	OutValue.Reset();
	return false;
}

void UESaveSubsystem::RemoveValue(FName Key)
{
	ActiveData->Values.Remove(Key);
}

void UESaveSubsystem::SetNextSaveThumbnail(const TArray<uint8>& ImageBytes)
{
	NextThumbnail = ImageBytes;
}

void UESaveSubsystem::SetSlotExtra(const FString& Key, const FString& Value)
{
	if (Key.IsEmpty())
	{
		return;
	}
	if (Value.IsEmpty())
	{
		ActiveData->Info.Extra.Remove(Key);
	}
	else
	{
		ActiveData->Info.Extra.Add(Key, Value);
	}
}

// Autosave

void UESaveSubsystem::RequestAutosave()
{
	if (IsAutosaveBlocked() || bOperationRunning)
	{
		UE_LOG(LogExtendedSave, Verbose, TEXT("Autosave skipped: %s."), IsAutosaveBlocked() ? TEXT("blocked") : TEXT("a save or load is running"));
		return;
	}
	SaveSlot(UESaveSettings::Get()->AutosaveSlotName);
}

void UESaveSubsystem::SetAutosaveBlocked(FName Reason, bool bBlocked)
{
	const FName Key = Reason.IsNone() ? FName(TEXT("Default")) : Reason;
	if (bBlocked)
	{
		AutosaveBlockers.Add(Key);
	}
	else
	{
		AutosaveBlockers.Remove(Key);
	}
}

bool UESaveSubsystem::TickAutosave(float DeltaTime)
{
	// Only while a game world is running: never from the main menu's empty world.
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (World && World->IsGameWorld() && (LiveComponents.Num() > 0 || LiveSaveables.Num() > 0))
	{
		RequestAutosave();
	}
	return true;
}

void UESaveSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	bIgnoreUnloads = false;
	RespawnMissingActors(LoadedWorld);
}

// Messages

void UESaveSubsystem::Broadcast(const FNativeGameplayTag& Channel, const FString& SlotName, bool bSuccess, bool bRestoredFromBackup)
{
	if (!UESaveSettings::Get()->bBroadcastMessages)
	{
		return;
	}
	if (UEFMessageSubsystem* Messages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEFMessageSubsystem>() : nullptr)
	{
		FESaveMessage Message;
		Message.SlotName = SlotName;
		Message.bSuccess = bSuccess;
		Message.bRestoredFromBackup = bRestoredFromBackup;
		Messages->BroadcastMessage(Channel.GetTag(), Message);
	}
}

// Cloud

void UESaveSubsystem::UploadSlotToCloud(const FString& SlotName, FOnSaved OnDone)
{
	// The name becomes a path below the save folder and a cloud file name: refuse anything else.
	if (!IsValidSlotName(SlotName))
	{
		ESavePrivate::NextTick([OnDone = MoveTemp(OnDone), SlotName]()
		{
			if (OnDone)
			{
				OnDone(FEFResult::InvalidArgument(ESavePrivate::Source, FString::Printf(TEXT("'%s' is not a valid slot name"), *SlotName)));
			}
		});
		return;
	}

	UEFPlatformSubsystem* Platform = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEFPlatformSubsystem>() : nullptr;
	if (!Platform || !Platform->IsFeatureAvailable(EEFPlatformFeature::CloudStorage))
	{
		ESavePrivate::NextTick([OnDone = MoveTemp(OnDone)]()
		{
			if (OnDone)
			{
				OnDone(FEFResult::NotAvailable(ESavePrivate::Source, TEXT("no platform provider serves cloud storage")));
			}
		});
		return;
	}

	// The active save is already in memory; any other slot is read from disk.
	USaveGame* Data = ActiveData && ActiveSlotName == SlotName ? ActiveData.Get() : nullptr;
	if (!Data)
	{
		const int32* Newest = SlotIndex->NewestCopy.Find(SlotName);
		Data = UGameplayStatics::LoadGameFromSlot(GetCopyFileName(SlotName, Newest ? *Newest : 0), UESaveSettings::Get()->UserIndex);
	}

	TArray<uint8> Bytes;
	if (!Data || !UGameplayStatics::SaveGameToMemory(Data, Bytes))
	{
		ESavePrivate::NextTick([OnDone = MoveTemp(OnDone), SlotName]()
		{
			if (OnDone)
			{
				OnDone(FEFResult::Failure(ESavePrivate::Source, ESaveResultCodes::NotFound, FString::Printf(TEXT("%s could not be read for upload"), *SlotName)));
			}
		});
		return;
	}

	Platform->WriteCloudFile(GetCloudFileName(SlotName), Bytes, [SlotName, OnDone = MoveTemp(OnDone)](const FEFResult& Result)
	{
		UE_LOG(LogExtendedSave, Log, TEXT("Cloud upload of %s: %s"), *SlotName, *Result.ToString());
		if (OnDone)
		{
			OnDone(Result);
		}
	});
}

void UESaveSubsystem::SyncSlotFromCloud(const FString& SlotName, FOnCloudSynced OnDone)
{
	if (!IsValidSlotName(SlotName))
	{
		ESavePrivate::NextTick([OnDone = MoveTemp(OnDone), SlotName]()
		{
			if (OnDone)
			{
				OnDone(FEFResult::InvalidArgument(ESavePrivate::Source, FString::Printf(TEXT("'%s' is not a valid slot name"), *SlotName)), false);
			}
		});
		return;
	}

	const TSharedRef<FOnCloudSynced> Callback = MakeShared<FOnCloudSynced>(MoveTemp(OnDone));
	TWeakObjectPtr<UESaveSubsystem> WeakThis(this);

	FOperation Operation;
	Operation.Description = FString::Printf(TEXT("Cloud sync %s"), *SlotName);
	Operation.Run = [WeakThis, SlotName, Callback]()
	{
		if (UESaveSubsystem* This = WeakThis.Get())
		{
			This->DoSyncFromCloud(SlotName, *Callback);
		}
	};
	Operation.Cancel = [Callback](const FEFResult& Result)
	{
		if (*Callback)
		{
			(*Callback)(Result, false);
		}
	};
	RunOrQueue(MoveTemp(Operation));
}

void UESaveSubsystem::DoSyncFromCloud(const FString& SlotName, FOnCloudSynced OnDone)
{
	UEFPlatformSubsystem* Platform = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEFPlatformSubsystem>() : nullptr;
	if (!Platform)
	{
		if (OnDone)
		{
			OnDone(FEFResult::NotAvailable(ESavePrivate::Source, TEXT("the platform subsystem")), false);
		}
		FinishOperation();
		return;
	}

	TWeakObjectPtr<UESaveSubsystem> WeakThis(this);
	Platform->ReadCloudFile(GetCloudFileName(SlotName), [WeakThis, SlotName, OnDone](const FEFResult& Result, const TArray<uint8>& Bytes)
	{
		UESaveSubsystem* This = WeakThis.Get();
		if (!This)
		{
			if (OnDone)
			{
				OnDone(FEFResult::Cancelled(ESavePrivate::Source), false);
			}
			return;
		}

		auto Finish = [This, &OnDone](const FEFResult& Outcome, bool bDownloaded)
		{
			if (OnDone)
			{
				OnDone(Outcome, bDownloaded);
			}
			This->FinishOperation();
		};

		if (!Result.IsSuccess())
		{
			// Nothing in the cloud yet is not an error.
			Finish(Result.HasCode(EFPlatformResultCodes::NotFound) ? FEFResult::Success(ESavePrivate::Source) : Result, false);
			return;
		}

		UESaveGameData* Cloud = Cast<UESaveGameData>(UGameplayStatics::LoadGameFromMemory(Bytes));
		if (!Cloud)
		{
			Finish(FEFResult::Failure(ESavePrivate::Source, ESaveResultCodes::Corrupt, FString::Printf(TEXT("the cloud copy of %s could not be read"), *SlotName)), false);
			return;
		}

		const UESaveSettings* Settings = UESaveSettings::Get();
		const FESaveSlotInfo* Local = This->SlotIndex->FindSlot(SlotName);
		const bool bTakeCloud = !Local
			|| Settings->CloudConflictPolicy == EESaveCloudConflictPolicy::PreferCloud
			|| (Settings->CloudConflictPolicy == EESaveCloudConflictPolicy::NewestWins && Cloud->Info.SaveTimeUtc > Local->SaveTimeUtc);

		if (!bTakeCloud)
		{
			const bool bUploadLocal = Settings->CloudConflictPolicy == EESaveCloudConflictPolicy::PreferLocal || Local->SaveTimeUtc > Cloud->Info.SaveTimeUtc;
			if (bUploadLocal)
			{
				This->UploadSlotToCloud(SlotName);
			}
			Finish(FEFResult::Success(ESavePrivate::Source), false);
			return;
		}

		// Write the cloud copy over the older local copy, like a save does.
		const int32* Newest = This->SlotIndex->NewestCopy.Find(SlotName);
		const int32 Copy = Settings->bKeepBackup ? (Newest ? 1 - *Newest : 0) : 0;
		Cloud->Info.SlotName = SlotName;
		UGameplayStatics::AsyncSaveGameToSlot(Cloud, GetCopyFileName(SlotName, Copy), Settings->UserIndex,
			FAsyncSaveGameToSlotDelegate::CreateLambda([WeakThis, SlotName, Copy, Info = Cloud->Info, OnDone](const FString&, const int32, bool bWritten)
			{
				UESaveSubsystem* Self = WeakThis.Get();
				if (!Self)
				{
					if (OnDone)
					{
						OnDone(FEFResult::Cancelled(ESavePrivate::Source), false);
					}
					return;
				}
				if (!bWritten)
				{
					if (OnDone)
					{
						OnDone(FEFResult::Failure(ESavePrivate::Source, ESaveResultCodes::WriteFailed, FString::Printf(TEXT("the cloud copy of %s could not be written"), *SlotName)), false);
					}
					Self->FinishOperation();
					return;
				}

				Self->SlotIndex->NewestCopy.Add(SlotName, Copy);
				if (FESaveSlotInfo* Existing = Self->SlotIndex->FindSlot(SlotName))
				{
					*Existing = Info;
				}
				else
				{
					Self->SlotIndex->Slots.Add(Info);
				}
				Self->WriteIndexAsync([WeakThis, SlotName, OnDone](bool)
				{
					UE_LOG(LogExtendedSave, Log, TEXT("Downloaded %s from the cloud."), *SlotName);
					if (OnDone)
					{
						OnDone(FEFResult::Success(ESavePrivate::Source), true);
					}
					if (UESaveSubsystem* Owner = WeakThis.Get())
					{
						Owner->FinishOperation();
					}
				});
			}));
	});
}
