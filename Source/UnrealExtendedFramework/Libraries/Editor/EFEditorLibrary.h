// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EFEditorLibrary.generated.h"

UENUM(BlueprintType)
enum class EExtendedLog : uint8
{
	Log,
	Warning,
	Error
};

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable , Category="Extended Log")
	static void ExtendedBlueprintLOG(EExtendedLog ExtendedLogType , FString Log);
	
	UFUNCTION(BlueprintCallable, Category = "Reflection" , meta=(DefaultToSelf="RequestOwner" , HidePin="RequestOwner"))
	static void ExecuteFunction(UObject* RequestOwner , UObject* TargetObject , const FString FunctionToExecute);
	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", Keywords="FPS"), Category = "Extended Editor")
	static float GetGameplayFramePerSecond(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable , Category = "Extended Editor")
	static void SetDefaultGameMap(const FString& MapName);

	UFUNCTION(BlueprintPure , Category = "Extended Editor")
	static FString GetDefaultGameMap();

	//Returns the project version set in the 'Project Settings' > 'Description' section of the editor
	UFUNCTION(BlueprintPure, Category = "Project")
	static FString GetProjectVersion();

	/** Returns true if this is being run from an editor preview */
	UFUNCTION(BlueprintPure, Category = Loading)
	static bool IsInEditor();
};
