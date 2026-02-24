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

	/**
	 * Returns true if Value is within the [Min, Max] range (inclusive), using near-equality for bounds.
	 * @param Value The float to test
	 * @param Min The minimum bound (inclusive)
	 * @param Max The maximum bound (inclusive)
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Float In Range", CompactNodeTitle = "InRange", BlueprintThreadSafe), Category="Condition|Float")
	static bool IsValueInRange(const float Value, const float Min, const float Max) { return Value >= Min && Value <= Max; }

	/**
	 * Returns true if A and B are nearly equal within the specified tolerance.
	 * @param A First float
	 * @param B Second float
	 * @param Tolerance The comparison tolerance (defaults to 1)
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Nearly Equal", CompactNodeTitle = "~=", BlueprintThreadSafe), Category="Condition|Float")
	static bool IsNearlyEqual(const float A, const float B, const float Tolerance = 1) { return FMath::IsNearlyEqual(A, B, Tolerance); }

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

	/**
	 * Returns true if Value is within the [Min, Max] range (inclusive).
	 * @param Value The integer to test
	 * @param Min The minimum bound (inclusive)
	 * @param Max The maximum bound (inclusive)
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Int In Range", CompactNodeTitle = "InRange", BlueprintThreadSafe), Category="Condition|Int")
	static bool IntIsValueInRange(const int32 Value, const int32 Min, const int32 Max) { return Value >= Min && Value <= Max; }

#pragma endregion

	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Byte >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#pragma region Byte
	
	/** Returns true if Value is strictly greater than zero. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = ">0", CompactNodeTitle = ">0", BlueprintThreadSafe), Category="Condition|Byte")
	static bool ByteIsBiggerThanZero(const uint8 Value) { return Value > 0; }
	
	/** Returns true if Value is exactly zero. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "=0", CompactNodeTitle = "=0", BlueprintThreadSafe), Category="Condition|Byte")
	static bool ByteIsEqualToZero(const uint8 Value) { return Value == 0; }
	
	/** Returns true if Value is not zero. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "!=0", CompactNodeTitle = "!=0", BlueprintThreadSafe), Category="Condition|Byte")
	static bool ByteIsNotEqualToZero(const uint8 Value) { return Value != 0; }

#pragma endregion


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Utility >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#pragma region Utility

	/**
	 * Returns true if the game is currently running inside the Unreal Editor.
	 * Evaluated at compile time via WITH_EDITOR preprocessor directive.
	 */
	UFUNCTION(BlueprintPure, Category="Condition|Utility")
	static bool IsPlayingInEditor();

#pragma endregion

};
