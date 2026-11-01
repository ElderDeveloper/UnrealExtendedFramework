// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EFInputLibrary.generated.h"

/**
 * Blueprint function library for clipboard operations.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFInputLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Clipboard >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Copies a string to the system clipboard. Cross-platform.
	 * @param TextToCopy The string to copy
	 * @return True on success
	 */
	UFUNCTION(BlueprintCallable, Category="Extended Input | Clipboard")
	static bool CopyStringToClipboard(const FString& TextToCopy);

	/**
	 * Returns the current text content of the system clipboard. Cross-platform.
	 * @return Clipboard text, or empty string if unavailable
	 */
	UFUNCTION(BlueprintPure, Category="Extended Input | Clipboard")
	static FString GetStringFromClipboard();
};
