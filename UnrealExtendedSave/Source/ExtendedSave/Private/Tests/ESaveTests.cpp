// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ESaveSettings.h"
#include "ESaveSubsystem.h"
#include "ESaveTestTypes.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Messages/EFMessageSubsystem.h"
#include "Misc/Guid.h"
#include "Serialization/MemoryWriter.h"
#include "Testing/EFTestHelpers.h"

namespace ESaveTestPrivate
{
	/** Save settings for a test, restored afterwards. Uses its own index slot so real saves are never touched. */
	struct FScopedSaveTestSettings
	{
		FScopedSaveTestSettings()
		{
			UESaveSettings* Settings = GetMutableDefault<UESaveSettings>();
			SavedSchema = Settings->CurrentSchemaVersion;
			SavedMigrations = Settings->Migrations;
			bSavedKeepBackup = Settings->bKeepBackup;
			SavedIndexSlot = Settings->IndexSlotName;
			SavedAutosaveInterval = Settings->AutosaveIntervalSeconds;
			bSavedUpload = Settings->bUploadToCloud;

			Settings->CurrentSchemaVersion = 1;
			Settings->Migrations.Reset();
			Settings->bKeepBackup = true;
			Settings->IndexSlotName = TEXT("ESaveTest_Index_") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
			Settings->AutosaveIntervalSeconds = 0.f;
			Settings->bUploadToCloud = false;
			IndexSlot = Settings->IndexSlotName;
		}

		~FScopedSaveTestSettings()
		{
			const int32 UserIndex = GetDefault<UESaveSettings>()->UserIndex;
			UGameplayStatics::DeleteGameInSlot(IndexSlot, UserIndex);

			UESaveSettings* Settings = GetMutableDefault<UESaveSettings>();
			Settings->CurrentSchemaVersion = SavedSchema;
			Settings->Migrations = SavedMigrations;
			Settings->bKeepBackup = bSavedKeepBackup;
			Settings->IndexSlotName = SavedIndexSlot;
			Settings->AutosaveIntervalSeconds = SavedAutosaveInterval;
			Settings->bUploadToCloud = bSavedUpload;
		}

		FString IndexSlot;
		int32 SavedSchema = 1;
		TArray<TSoftClassPtr<UESaveMigration>> SavedMigrations;
		bool bSavedKeepBackup = true;
		FString SavedIndexSlot;
		float SavedAutosaveInterval = 0.f;
		bool bSavedUpload = false;
	};

	FString MakeTestSlotName()
	{
		return TEXT("ESaveTest ") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESaveRoundTripTest, "UnrealExtendedSave.RoundTripAndBackup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESaveRoundTripTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		TSharedPtr<ESaveTestPrivate::FScopedSaveTestSettings> Settings;
		TSharedPtr<EFTest::FScopedGameInstance> Game;
		FString Slot;
		TWeakObjectPtr<AESaveTestActor> Actor;
		TWeakObjectPtr<AESaveTestActor> LateActor;
		TStrongObjectPtr<UESaveTestObject> Object;
		FEFMessageListenerHandle SavedListener;
		int32 SavedMessages = 0;
		TOptional<FEFResult> FirstSave;
		TOptional<FEFResult> Load;
		bool bLoadUsedBackup = true;
		TOptional<FEFResult> SecondSave;
		TOptional<FEFResult> BackupLoad;
		bool bBackupLoadUsedBackup = false;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	State->Settings = MakeShared<ESaveTestPrivate::FScopedSaveTestSettings>();
	State->Game = MakeShared<EFTest::FScopedGameInstance>();
	State->Slot = ESaveTestPrivate::MakeTestSlotName();

	UESaveSubsystem* Save = State->Game->GetSubsystem<UESaveSubsystem>();
	UEFMessageSubsystem* Messages = State->Game->GetSubsystem<UEFMessageSubsystem>();
	UWorld* World = State->Game->GetWorld();
	if (!TestNotNull(TEXT("the save subsystem exists"), Save) || !TestNotNull(TEXT("the test world exists"), World) || !TestNotNull(TEXT("messages exist"), Messages))
	{
		return false;
	}

	AESaveTestActor* Actor = World->SpawnActor<AESaveTestActor>();
	if (!TestNotNull(TEXT("the test actor spawns"), Actor))
	{
		return false;
	}
	State->Actor = Actor;
	Actor->Health = 42;
	Actor->Title = TEXT("Knight");
	Actor->Waypoint = FVector(1, 2, 3);
	Actor->Counter->Count = 5;
	Actor->SetActorLocation(FVector(100, 0, 0));
	TestFalse(TEXT("a spawned actor is not placed"), Actor->Saveable->IsPlacedInLevel());

	State->Object.Reset(NewObject<UESaveTestObject>());
	State->Object->Score = 7;
	State->Object->Unlocked = { TEXT("a"), TEXT("b") };
	Save->RegisterSaveable(State->Object.Get());
	Save->SetValue(TEXT("Chapter"), TEXT("3"));

	State->SavedListener = Messages->RegisterListener<FESaveMessage>(ESaveMessages::Saved.GetTag(), [State](FGameplayTag, const FESaveMessage& Message)
	{
		if (Message.SlotName == State->Slot && Message.bSuccess)
		{
			++State->SavedMessages;
		}
	});

	Save->SaveSlot(State->Slot, [State](const FEFResult& Result) { State->FirstSave = Result; }, TEXT("Chapter 3"));

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the first save"), [State]() { return State->FirstSave.IsSet(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		UESaveSubsystem* LatentSave = State->Game->GetSubsystem<UESaveSubsystem>();
		TestTrue(TEXT("the slot is saved"), State->FirstSave->IsSuccess());
		TestEqual(TEXT("Saved was announced on the message bus"), State->SavedMessages, 1);
		TestTrue(TEXT("the saveable object was told before saving"), State->Object->BeforeSaveCalls >= 1);
		TestTrue(TEXT("the slot exists"), LatentSave->DoesSlotExist(State->Slot));

		const TArray<FESaveSlotInfo> Slots = LatentSave->GetSlots();
		const FESaveSlotInfo* Info = Slots.FindByPredicate([State](const FESaveSlotInfo& Candidate) { return Candidate.SlotName == State->Slot; });
		if (TestNotNull(TEXT("the slot is listed"), Info))
		{
			TestEqual(TEXT("with its display name"), Info->DisplayName, FString(TEXT("Chapter 3")));
			TestEqual(TEXT("and the schema version"), Info->SchemaVersion, 1);
			TestTrue(TEXT("and a save time"), Info->SaveTimeUtc.GetTicks() > 0);
		}

		// Change everything, and spawn an actor the save does not know about.
		AESaveTestActor* LiveActor = State->Actor.Get();
		LiveActor->Health = 1;
		LiveActor->Title.Reset();
		LiveActor->Waypoint = FVector::ZeroVector;
		LiveActor->NotSaved = 99;
		LiveActor->Counter->Count = 0;
		LiveActor->SetActorLocation(FVector::ZeroVector);
		State->Object->Score = 0;
		State->Object->Unlocked.Reset();
		LatentSave->RemoveValue(TEXT("Chapter"));
		State->LateActor = State->Game->GetWorld()->SpawnActor<AESaveTestActor>();

		LatentSave->LoadSlot(State->Slot, EESaveLoadMode::ApplyToCurrentWorld, [State](const FEFResult& Result, bool bBackup)
		{
			State->Load = Result;
			State->bLoadUsedBackup = bBackup;
		});
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the load"), [State]() { return State->Load.IsSet(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		UESaveSubsystem* LatentSave = State->Game->GetSubsystem<UESaveSubsystem>();
		TestTrue(TEXT("the slot loads"), State->Load->IsSuccess());
		TestFalse(TEXT("from its newest copy"), State->bLoadUsedBackup);

		AESaveTestActor* LiveActor = State->Actor.Get();
		if (TestNotNull(TEXT("the saved actor still exists"), LiveActor))
		{
			TestEqual(TEXT("an int property is restored"), LiveActor->Health, 42);
			TestEqual(TEXT("a string property is restored"), LiveActor->Title, FString(TEXT("Knight")));
			TestTrue(TEXT("a struct property is restored"), LiveActor->Waypoint.Equals(FVector(1, 2, 3)));
			TestEqual(TEXT("a property not marked SaveGame keeps its value"), LiveActor->NotSaved, 99);
			TestEqual(TEXT("a component's SaveGame property is restored"), LiveActor->Counter->Count, 5);
			TestTrue(TEXT("the transform is restored"), LiveActor->GetActorLocation().Equals(FVector(100, 0, 0)));
		}
		TestEqual(TEXT("the object's score is restored"), State->Object->Score, 7);
		TestEqual(TEXT("the object's array is restored"), State->Object->Unlocked.Num(), 2);
		TestTrue(TEXT("the object was told after loading"), State->Object->AfterLoadCalls >= 1);

		FString Chapter;
		TestTrue(TEXT("values are restored"), LatentSave->GetValue(TEXT("Chapter"), Chapter) && Chapter == TEXT("3"));
		TestTrue(TEXT("an actor spawned after the save is removed"), !State->LateActor.IsValid() || State->LateActor->IsActorBeingDestroyed());

		// Save again (to the other copy), then damage that newer copy.
		State->Actor->Health = 55;
		LatentSave->SaveSlot(State->Slot, [State](const FEFResult& Result) { State->SecondSave = Result; });
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the second save"), [State]() { return State->SecondSave.IsSet(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		UESaveSubsystem* LatentSave = State->Game->GetSubsystem<UESaveSubsystem>();
		const int32 UserIndex = GetDefault<UESaveSettings>()->UserIndex;
		TestTrue(TEXT("the second save succeeds"), State->SecondSave->IsSuccess());
		TestTrue(TEXT("both copies exist"), UGameplayStatics::DoesSaveGameExist(UESaveSubsystem::GetCopyFileName(State->Slot, 0), UserIndex)
			&& UGameplayStatics::DoesSaveGameExist(UESaveSubsystem::GetCopyFileName(State->Slot, 1), UserIndex));

		// Not a save game: a short header naming a class that does not exist, so the engine's reader
		// fails cleanly (raw garbage could be read as a huge string length).
		TArray<uint8> Garbage;
		FMemoryWriter Writer(Garbage);
		FString MissingClass = TEXT("/Script/ExtendedSave.NotASaveGame");
		Writer << MissingClass;
		TestTrue(TEXT("the newer copy is overwritten with something unreadable"), UGameplayStatics::SaveDataToSlot(Garbage, UESaveSubsystem::GetCopyFileName(State->Slot, 1), UserIndex));

		AddExpectedMessage(TEXT("could not be read"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);
		AddExpectedMessage(TEXT("NotASaveGame"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);
		LatentSave->LoadSlot(State->Slot, EESaveLoadMode::ApplyToCurrentWorld, [State](const FEFResult& Result, bool bBackup)
		{
			State->BackupLoad = Result;
			State->bBackupLoadUsedBackup = bBackup;
		});
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the load from backup"), [State]() { return State->BackupLoad.IsSet(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		UESaveSubsystem* LatentSave = State->Game->GetSubsystem<UESaveSubsystem>();
		TestTrue(TEXT("a damaged newest copy still loads"), State->BackupLoad->IsSuccess());
		TestTrue(TEXT("from the backup"), State->bBackupLoadUsedBackup);
		TestEqual(TEXT("with the state of the previous save"), State->Actor.IsValid() ? State->Actor->Health : -1, 42);

		TestTrue(TEXT("the slot is deleted"), LatentSave->DeleteSlot(State->Slot));
		TestFalse(TEXT("and is gone"), LatentSave->DoesSlotExist(State->Slot));
		TestFalse(TEXT("and no longer listed"), LatentSave->GetSlots().ContainsByPredicate([State](const FESaveSlotInfo& Info) { return Info.SlotName == State->Slot; }));

		State->SavedListener.Unregister();
		State->Game.Reset();
		State->Settings.Reset();
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESaveMigrationTest, "UnrealExtendedSave.Migration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESaveMigrationTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		TSharedPtr<ESaveTestPrivate::FScopedSaveTestSettings> Settings;
		TSharedPtr<EFTest::FScopedGameInstance> Game;
		TStrongObjectPtr<UESaveTestObject> Object;
		FString Slot;
		TOptional<FEFResult> OldSave;
		TOptional<FEFResult> MigratedLoad;
		TOptional<FEFResult> NewSave;
		TOptional<FEFResult> TooNewLoad;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	State->Settings = MakeShared<ESaveTestPrivate::FScopedSaveTestSettings>();
	State->Game = MakeShared<EFTest::FScopedGameInstance>();
	State->Slot = ESaveTestPrivate::MakeTestSlotName();

	UESaveSubsystem* Save = State->Game->GetSubsystem<UESaveSubsystem>();
	if (!TestNotNull(TEXT("the save subsystem exists"), Save))
	{
		return false;
	}

	// A save from "version 1" of the game, where the object's score was called OldScore.
	FESaveRecord Record;
	Record.Id = TEXT("ESaveTestObject");
	Record.Properties.Add(TEXT("OldScore"), TEXT("9"));
	Save->GetActiveData()->Records.Add(Record.Id, Record);
	Save->SaveSlot(State->Slot, [State](const FEFResult& Result) { State->OldSave = Result; });

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the version 1 save"), [State]() { return State->OldSave.IsSet(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		TestTrue(TEXT("the version 1 save is written"), State->OldSave->IsSuccess());

		// "Version 2" of the game ships a migration for the rename.
		UESaveSettings* Settings = GetMutableDefault<UESaveSettings>();
		Settings->CurrentSchemaVersion = 2;
		Settings->Migrations = { TSoftClassPtr<UESaveMigration>(UESaveTestMigration::StaticClass()) };

		UESaveSubsystem* LatentSave = State->Game->GetSubsystem<UESaveSubsystem>();
		LatentSave->StartNewGame();
		State->Object.Reset(NewObject<UESaveTestObject>());
		LatentSave->RegisterSaveable(State->Object.Get());
		LatentSave->LoadSlot(State->Slot, EESaveLoadMode::ApplyToCurrentWorld, [State](const FEFResult& Result, bool) { State->MigratedLoad = Result; });
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the migrated load"), [State]() { return State->MigratedLoad.IsSet(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		UESaveSubsystem* LatentSave = State->Game->GetSubsystem<UESaveSubsystem>();
		TestTrue(TEXT("the old save loads"), State->MigratedLoad->IsSuccess());
		TestEqual(TEXT("the renamed property keeps its value"), State->Object->Score, 9);
		FString Migrated;
		TestTrue(TEXT("the migration ran"), LatentSave->GetValue(TEXT("Migrated"), Migrated) && Migrated == TEXT("yes"));
		TestEqual(TEXT("the loaded save is at the current version"), LatentSave->GetActiveData()->Info.SchemaVersion, 2);

		// Save at version 2, then pretend to be an older build that only knows version 1.
		LatentSave->SaveSlot(State->Slot, [State](const FEFResult& Result) { State->NewSave = Result; });
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the version 2 save"), [State]() { return State->NewSave.IsSet(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		GetMutableDefault<UESaveSettings>()->CurrentSchemaVersion = 1;
		AddExpectedMessage(TEXT("schema version"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, false);
		State->Game->GetSubsystem<UESaveSubsystem>()->LoadSlot(State->Slot, EESaveLoadMode::ApplyToCurrentWorld, [State](const FEFResult& Result, bool) { State->TooNewLoad = Result; });
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEFWaitUntilCommand(this, TEXT("the too-new load"), [State]() { return State->TooNewLoad.IsSet(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEFRunLatentCommand([this, State]()
	{
		TestTrue(TEXT("a save from a newer build is refused"), State->TooNewLoad->HasCode(ESaveResultCodes::NewerVersion));
		State->Game->GetSubsystem<UESaveSubsystem>()->DeleteSlot(State->Slot);
		State->Game.Reset();
		State->Settings.Reset();
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESaveSlotNameTest, "UnrealExtendedSave.SlotNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FESaveSlotNameTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("letters, digits, spaces, - and _ are fine"), UESaveSubsystem::IsValidSlotName(TEXT("Slot 1_a-b")));
	TestFalse(TEXT("empty is refused"), UESaveSubsystem::IsValidSlotName(FString()));
	TestFalse(TEXT("folders are refused"), UESaveSubsystem::IsValidSlotName(TEXT("../Other")));
	TestFalse(TEXT("a double underscore is reserved for backups"), UESaveSubsystem::IsValidSlotName(TEXT("Slot__backup")));
	TestFalse(TEXT("surrounding spaces are refused"), UESaveSubsystem::IsValidSlotName(TEXT(" Slot")));
	TestFalse(TEXT("names over 64 characters are refused"), UESaveSubsystem::IsValidSlotName(FString::ChrN(65, TEXT('a'))));
	TestEqual(TEXT("copy 0 is the slot name"), UESaveSubsystem::GetCopyFileName(TEXT("Slot"), 0), FString(TEXT("Slot")));
	TestEqual(TEXT("copy 1 is the backup"), UESaveSubsystem::GetCopyFileName(TEXT("Slot"), 1), FString(TEXT("Slot__backup")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
