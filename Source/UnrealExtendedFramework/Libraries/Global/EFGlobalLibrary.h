// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "EFGlobalLibrary.generated.h"


/**
 * Blueprint function library providing a lightweight global variable system keyed by GameplayTags.
 * Supports primitive types (bool, int, float, string, etc.) and composite types (Vector, Rotator, Transform).
 *
 * Actor and Object globals use UEFGlobalSubsystem (engine subsystem, persistent across levels).
 * Primitive globals use process-lifetime static storage (also persistent across levels).
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFGlobalLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< GETTERS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/** Returns the global class stored for the tag, or nullptr if not set. */
	UFUNCTION(BlueprintPure, Category="Global Library|Get")
	static TSubclassOf<UObject> GetGlobalClass(FGameplayTag Tag);

	/** Returns the global bool stored for the tag, or false if not set. */
	UFUNCTION(BlueprintPure, Category="Global Library|Get")
	static bool GetGlobalBool(FGameplayTag Tag);

	/** Returns the global byte stored for the tag, or 0 if not set. */
	UFUNCTION(BlueprintPure, Category="Global Library|Get")
	static uint8 GetGlobalByte(FGameplayTag Tag);

	/** Returns the global float stored for the tag, or 0 if not set. */
	UFUNCTION(BlueprintPure, Category="Global Library|Get")
	static float GetGlobalFloat(FGameplayTag Tag);

	/** Returns the global int32 stored for the tag, or 0 if not set. */
	UFUNCTION(BlueprintPure, Category="Global Library|Get")
	static int32 GetGlobalInt(FGameplayTag Tag);

	/** Returns the global int64 stored for the tag, or 0 if not set. */
	UFUNCTION(BlueprintPure, Category="Global Library|Get")
	static int64 GetGlobalInt64(FGameplayTag Tag);

	/** Returns the global FName stored for the tag, or NAME_None if not set. */
	UFUNCTION(BlueprintPure, Category="Global Library|Get")
	static FName GetGlobalName(FGameplayTag Tag);

	/** Returns the global FString stored for the tag, or empty string if not set. */
	UFUNCTION(BlueprintPure, Category="Global Library|Get")
	static FString GetGlobalString(FGameplayTag Tag);

	/** Returns the global FText stored for the tag, or empty text if not set. */
	UFUNCTION(BlueprintPure, Category="Global Library|Get")
	static FText GetGlobalText(FGameplayTag Tag);

	/** Returns the global FVector stored for the tag, or ZeroVector if not set. */
	UFUNCTION(BlueprintPure, Category="Global Library|Get")
	static FVector GetGlobalVector(FGameplayTag Tag);

	/** Returns the global FRotator stored for the tag, or ZeroRotator if not set. */
	UFUNCTION(BlueprintPure, Category="Global Library|Get")
	static FRotator GetGlobalRotator(FGameplayTag Tag);

	/** Returns the global FTransform stored for the tag, or identity transform if not set. */
	UFUNCTION(BlueprintPure, Category="Global Library|Get")
	static FTransform GetGlobalTransform(FGameplayTag Tag);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< SETTERS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/** Sets a global class and returns the stored value. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Set")
	static TSubclassOf<UObject> SetGlobalClass(FGameplayTag Tag, TSubclassOf<UObject> Value);

	/** Sets a global bool and returns the stored value. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Set")
	static bool SetGlobalBool(FGameplayTag Tag, bool Value);

	/** Sets a global byte and returns the stored value. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Set")
	static uint8 SetGlobalByte(FGameplayTag Tag, uint8 Value);

	/** Sets a global float and returns the stored value. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Set")
	static float SetGlobalFloat(FGameplayTag Tag, float Value);

	/** Sets a global int32 and returns the stored value. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Set")
	static int32 SetGlobalInt(FGameplayTag Tag, int32 Value);

	/** Sets a global int64 and returns the stored value. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Set")
	static int64 SetGlobalInt64(FGameplayTag Tag, int64 Value);

	/** Sets a global FName and returns the stored value. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Set")
	static FName SetGlobalName(FGameplayTag Tag, FName Value);

	/** Sets a global FString and returns the stored value. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Set")
	static FString SetGlobalString(FGameplayTag Tag, FString Value);

	/** Sets a global FText and returns the stored value. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Set")
	static FText SetGlobalText(FGameplayTag Tag, FText Value);

	/** Sets a global FVector and returns the stored value. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Set")
	static FVector SetGlobalVector(FGameplayTag Tag, FVector Value);

	/** Sets a global FRotator and returns the stored value. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Set")
	static FRotator SetGlobalRotator(FGameplayTag Tag, FRotator Value);

	/** Sets a global FTransform and returns the stored value. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Set")
	static FTransform SetGlobalTransform(FGameplayTag Tag, FTransform Value);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< REMOVE >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/** Removes the global class entry for the given tag. Returns true if removed. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Remove")
	static bool RemoveGlobalClass(FGameplayTag Tag);

	/** Removes the global bool entry for the given tag. Returns true if removed. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Remove")
	static bool RemoveGlobalBool(FGameplayTag Tag);

	/** Removes the global byte entry for the given tag. Returns true if removed. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Remove")
	static bool RemoveGlobalByte(FGameplayTag Tag);

	/** Removes the global float entry for the given tag. Returns true if removed. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Remove")
	static bool RemoveGlobalFloat(FGameplayTag Tag);

	/** Removes the global int32 entry for the given tag. Returns true if removed. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Remove")
	static bool RemoveGlobalInt(FGameplayTag Tag);

	/** Removes the global int64 entry for the given tag. Returns true if removed. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Remove")
	static bool RemoveGlobalInt64(FGameplayTag Tag);

	/** Removes the global FName entry for the given tag. Returns true if removed. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Remove")
	static bool RemoveGlobalName(FGameplayTag Tag);

	/** Removes the global FString entry for the given tag. Returns true if removed. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Remove")
	static bool RemoveGlobalString(FGameplayTag Tag);

	/** Removes the global FText entry for the given tag. Returns true if removed. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Remove")
	static bool RemoveGlobalText(FGameplayTag Tag);

	/** Removes the global FVector entry for the given tag. Returns true if removed. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Remove")
	static bool RemoveGlobalVector(FGameplayTag Tag);

	/** Removes the global FRotator entry for the given tag. Returns true if removed. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Remove")
	static bool RemoveGlobalRotator(FGameplayTag Tag);

	/** Removes the global FTransform entry for the given tag. Returns true if removed. */
	UFUNCTION(BlueprintCallable, Category="Global Library|Remove")
	static bool RemoveGlobalTransform(FGameplayTag Tag);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< CONTAINS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/** Returns true if a global class entry exists for the given tag. */
	UFUNCTION(BlueprintPure, Category="Global Library|Contains")
	static bool ContainsGlobalClass(FGameplayTag Tag);

	/** Returns true if a global bool entry exists for the given tag. */
	UFUNCTION(BlueprintPure, Category="Global Library|Contains")
	static bool ContainsGlobalBool(FGameplayTag Tag);

	/** Returns true if a global byte entry exists for the given tag. */
	UFUNCTION(BlueprintPure, Category="Global Library|Contains")
	static bool ContainsGlobalByte(FGameplayTag Tag);

	/** Returns true if a global float entry exists for the given tag. */
	UFUNCTION(BlueprintPure, Category="Global Library|Contains")
	static bool ContainsGlobalFloat(FGameplayTag Tag);

	/** Returns true if a global int32 entry exists for the given tag. */
	UFUNCTION(BlueprintPure, Category="Global Library|Contains")
	static bool ContainsGlobalInt(FGameplayTag Tag);

	/** Returns true if a global int64 entry exists for the given tag. */
	UFUNCTION(BlueprintPure, Category="Global Library|Contains")
	static bool ContainsGlobalInt64(FGameplayTag Tag);

	/** Returns true if a global FName entry exists for the given tag. */
	UFUNCTION(BlueprintPure, Category="Global Library|Contains")
	static bool ContainsGlobalName(FGameplayTag Tag);

	/** Returns true if a global FString entry exists for the given tag. */
	UFUNCTION(BlueprintPure, Category="Global Library|Contains")
	static bool ContainsGlobalString(FGameplayTag Tag);

	/** Returns true if a global FText entry exists for the given tag. */
	UFUNCTION(BlueprintPure, Category="Global Library|Contains")
	static bool ContainsGlobalText(FGameplayTag Tag);

	/** Returns true if a global FVector entry exists for the given tag. */
	UFUNCTION(BlueprintPure, Category="Global Library|Contains")
	static bool ContainsGlobalVector(FGameplayTag Tag);

	/** Returns true if a global FRotator entry exists for the given tag. */
	UFUNCTION(BlueprintPure, Category="Global Library|Contains")
	static bool ContainsGlobalRotator(FGameplayTag Tag);

	/** Returns true if a global FTransform entry exists for the given tag. */
	UFUNCTION(BlueprintPure, Category="Global Library|Contains")
	static bool ContainsGlobalTransform(FGameplayTag Tag);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< GET ALL >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/** Returns all global class entries as parallel tag/value arrays. */
	UFUNCTION(BlueprintCallable, Category="Global Library|All")
	static void GetAllGlobalClass(TArray<FGameplayTag>& Tags, TArray<TSubclassOf<UObject>>& Values);

	/** Returns all global bool entries as parallel tag/value arrays. */
	UFUNCTION(BlueprintCallable, Category="Global Library|All")
	static void GetAllGlobalBool(TArray<FGameplayTag>& Tags, TArray<bool>& Values);

	/** Returns all global byte entries as parallel tag/value arrays. */
	UFUNCTION(BlueprintCallable, Category="Global Library|All")
	static void GetAllGlobalByte(TArray<FGameplayTag>& Tags, TArray<uint8>& Values);

	/** Returns all global float entries as parallel tag/value arrays. */
	UFUNCTION(BlueprintCallable, Category="Global Library|All")
	static void GetAllGlobalFloat(TArray<FGameplayTag>& Tags, TArray<float>& Values);

	/** Returns all global int32 entries as parallel tag/value arrays. */
	UFUNCTION(BlueprintCallable, Category="Global Library|All")
	static void GetAllGlobalInt(TArray<FGameplayTag>& Tags, TArray<int32>& Values);

	/** Returns all global int64 entries as parallel tag/value arrays. */
	UFUNCTION(BlueprintCallable, Category="Global Library|All")
	static void GetAllGlobalInt64(TArray<FGameplayTag>& Tags, TArray<int64>& Values);

	/** Returns all global FName entries as parallel tag/value arrays. */
	UFUNCTION(BlueprintCallable, Category="Global Library|All")
	static void GetAllGlobalName(TArray<FGameplayTag>& Tags, TArray<FName>& Values);

	/** Returns all global FString entries as parallel tag/value arrays. */
	UFUNCTION(BlueprintCallable, Category="Global Library|All")
	static void GetAllGlobalString(TArray<FGameplayTag>& Tags, TArray<FString>& Values);

	/** Returns all global FText entries as parallel tag/value arrays. */
	UFUNCTION(BlueprintCallable, Category="Global Library|All")
	static void GetAllGlobalText(TArray<FGameplayTag>& Tags, TArray<FText>& Values);

	/** Returns all global FVector entries as parallel tag/value arrays. */
	UFUNCTION(BlueprintCallable, Category="Global Library|All")
	static void GetAllGlobalVector(TArray<FGameplayTag>& Tags, TArray<FVector>& Values);

	/** Returns all global FRotator entries as parallel tag/value arrays. */
	UFUNCTION(BlueprintCallable, Category="Global Library|All")
	static void GetAllGlobalRotator(TArray<FGameplayTag>& Tags, TArray<FRotator>& Values);

	/** Returns all global FTransform entries as parallel tag/value arrays. */
	UFUNCTION(BlueprintCallable, Category="Global Library|All")
	static void GetAllGlobalTransform(TArray<FGameplayTag>& Tags, TArray<FTransform>& Values);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< ACTOR / OBJECT (via Subsystem) >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Retrieves a global actor from the engine subsystem.
	 * @param WorldContextObject World context
	 * @param Tag The GameplayTag to look up
	 * @param Valid Execution pin — true if the actor is found and valid
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", ExpandBoolAsExecs = "Valid"))
	static AActor* GetGlobalActor(const UObject* WorldContextObject, FGameplayTag Tag, bool& Valid);

	/** Registers an actor in the global engine subsystem under the given tag. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetGlobalActor(const UObject* WorldContextObject, FGameplayTag Tag, AActor* Value);

	/**
	 * Retrieves a global object from the engine subsystem.
	 * @param WorldContextObject World context
	 * @param Tag The GameplayTag to look up
	 * @param Valid Execution pin — true if the object is found and valid
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", ExpandBoolAsExecs = "Valid"))
	static UObject* GetGlobalObject(const UObject* WorldContextObject, FGameplayTag Tag, bool& Valid);

	/** Registers an object in the global engine subsystem under the given tag. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetGlobalObject(const UObject* WorldContextObject, FGameplayTag Tag, UObject* Value);

};
