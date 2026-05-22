// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/Data/EFEnums.h"
#include "EFConditionLibrary.generated.h"


/**
 * Blueprint function library providing compact condition-check nodes for
 * common comparisons against zero, minus-one, ranges, and near-equality.
 * Designed to reduce Blueprint node clutter for frequent checks.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFConditionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Float >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#pragma region Float

	/** Returns true if Value is strictly greater than zero. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = ">0", CompactNodeTitle = ">0", BlueprintThreadSafe), Category="Condition|Float")
	static bool FloatIsBiggerThanZero(const float Value) { return Value > 0.f; }

	/** Returns true if Value is strictly less than zero. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "<0", CompactNodeTitle = "<0", BlueprintThreadSafe), Category="Condition|Float")
	static bool FloatIsLesserThanZero(const float Value) { return Value < 0.f; }

	/** Returns true if Value is nearly zero (within KINDA_SMALL_NUMBER tolerance). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "==0", CompactNodeTitle = "==0", BlueprintThreadSafe), Category="Condition|Float")
	static bool FloatIsEqualToZero(const float Value) { return FMath::IsNearlyZero(Value); }
	
	/** Returns true if Value is NOT nearly zero (outside KINDA_SMALL_NUMBER tolerance). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "!=0", CompactNodeTitle = "!=0", BlueprintThreadSafe), Category="Condition|Float")
	static bool FloatIsNotEqualToZero(const float Value) { return !FMath::IsNearlyZero(Value); }
	
	/** Returns true if Value is greater than or nearly equal to zero. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = ">=0", CompactNodeTitle = ">=0", BlueprintThreadSafe), Category="Condition|Float")
	static bool FloatIsBiggerThanAndEqualZero(const float Value) { return Value >= 0.f; }
	
	/** Returns true if Value is less than or nearly equal to zero. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "<=0", CompactNodeTitle = "<=0", BlueprintThreadSafe), Category="Condition|Float")
	static bool FloatIsLesserThanAndEqualZero(const float Value) { return Value <= 0.f; }

	/** Returns true if Value is nearly equal to -1 (within KINDA_SMALL_NUMBER tolerance). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "=-1", CompactNodeTitle = "=-1", BlueprintThreadSafe), Category="Condition|Float")
	static bool FloatIsEqualToMinusOne(const float Value) { return FMath::IsNearlyEqual(Value, -1.f); }
	
	/** Returns true if Value is NOT nearly equal to -1 (outside KINDA_SMALL_NUMBER tolerance). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "!=-1", CompactNodeTitle = "!=-1", BlueprintThreadSafe), Category="Condition|Float")
	static bool FloatIsNotEqualToMinusOne(const float Value) { return !FMath::IsNearlyEqual(Value, -1.f); }

#pragma endregion
	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Int32 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#pragma region Int32
	
	/** Returns true if Value is strictly greater than zero. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = ">0", CompactNodeTitle = ">0", BlueprintThreadSafe), Category="Condition|Int")
	static bool IntIsBiggerThanZero(const int32 Value) { return Value > 0; }

	/** Returns true if Value is strictly less than zero. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "<0", CompactNodeTitle = "<0", BlueprintThreadSafe), Category="Condition|Int")
	static bool IntIsLesserThanZero(const int32 Value) { return Value < 0; }

	/** Returns true if Value is exactly zero. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "=0", CompactNodeTitle = "=0", BlueprintThreadSafe), Category="Condition|Int")
	static bool IntIsEqualToZero(const int32 Value) { return Value == 0; }
	
	/** Returns true if Value is not zero. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "!=0", CompactNodeTitle = "!=0", BlueprintThreadSafe), Category="Condition|Int")
	static bool IntIsNotEqualToZero(const int32 Value) { return Value != 0; }
	
	/** Returns true if Value is greater than or equal to zero (non-negative). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = ">=0", CompactNodeTitle = ">=0", BlueprintThreadSafe), Category="Condition|Int")
	static bool IntIsBiggerThanAndEqualZero(const int32 Value) { return Value >= 0; }
	
	/** Returns true if Value is less than or equal to zero. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "<=0", CompactNodeTitle = "<=0", BlueprintThreadSafe), Category="Condition|Int")
	static bool IntIsLesserThanAndEqualZero(const int32 Value) { return Value <= 0; }

	/** Returns true if Value is exactly -1. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "=-1", CompactNodeTitle = "=-1", BlueprintThreadSafe), Category="Condition|Int")
	static bool IntIsEqualToMinusOne(const int32 Value) { return Value == -1; }
	
	/** Returns true if Value is not -1. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "!=-1", CompactNodeTitle = "!=-1", BlueprintThreadSafe), Category="Condition|Int")
	static bool IntIsNotEqualToMinusOne(const int32 Value) { return Value != -1; }

#pragma endregion


#pragma region Utility

	/**
	 * Returns true if the game is currently running inside the Unreal Editor.
	 * Evaluated at compile time via WITH_EDITOR preprocessor directive.
	 */
	UFUNCTION(BlueprintPure, Category="Condition|Utility")
	static bool IsPlayingInEditor();

#pragma endregion

};
