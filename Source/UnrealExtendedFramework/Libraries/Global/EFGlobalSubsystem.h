// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "EFGlobalSubsystem.generated.h"


/**
 * Engine subsystem providing a global registry for actors and objects, keyed by GameplayTag.
 * Persists for the lifetime of the engine (across level transitions).
 * Use EFGlobalLibrary Blueprint functions to access this subsystem.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFGlobalSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
	
public:

	/**
	 * Registers or overwrites a global actor with the given tag.
	 * @param Tag The GameplayTag key
	 * @param Actor The actor to store (may be nullptr to clear)
	 */
	void SetGlobalActor(FGameplayTag Tag, AActor* Actor);

	/**
	 * Retrieves a global actor by tag.
	 * @param Tag The GameplayTag to look up
	 * @param Valid Set to true if the actor was found and is still valid
	 * @return The stored actor, or nullptr if not found
	 */
	AActor* GetGlobalActor(FGameplayTag Tag, bool& Valid);

	/**
	 * Removes a global actor entry by tag.
	 * @param Tag The GameplayTag to remove
	 * @return True if an entry was removed
	 */
	bool RemoveGlobalActor(FGameplayTag Tag);

	/**
	 * Checks if a global actor exists for the given tag (without modifying the map).
	 * @param Tag The GameplayTag to check
	 * @return True if an entry exists for this tag
	 */
	bool ContainsGlobalActor(FGameplayTag Tag) const;

	/**
	 * Removes all registered global actors.
	 */
	void ClearAllGlobalActors();

	/**
	 * Registers or overwrites a global object with the given tag.
	 * @param Tag The GameplayTag key
	 * @param Object The object to store (may be nullptr to clear)
	 */
	void SetGlobalObject(FGameplayTag Tag, UObject* Object);

	/**
	 * Retrieves a global object by tag.
	 * @param Tag The GameplayTag to look up
	 * @param Valid Set to true if the object was found and is still valid
	 * @return The stored object, or nullptr if not found
	 */
	UObject* GetGlobalObject(FGameplayTag Tag, bool& Valid);

	/**
	 * Removes a global object entry by tag.
	 * @param Tag The GameplayTag to remove
	 * @return True if an entry was removed
	 */
	bool RemoveGlobalObject(FGameplayTag Tag);

	/**
	 * Checks if a global object exists for the given tag (without modifying the map).
	 * @param Tag The GameplayTag to check
	 * @return True if an entry exists for this tag
	 */
	bool ContainsGlobalObject(FGameplayTag Tag) const;

	/**
	 * Removes all registered global objects.
	 */
	void ClearAllGlobalObjects();


	/**
	 * Returns all registered global actors as parallel arrays of tags and actors.
	 * @param Print If true, prints each entry to the screen
	 * @param Tags Output array of tags
	 * @param Actors Output array of corresponding actors
	 */
	UFUNCTION(BlueprintCallable)
	void GetAllGlobalActors(bool Print, TArray<FGameplayTag>& Tags, TArray<AActor*>& Actors);

	/**
	 * Returns all registered global objects as parallel arrays of tags and objects.
	 * @param Print If true, prints each entry to the screen
	 * @param Tags Output array of tags
	 * @param Objects Output array of corresponding objects
	 */
	UFUNCTION(BlueprintCallable)
	void GetAllGlobalObjects(bool Print, TArray<FGameplayTag>& Tags, TArray<UObject*>& Objects);

	
	UPROPERTY(BlueprintReadWrite)
	TMap<FGameplayTag, AActor*> EFGlobalActors;
	
	UPROPERTY(BlueprintReadWrite)
	TMap<FGameplayTag, UObject*> EFGlobalObjects;
	
};
