// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EFEditorLibrary.generated.h"

/** Log severity level for the extended Blueprint logging function. */
UENUM(BlueprintType)
enum class EExtendedLog : uint8
{
	Log,
	Warning,
	Error
};

/**
 * Blueprint function library providing extended editor/debug utilities including
 * logging, reflection-based function execution, FPS display, project settings,
 * and platform information.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Writes a log message at the specified severity level to LogTemp.
	 * @param ExtendedLogType The severity (Log, Warning, Error)
	 * @param Log The message string to log
	 */
	UFUNCTION(BlueprintCallable, Category="Extended Log")
	static void ExtendedBlueprintLOG(EExtendedLog ExtendedLogType, FString Log);
	
	/**
	 * Calls a function by name on the target object using ProcessEvent.
	 * Only works with UFUNCTION-marked functions that have no parameters.
	 * @param RequestOwner The calling object (hidden, auto-filled to self)
	 * @param TargetObject The object whose function to invoke
	 * @param FunctionToExecute The name of the UFUNCTION to call
	 */
	UFUNCTION(BlueprintCallable, Category = "Reflection", meta=(DefaultToSelf="RequestOwner", HidePin="RequestOwner"))
	static void ExecuteFunction(UObject* RequestOwner, UObject* TargetObject, const FString FunctionToExecute);
	
	/**
	 * Returns the current gameplay FPS, accounting for global time dilation.
	 * @param WorldContextObject World context
	 * @return Frames per second, or 0 if world is unavailable
	 */
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", Keywords="FPS"), Category = "Extended Editor")
	static float GetGameplayFramePerSecond(const UObject* WorldContextObject);

	/**
	 * Returns the current world delta seconds.
	 * @param WorldContextObject World context
	 * @return Delta time in seconds, or 0 if world is unavailable
	 */
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", Keywords="Delta Time"), Category = "Extended Editor")
	static float GetDeltaSeconds(const UObject* WorldContextObject);

	/**
	 * Sets the default game map in the project settings (persists to config).
	 * @param MapName Asset path of the map (e.g. "/Game/Maps/MainMenu")
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended Editor")
	static void SetDefaultGameMap(const FString& MapName);

	/**
	 * Returns the default game map set in project settings.
	 * @return Asset path of the default map
	 */
	UFUNCTION(BlueprintPure, Category = "Extended Editor")
	static FString GetDefaultGameMap();

	/**
	 * Returns the project version string set in Project Settings > Description.
	 * @return The ProjectVersion string from DefaultGame.ini
	 */
	UFUNCTION(BlueprintPure, Category = "Project")
	static FString GetProjectVersion();

	/**
	 * Returns true if the game is currently running inside the Unreal Editor.
	 * Unlike IsPlayingInEditor (which uses WITH_EDITOR preprocessor), this checks GIsEditor at runtime.
	 */
	UFUNCTION(BlueprintPure, Category = "Loading")
	static bool IsInEditor();

	/**
	 * Returns the name of the current platform (e.g. "Windows", "Linux", "Android", "IOS").
	 * @return The platform name string
	 */
	UFUNCTION(BlueprintPure, Category = "Project")
	static FString GetPlatformName();
};
