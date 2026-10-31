// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ESaveTypes.h"
#include "GameFramework/SaveGame.h"
#include "ESaveGameData.generated.h"

/**
 * Everything one save slot holds: metadata, the state of every saveable actor and object, and a
 * free key/value store for small games and global flags.
 *
 * Migrations (UESaveMigration) receive this object and may change any of it.
 */
UCLASS(BlueprintType)
class EXTENDEDSAVE_API UESaveGameData : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FESaveSlotInfo Info;

	/** Saved actors and objects by save id. */
	UPROPERTY()
	TMap<FName, FESaveRecord> Records;

	/** Free key/value store (Set Save Value / Get Save Value). */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	TMap<FName, FString> Values;

	/** Removes the record and value whose key is Id; for migrations. */
	UFUNCTION(BlueprintCallable, Category = "Extended Save|Migration")
	void RemoveRecord(FName Id) { Records.Remove(Id); }

	/** Renames a property inside every record, e.g. after renaming a SaveGame variable; for migrations. Returns how many were renamed. */
	UFUNCTION(BlueprintCallable, Category = "Extended Save|Migration")
	int32 RenameSavedProperty(FName OldName, FName NewName);

	/** Every record id, for migrations. */
	UFUNCTION(BlueprintPure, Category = "Extended Save|Migration")
	TArray<FName> GetRecordIds() const;
};

/** The list of slots, kept in its own small save so menus can list slots without loading them. */
UCLASS()
class EXTENDEDSAVE_API UESaveIndex : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FESaveSlotInfo> Slots;

	/** Which copy (0 or 1) of each slot is the newer. */
	UPROPERTY()
	TMap<FString, int32> NewestCopy;

	/** Last SaveSequence handed out. */
	UPROPERTY()
	int64 LastSaveSequence = 0;

	FESaveSlotInfo* FindSlot(const FString& SlotName);
	const FESaveSlotInfo* FindSlot(const FString& SlotName) const;
};
