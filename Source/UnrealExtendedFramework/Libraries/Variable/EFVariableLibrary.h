// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "EFVariableLibrary.generated.h"

/**
 * Blueprint function library providing extended variable operations for
 * rotators, vectors, transforms, averages, gameplay tags, and string utilities.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFVariableLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Rotator >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/** Adds two rotators component-wise. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Rotator+Rotator"), Category="Math|Rotator" )
	static FRotator Add_RotatorRotator(const FRotator A, const FRotator B);

	/**
	 * Divides a rotator by a float. Returns ZeroRotator if divisor is zero.
	 * @param B The divisor
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Rotator/float"), Category="Math|Rotator" )
	static FRotator Divide_RotatorFloat(const FRotator A, const float B);

	/** Subtracts rotator B from rotator A component-wise. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Rotator-Rotator"), Category="Math|Rotator" )
	static FRotator Minus_RotatorRotator(const FRotator A, const FRotator B);

	/** Returns the Yaw component of a rotator. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Rotator Yaw", CompactNodeTitle = "Yaw"), Category="Math|Rotator" )
	static float Rotator_GetYaw(const FRotator& A) { return A.Yaw; }

	/** Returns the Pitch component of a rotator. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Rotator Pitch", CompactNodeTitle = "Pitch"), Category="Math|Rotator" )
	static float Rotator_GetPitch(const FRotator& A) { return A.Pitch; }

	/** Returns the Roll component of a rotator. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Rotator Roll", CompactNodeTitle = "Roll"), Category="Math|Rotator" )
	static float Rotator_GetRoll(const FRotator& A) { return A.Roll; }


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Vector >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/** Returns true if IsBigger has a larger squared magnitude than IsLesser. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Compare Vector Sizes"), Category="Math|Vector")
	static bool CompareVectorSizes(const FVector& IsBigger, const FVector& IsLesser);

	/** Returns the X component of a vector. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Vector X", CompactNodeTitle = "X"), Category="Math|Vector" )
	static float Vector_GetX(const FVector& A) { return A.X; }

	/** Returns the Y component of a vector. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Vector Y", CompactNodeTitle = "Y"), Category="Math|Vector" )
	static float Vector_GetY(const FVector& A) { return A.Y; }

	/** Returns the Z component of a vector. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Vector Z", CompactNodeTitle = "Z"), Category="Math|Vector" )
	static float Vector_GetZ(const FVector& A) { return A.Z; }

	/**
	 * Normalizes a vector and scales it.
	 * @param Tolerance Normalization tolerance
	 * @param Scale Scaling factor applied after normalization
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Normalize Scaled"), Category="Math|Vector" )
	static FVector Vector_NormalizeScaled(const FVector& Vector, float Tolerance = 0.0001, float Scale = 1);

	/**
	 * Returns two vectors offset from VectorRef along the Z axis.
	 * @param UpValue Z offset added for UpVector
	 * @param DownValue Z offset added for DownVector (typically negative)
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Vector Up Down"), Category="Math|Vector" )
	static void VectorGetUpDownValues(const FVector& VectorRef, float UpValue, float DownValue, FVector& UpVector, FVector& DownVector);

	/** Creates a vector with all components set to the given int value. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Int To Vector"), Category="Math|Vector" )
	static FVector IntToVector(const int32 Value) { return FVector(Value, Value, Value); }

	/** Creates a vector with all components set to the given float value. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Float To Vector"), Category="Math|Vector" )
	static FVector FloatToVector(const float Value) { return FVector(Value, Value, Value); }

	/** Creates a vector with all components set to the given byte value. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte To Vector"), Category="Math|Vector" )
	static FVector ByteToVector(const uint8 Value) { return FVector(Value, Value, Value); }


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Transform >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/** Adds two transforms component-wise (location + location, rotation + rotation, scale + scale). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Transform + Transform", CompactNodeTitle = "Transform+"), Category="Math|Transform" )
	static FTransform Add_TransformTransform(const FTransform A, const FTransform B);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Average >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/** Returns the arithmetic average of a float array. Returns 0 for an empty array. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Average Floats"), Category="Math|Float")
	static float Average_Floats(const TArray<float>& Array);

	/** Returns the arithmetic average of an integer array (integer division). Returns 0 for an empty array. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Average Integers", CompactNodeTitle = "IntegerArrayAverage"), Category="Math|Integer")
	static int32 Average_Integers(const TArray<int32>& Array);

	/**
	 * Returns the weighted average of an array of values.
	 * Each value is multiplied by its corresponding weight, then divided by the total weight.
	 * Arrays must be the same length. Returns 0 if arrays are empty or mismatched.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Weighted Average"), Category="Math|Float")
	static float WeightedAverage(const TArray<float>& Values, const TArray<float>& Weights);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< String Utilities >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/** Formats an FIntPoint as "X{Separator}Y" (e.g. "1920x1080"). */
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|String")
	static FString GetFIntPointAsString(const FIntPoint& Point, const FString& Separator = "x");

	/**
	 * Returns the current screen resolution as a formatted string and outputs the UserSettings.
	 * @param UserSettings Output: the game user settings object
	 */
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|String")
	static FString GetScreenResolutionAsString(UGameUserSettings*& UserSettings, const FString& Separator = "x");


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Gameplay Tag >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/** Converts an FName to a FGameplayTag. Logs a warning if the tag is invalid. */
	UFUNCTION(BlueprintPure, meta=(CompactNodeTitle="ToGameplayTag"), Category = "VariableLibrary|GameplayTag")
	static FGameplayTag GetGameplayTagFromName(const FName& Name);

	/** Converts an FString to a FGameplayTag. Logs a warning if the tag is invalid. */
	UFUNCTION(BlueprintPure, meta=(CompactNodeTitle="ToGameplayTag"), Category = "VariableLibrary|GameplayTag")
	static FGameplayTag GetGameplayTagFromString(const FString& Name);

	/** Converts an FText to a FGameplayTag. Logs a warning if the tag is invalid. */
	UFUNCTION(BlueprintPure, meta=(CompactNodeTitle="ToGameplayTag"), Category = "VariableLibrary|GameplayTag")
	static FGameplayTag GetGameplayTagFromText(const FText& Name);

	/** Converts FText to FName. */
	UFUNCTION(BlueprintPure, meta=(CompactNodeTitle="ToName"), Category = "VariableLibrary|GameplayTag")
	static FName GetNameFromText(const FText& Name);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Password >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Generates a random password from the selected character sets.
	 * @param Length Desired password length
	 * @param bIncludeNumbers Include digits 0-9
	 * @param bIncludeSymbols Include special characters
	 * @param bIncludeUppercase Include A-Z
	 * @param bIncludeLowercase Include a-z
	 * @return Generated password, or empty string if no character sets selected
	 */
	UFUNCTION(BlueprintPure, Category = "VariableLibrary|Password")
	static FString GetRandomPassword(int32 Length = 6, bool bIncludeNumbers = true, bool bIncludeSymbols = false, bool bIncludeUppercase = false, bool bIncludeLowercase = false);
};
