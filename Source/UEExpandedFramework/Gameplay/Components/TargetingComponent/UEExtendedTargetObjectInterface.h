// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UEExtendedTargetObjectInterface.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UUEExtendedTargetObjectInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class UEEXPANDEDFRAMEWORK_API IUEExtendedTargetObjectInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Target System")
	bool IsTargetable() const;
};
