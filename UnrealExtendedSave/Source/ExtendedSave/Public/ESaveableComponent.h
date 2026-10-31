// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ESaveableComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FESaveableComponentEvent);

/**
 * Makes its actor part of the save game: the actor's SaveGame properties (and its components'),
 * its transform, and whether it was destroyed.
 *
 * - Placed actors are saved under their level and name. A placed actor destroyed during play (a
 *   pickup, a killed enemy) stays destroyed when the save is loaded.
 * - Actors spawned at run time get a generated id and are spawned again on load, then restored.
 * - When a streaming level unloads, its actors' state is kept in memory, so it is back when the
 *   level streams in again and is included in the next save.
 *
 * Mark the variables to keep with SaveGame (the "SaveGame" checkbox in a Blueprint variable's
 * advanced details, or UPROPERTY(SaveGame) in C++). Implement IESaveable on the actor or its
 * components for OnBeforeSave and OnAfterLoad callbacks.
 */
UCLASS(ClassGroup = "Extended", meta = (BlueprintSpawnableComponent, DisplayName = "Saveable"))
class EXTENDEDSAVE_API UESaveableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UESaveableComponent();

	/**
	 * A fixed id for this actor. Leave it empty: placed actors then use their level and name, and
	 * spawned actors a generated id. Set it to keep a save working after renaming a placed actor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save")
	FName SaveId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	bool bSaveTransform = true;

	/** A placed actor destroyed during play stays destroyed when the save is loaded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	bool bRememberDestruction = true;

	/** Also saves the SaveGame properties of the actor's other components. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	bool bSaveComponents = true;

	/** Just before the actor's state is written. */
	UPROPERTY(BlueprintAssignable, Category = "Save")
	FESaveableComponentEvent OnSaving;

	/** Just after the actor's state was restored from a save. */
	UPROPERTY(BlueprintAssignable, Category = "Save")
	FESaveableComponentEvent OnLoaded;

	/** The id the actor is saved under. Valid from BeginPlay. */
	UFUNCTION(BlueprintPure, Category = "Save")
	FName GetResolvedSaveId();

	/** True for actors that are part of a level, false for actors spawned at run time. */
	UFUNCTION(BlueprintPure, Category = "Save")
	bool IsPlacedInLevel() const;

	/** Gives a respawned actor the id it was saved under. Called by the save subsystem before BeginPlay. */
	void SetRuntimeSaveId(FName InId);

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** The generated id of a spawned actor. */
	FName RuntimeSaveId;

	/** Resolved on first use, once the actor knows whether it was placed. */
	FName CachedSaveId;
};
