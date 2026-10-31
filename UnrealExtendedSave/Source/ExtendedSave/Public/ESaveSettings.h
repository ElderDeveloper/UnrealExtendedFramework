// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ESaveMigration.h"
#include "ESaveTypes.h"
#include "Settings/EFDeveloperSettings.h"
#include "ESaveSettings.generated.h"

/** Project Settings > Extended Framework > Extended Save. */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Extended Save"))
class EXTENDEDSAVE_API UESaveSettings : public UEFDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UESaveSettings* Get() { return GetDefault<UESaveSettings>(); }

	/** The version of the saved data this build writes. Bump it with a migration when the data changes. */
	UPROPERTY(config, EditAnywhere, Category = "Versioning", meta = (ClampMin = "1"))
	int32 CurrentSchemaVersion = 1;

	/** Migrations from older schema versions, applied in FromVersion order. */
	UPROPERTY(config, EditAnywhere, Category = "Versioning")
	TArray<TSoftClassPtr<UESaveMigration>> Migrations;

	/**
	 * Writes each slot to two files in turn and keeps the previous one, so a crash or power loss in
	 * the middle of a save never destroys the last good save. Loading falls back to the older copy
	 * when the newer one cannot be read.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Storage")
	bool bKeepBackup = true;

	/** The platform user index saves are written for. */
	UPROPERTY(config, EditAnywhere, Category = "Storage", meta = (ClampMin = "0"))
	int32 UserIndex = 0;

	/** The slot that holds the list of slots. */
	UPROPERTY(config, EditAnywhere, Category = "Storage")
	FString IndexSlotName = TEXT("ExtendedSave_Index");

	/** Keeps the state of actors in a streaming level that unloads, so it is restored when the level returns. */
	UPROPERTY(config, EditAnywhere, Category = "World")
	bool bKeepStateOfUnloadedLevels = true;

	/** Seconds between autosaves; 0 turns timed autosaves off. Request Autosave still works. */
	UPROPERTY(config, EditAnywhere, Category = "Autosave", meta = (ClampMin = "0", Units = "Seconds"))
	float AutosaveIntervalSeconds = 0.f;

	UPROPERTY(config, EditAnywhere, Category = "Autosave")
	FString AutosaveSlotName = TEXT("Autosave");

	/**
	 * Uploads every slot through the platform layer's cloud storage after it is saved. Steam
	 * Auto-Cloud can do the same without code; use this for EOS, PlayFab or explicit sync.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Cloud")
	bool bUploadToCloud = false;

	/** Which copy Sync Slot From Cloud keeps when both exist. */
	UPROPERTY(config, EditAnywhere, Category = "Cloud")
	EESaveCloudConflictPolicy CloudConflictPolicy = EESaveCloudConflictPolicy::NewestWins;

	/** Cloud file name = prefix + slot name + ".sav". */
	UPROPERTY(config, EditAnywhere, Category = "Cloud")
	FString CloudFilePrefix;

	/** Sends Extended.Save.Saving, Saved and Loaded on the message bus. */
	UPROPERTY(config, EditAnywhere, Category = "Messages")
	bool bBroadcastMessages = true;
};
