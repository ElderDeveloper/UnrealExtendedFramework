// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/CoreUObject/Public/UObject/Interface.h"
#include "EFLoadingProcessInterface.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UEFLoadingProcessInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class UNREALEXTENDEDFRAMEWORK_API IEFLoadingProcessInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	// Checks to see if this object implements the interface, and if so asks whether or not we should
	// be currently showing a loading screen
	static bool ShouldShowLoadingScreen(UObject* TestObject, FString& OutReason);

	virtual bool ShouldShowLoadingScreen(FString& OutReason) const
	{
		return false;
	}
};
