// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "ESaveableComponent.h"
#include "ESaveableInterface.h"
#include "ESaveGameData.h"
#include "ESaveMigration.h"
#include "GameFramework/Actor.h"
#include "ESaveTestTypes.generated.h"

/** Types for the save plugin's automation tests. Not for use in games. */

UCLASS(NotBlueprintable, HideDropdown)
class UESaveTestCounterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	int32 Count = 0;
};

UCLASS(NotBlueprintable, NotPlaceable, HideDropdown)
class AESaveTestActor : public AActor
{
	GENERATED_BODY()

public:
	AESaveTestActor()
	{
		RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
		Saveable = CreateDefaultSubobject<UESaveableComponent>(TEXT("Saveable"));
		Counter = CreateDefaultSubobject<UESaveTestCounterComponent>(TEXT("Counter"));
	}

	UPROPERTY(SaveGame)
	int32 Health = 100;

	UPROPERTY(SaveGame)
	FString Title;

	UPROPERTY(SaveGame)
	FVector Waypoint = FVector::ZeroVector;

	/** Not marked SaveGame: must keep its current value on load. */
	UPROPERTY()
	int32 NotSaved = 0;

	UPROPERTY()
	TObjectPtr<UESaveableComponent> Saveable;

	UPROPERTY()
	TObjectPtr<UESaveTestCounterComponent> Counter;
};

UCLASS(NotBlueprintable, HideDropdown)
class UESaveTestObject : public UObject, public IESaveable
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	int32 Score = 0;

	UPROPERTY(SaveGame)
	TArray<FString> Unlocked;

	int32 BeforeSaveCalls = 0;
	int32 AfterLoadCalls = 0;

	virtual FName GetSaveId_Implementation() const override { return TEXT("ESaveTestObject"); }
	virtual void OnBeforeSave_Implementation() override { ++BeforeSaveCalls; }
	virtual void OnAfterLoad_Implementation() override { ++AfterLoadCalls; }
};

/** Upgrades schema 1 to 2: the object's OldScore became Score. */
UCLASS(NotBlueprintable, HideDropdown)
class UESaveTestMigration : public UESaveMigration
{
	GENERATED_BODY()

public:
	UESaveTestMigration()
	{
		FromVersion = 1;
	}

	virtual bool Migrate_Implementation(UESaveGameData* Data) override
	{
		Data->RenameSavedProperty(TEXT("OldScore"), TEXT("Score"));
		Data->Values.Add(TEXT("Migrated"), TEXT("yes"));
		return true;
	}
};
