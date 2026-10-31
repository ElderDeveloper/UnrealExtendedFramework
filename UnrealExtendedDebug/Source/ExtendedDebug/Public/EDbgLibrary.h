// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EDbgTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EDbgLibrary.generated.h"

/**
 * Debug tools from Blueprint. Every function does nothing in Shipping builds, so it is safe to
 * leave the calls in.
 */
UCLASS(meta = (ScriptName = "EDbgLibrary"))
class EXTENDEDDEBUG_API UEDbgLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** True in development builds with the tools enabled in the project settings. */
	UFUNCTION(BlueprintPure, Category = "Extended Debug", meta = (WorldContext = "WorldContextObject"))
	static bool AreDebugToolsEnabled(const UObject* WorldContextObject);

	/**
	 * Adds a cheat to the console (Extended.Cheat Name args) and the debug menu. Handler runs with
	 * the player who ran it and the words typed after the name. It is removed when the object that
	 * owns the event is destroyed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended Debug", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Info"))
	static void RegisterCheat(const UObject* WorldContextObject, const FEDbgCheatInfo& Info, FEDbgCheatDelegate Handler);

	UFUNCTION(BlueprintCallable, Category = "Extended Debug", meta = (WorldContext = "WorldContextObject"))
	static void UnregisterCheat(const UObject* WorldContextObject, FName CheatName);

	/** Runs a cheat as the first local player would from the menu. */
	UFUNCTION(BlueprintCallable, Category = "Extended Debug", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Arguments"))
	static bool RunCheat(const UObject* WorldContextObject, FName CheatName, const TArray<FString>& Arguments);

	UFUNCTION(BlueprintCallable, Category = "Extended Debug", meta = (WorldContext = "WorldContextObject"))
	static TArray<FEDbgCheatInfo> GetCheats(const UObject* WorldContextObject);

	/** Shows Key = Value on screen until it is cleared or not updated for WatchTimeoutSeconds. Call it every frame for live values. */
	UFUNCTION(BlueprintCallable, Category = "Extended Debug", meta = (WorldContext = "WorldContextObject"))
	static void SetDebugWatch(const UObject* WorldContextObject, const FString& Key, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Extended Debug", meta = (WorldContext = "WorldContextObject"))
	static void ClearDebugWatch(const UObject* WorldContextObject, const FString& Key);

	UFUNCTION(BlueprintCallable, Category = "Extended Debug", meta = (WorldContext = "WorldContextObject"))
	static void ToggleDebugMenu(const UObject* WorldContextObject);
};
