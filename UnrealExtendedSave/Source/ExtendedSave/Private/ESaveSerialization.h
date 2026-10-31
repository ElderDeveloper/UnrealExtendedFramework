// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Reads and writes the properties marked SaveGame on an object as text, one entry per property
 * ("Health", or "Slots[2]" for a fixed-size C array).
 *
 * Text keeps saves readable and tolerant of change: a property that was removed is skipped on
 * load, a new one keeps its default, and a struct is written whole (its fields need no SaveGame
 * flag of their own). Object references are written as paths, so they survive only for objects
 * that have the same path every run (assets, placed actors).
 */
namespace ESaveSerialization
{
	/** Whether Class has any property marked SaveGame. */
	bool HasSaveGameProperties(const UClass* Class);

	void Capture(const UObject* Object, TMap<FName, FString>& OutProperties);

	/** Returns the number of properties restored. Entries that match no SaveGame property are skipped. */
	int32 Restore(UObject* Object, const TMap<FName, FString>& Properties);
}
