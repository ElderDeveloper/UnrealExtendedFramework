// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ESaveMigration.generated.h"

class UESaveGameData;

/**
 * Upgrades saves written by an older version of the game by one schema version.
 *
 * Bump CurrentSchemaVersion in Project Settings > Extended Framework > Extended Save when the saved
 * data changes in a way old saves cannot load as they are (a renamed SaveGame variable, a split
 * stat, a removed actor), and add a migration whose FromVersion is the old version. Loading a save
 * at version 1 with CurrentSchemaVersion 3 runs the migrations from 1 and from 2, in order.
 *
 * Renames and removed properties alone need no migration: properties are saved by name and
 * unknown ones are skipped. RenameSavedProperty keeps the value of a renamed one.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew)
class EXTENDEDSAVE_API UESaveMigration : public UObject
{
	GENERATED_BODY()

public:
	/** The schema version this migration upgrades from; the save is at FromVersion + 1 afterwards. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Migration", meta = (ClampMin = "0"))
	int32 FromVersion = 1;

	/** Changes Data in place. Return false to stop the load with MigrationFailed. */
	UFUNCTION(BlueprintNativeEvent, Category = "Migration")
	bool Migrate(UESaveGameData* Data);
};
