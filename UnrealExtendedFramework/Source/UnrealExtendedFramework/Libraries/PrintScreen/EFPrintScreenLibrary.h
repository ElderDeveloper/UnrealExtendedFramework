// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EFPrintScreenLibrary.generated.h"

/**
 * Blueprint function library for on-screen debug printing.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFPrintScreenLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	/**
	 * Prints a string to the screen using key 0 (overwrites previous with same key).
	 * @param WorldContextObject World context
	 * @param String The text to display
	 * @param Time Duration in seconds (0 = single frame)
	 * @param Color Display color
	 * @param bPrintToLog If true, also prints to the output log
	 */
	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject", DisplayName = "Print Zero String"), Category="ExtendedFramework|Debug")
	static void PrintZeroString(const UObject* WorldContextObject, FString String = "", float Time = 0, FLinearColor Color = FLinearColor::Green, const bool bPrintToLog = false);
};
