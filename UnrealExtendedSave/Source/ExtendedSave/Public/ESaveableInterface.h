// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ESaveableInterface.generated.h"

UINTERFACE(Blueprintable)
class EXTENDEDSAVE_API UESaveable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Something whose state is saved: every property marked SaveGame (the "SaveGame" checkbox on a
 * Blueprint variable, or UPROPERTY(SaveGame) in C++) is written and restored.
 *
 * Actors get this by adding a UESaveableComponent; the interface is then optional, for the
 * callbacks. Objects that are not actors (a quest log, an inventory subsystem) implement the
 * interface, return a stable id from GetSaveId, and call UESaveSubsystem::RegisterSaveable.
 */
class EXTENDEDSAVE_API IESaveable
{
	GENERATED_BODY()

public:
	/**
	 * Id under which the object is saved; must be the same every run. Actors with a
	 * UESaveableComponent do not need it: the component provides one.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended Save")
	FName GetSaveId() const;
	virtual FName GetSaveId_Implementation() const { return NAME_None; }

	/** Called just before the object's SaveGame properties are written: copy runtime state into them. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended Save")
	void OnBeforeSave();
	virtual void OnBeforeSave_Implementation() {}

	/** Called after the object's SaveGame properties were restored: rebuild runtime state from them. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended Save")
	void OnAfterLoad();
	virtual void OnAfterLoad_Implementation() {}
};
