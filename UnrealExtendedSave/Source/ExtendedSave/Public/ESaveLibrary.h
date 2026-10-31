// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ESaveTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ESaveLibrary.generated.h"

/** Blueprint shortcuts to UESaveSubsystem that work from any world context. */
UCLASS(meta = (ScriptName = "ESaveLibrary"))
class EXTENDEDSAVE_API UESaveLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Every save slot, newest first, for a load menu. */
	UFUNCTION(BlueprintPure, Category = "Extended Save", meta = (WorldContext = "WorldContextObject"))
	static TArray<FESaveSlotInfo> GetSaveSlots(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Extended Save", meta = (WorldContext = "WorldContextObject"))
	static bool DoesSaveSlotExist(const UObject* WorldContextObject, const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "Extended Save", meta = (WorldContext = "WorldContextObject"))
	static bool DeleteSaveSlot(const UObject* WorldContextObject, const FString& SlotName);

	/** Forgets the active save before starting a new game. */
	UFUNCTION(BlueprintCallable, Category = "Extended Save", meta = (WorldContext = "WorldContextObject"))
	static void StartNewSaveGame(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Extended Save|Autosave", meta = (WorldContext = "WorldContextObject"))
	static void RequestAutosave(const UObject* WorldContextObject);

	/** Blocks autosaves while any reason is set (Cutscene, Combat, MainMenu, ...). */
	UFUNCTION(BlueprintCallable, Category = "Extended Save|Autosave", meta = (WorldContext = "WorldContextObject"))
	static void SetAutosaveBlocked(const UObject* WorldContextObject, FName Reason, bool bBlocked);

	/** Includes an object that implements the Saveable interface in saves. */
	UFUNCTION(BlueprintCallable, Category = "Extended Save", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "Object"))
	static void RegisterSaveable(const UObject* WorldContextObject, UObject* Object);

	// Values: a key/value store inside the active save, for flags and progress that belong to no actor.

	UFUNCTION(BlueprintCallable, Category = "Extended Save|Values", meta = (WorldContext = "WorldContextObject"))
	static void SetSaveString(const UObject* WorldContextObject, FName Key, const FString& Value);

	UFUNCTION(BlueprintPure, Category = "Extended Save|Values", meta = (WorldContext = "WorldContextObject"))
	static FString GetSaveString(const UObject* WorldContextObject, FName Key, const FString& Default);

	UFUNCTION(BlueprintCallable, Category = "Extended Save|Values", meta = (WorldContext = "WorldContextObject"))
	static void SetSaveInt(const UObject* WorldContextObject, FName Key, int32 Value);

	UFUNCTION(BlueprintPure, Category = "Extended Save|Values", meta = (WorldContext = "WorldContextObject"))
	static int32 GetSaveInt(const UObject* WorldContextObject, FName Key, int32 Default = 0);

	UFUNCTION(BlueprintCallable, Category = "Extended Save|Values", meta = (WorldContext = "WorldContextObject"))
	static void SetSaveFloat(const UObject* WorldContextObject, FName Key, float Value);

	UFUNCTION(BlueprintPure, Category = "Extended Save|Values", meta = (WorldContext = "WorldContextObject"))
	static float GetSaveFloat(const UObject* WorldContextObject, FName Key, float Default = 0.f);

	UFUNCTION(BlueprintCallable, Category = "Extended Save|Values", meta = (WorldContext = "WorldContextObject"))
	static void SetSaveBool(const UObject* WorldContextObject, FName Key, bool Value);

	UFUNCTION(BlueprintPure, Category = "Extended Save|Values", meta = (WorldContext = "WorldContextObject"))
	static bool GetSaveBool(const UObject* WorldContextObject, FName Key, bool Default = false);

	UFUNCTION(BlueprintPure, Category = "Extended Save|Values", meta = (WorldContext = "WorldContextObject"))
	static bool HasSaveValue(const UObject* WorldContextObject, FName Key);
};
