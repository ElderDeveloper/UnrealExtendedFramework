// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "UObject/SoftObjectPath.h"
#include "ESaveTypes.generated.h"

/** What Load Slot does with the world. */
UENUM(BlueprintType)
enum class EESaveLoadMode : uint8
{
	/**
	 * Restores saveable objects that exist now and respawns saved runtime actors. Placed actors that
	 * were destroyed after the save come back only when their level is loaded again.
	 */
	ApplyToCurrentWorld,
	/** Opens the level the save was made in; objects restore themselves as they begin play. */
	OpenSavedLevel
};

/** Which copy wins when a slot exists both locally and in the cloud. */
UENUM(BlueprintType)
enum class EESaveCloudConflictPolicy : uint8
{
	/** The copy saved most recently. */
	NewestWins,
	/** Keep the local copy and upload it. */
	PreferLocal,
	/** Take the cloud copy. */
	PreferCloud
};

/** What the slot list shows without loading the save itself. */
USTRUCT(BlueprintType)
struct EXTENDEDSAVE_API FESaveSlotInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString SlotName;

	/** Set by the game when saving, e.g. "Chapter 3 - The Docks". */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FDateTime SaveTimeUtc;

	/** Seconds played in this save when it was made. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	double PlayTimeSeconds = 0.0;

	/** The level the save was made in. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString LevelName;

	/** CurrentSchemaVersion of the game that wrote it. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 SchemaVersion = 0;

	/** The game's build version (FApp::GetBuildVersion) that wrote it. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString BuildVersion;

	/** Counts up with every save of any slot; the higher of two copies is the newer. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int64 SaveSequence = 0;

	/** A screenshot or any image the game attached (PNG or JPEG bytes). */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	TArray<uint8> Thumbnail;

	/** Anything else the game wants to show in a slot list: difficulty, character name, ... */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	TMap<FString, FString> Extra;
};

/** One component's saved properties. */
USTRUCT()
struct EXTENDEDSAVE_API FESaveComponentRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name;

	/** Property name -> value as text. */
	UPROPERTY()
	TMap<FName, FString> Properties;
};

/** The saved state of one actor or object. */
USTRUCT()
struct EXTENDEDSAVE_API FESaveRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FName Id;

	/** The world the actor lived in; empty for objects that do not belong to a world. */
	UPROPERTY()
	FString World;

	/** True for actors spawned at run time: they are spawned again on load. */
	UPROPERTY()
	bool bSpawned = false;

	/** The class to spawn again, for spawned actors. */
	UPROPERTY()
	FSoftClassPath ActorClass;

	/** A placed actor that was destroyed (picked up, killed) and must stay gone. */
	UPROPERTY()
	bool bDestroyed = false;

	UPROPERTY()
	bool bHasTransform = false;

	UPROPERTY()
	FTransform Transform;

	/** The actor's or object's SaveGame properties: name -> value as text. */
	UPROPERTY()
	TMap<FName, FString> Properties;

	/** SaveGame properties of the actor's components. */
	UPROPERTY()
	TArray<FESaveComponentRecord> Components;
};

/** Payload of Extended.Save.* messages. */
USTRUCT(BlueprintType)
struct EXTENDEDSAVE_API FESaveMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString SlotName;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	bool bSuccess = false;

	/** For Loaded: the newest copy could not be read and the backup was used. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	bool bRestoredFromBackup = false;
};

/** Message bus channels the save subsystem sends on. Listen with Partial Match on Extended.Save for all of them. */
namespace ESaveMessages
{
	/** Before a save captures the world: last chance to write state into saveable objects. */
	EXTENDEDSAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Saving);
	EXTENDEDSAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Saved);
	EXTENDEDSAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loaded);
}

/** Codes save results use beyond EFResultCodes. */
namespace ESaveResultCodes
{
	/** The slot does not exist. */
	inline constexpr const TCHAR* NotFound = TEXT("NotFound");
	/** The slot exists but no copy of it could be read. */
	inline constexpr const TCHAR* Corrupt = TEXT("Corrupt");
	/** The save was written by a newer version of the game (higher schema version). */
	inline constexpr const TCHAR* NewerVersion = TEXT("NewerVersion");
	/** A migration step failed. */
	inline constexpr const TCHAR* MigrationFailed = TEXT("MigrationFailed");
	/** Writing to disk failed. */
	inline constexpr const TCHAR* WriteFailed = TEXT("WriteFailed");
}
