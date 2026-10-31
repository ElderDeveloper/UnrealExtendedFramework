// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EFAsyncTestListener.generated.h"

/**
 * Stands in for a Blueprint graph in the AsyncLibrary automation tests: bound to an async node's
 * output pins, it records what fired. Not for use in games.
 */
UCLASS(Transient, NotBlueprintable, HideDropdown)
class UEFAsyncTestListener : public UObject
{
	GENERATED_BODY()

public:
	/** For pins without parameters: Delay's Finished, Loop Delay's Loop, a timeline's Completed. */
	UFUNCTION()
	void HandleEvent()
	{
		++EventCount;
	}

	/** For float pins, e.g. a float timeline's Loop. */
	UFUNCTION()
	void HandleFloat(float Value)
	{
		FloatValues.Add(Value);
	}

	/** For int32 pins, e.g. a for loop's Loop. */
	UFUNCTION()
	void HandleIndex(int32 Index)
	{
		Indices.Add(Index);
	}

	/** For a second int32 pin on the same node, e.g. a for loop's Completed. */
	UFUNCTION()
	void HandleCompletedIndex(int32 Index)
	{
		CompletedIndices.Add(Index);
	}

	int32 EventCount = 0;
	TArray<float> FloatValues;
	TArray<int32> Indices;
	TArray<int32> CompletedIndices;
};
