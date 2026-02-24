// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/Data/EFEnums.h"
#include "UObject/Object.h"
#include "UEFArrayLibrary.generated.h"

/**
 * Blueprint function library providing extended array utilities including
 * random selection, sorting, shuffling, filtering, and validation helpers.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UUEFArrayLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< RANDOM >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Retrieves a random member from the given array.
	 * @param TargetArray The array to read from
	 * @param Item The randomly selected item (default-initialised if the array is empty)
	 * @return The index of the selected item, or -1 if the array is empty
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Random Array Member", CompactNodeTitle = "Random Member", ArrayParm = "TargetArray", ArrayTypeDependentParams = "Item", BlueprintThreadSafe), Category = "Array|Library")
	static int32 GetRandomArrayMember(const TArray<int32>& TargetArray, int32& Item);
	
	/**
	 * Retrieves a specified amount of random members from the given array.
	 * When bUnique is true, Amount is clamped to the array size so that no
	 * duplicates are produced.
	 * @param TargetArray The array to read from
	 * @param Items The randomly selected items (empty if source array is empty)
	 * @param Amount The number of items to retrieve
	 * @param bUnique When true, each item appears at most once in the result
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Random Array Members", CompactNodeTitle = "Random Members", ArrayParm = "TargetArray", ArrayTypeDependentParams = "Items", BlueprintThreadSafe), Category="Array|Library")
	static void GetRandomArrayMembers(const TArray<int32>& TargetArray, TArray<int32>& Items, const int32 Amount = 1, const bool bUnique = false); 
	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< SORTING >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	
	/**
	 * Sorts a float array in ascending order using TArray::Sort (Introsort).
	 * @param FloatArray The source array
	 * @param SortedArray The resulting sorted copy
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Sort Float Array"), Category = "Array|Sort")
	static void SortFloatArray(const TArray<float>& FloatArray, TArray<float>& SortedArray);

	/**
	 * Sorts an integer array in ascending or descending order.
	 * @param IntArray The source array
	 * @param SortedArray The resulting sorted copy
	 * @param bDescending If true, sorts largest-first; ascending otherwise
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Sort Int Array"), Category = "Array|Sort")
	static void SortIntArray(const TArray<int32>& IntArray, TArray<int32>& SortedArray, bool bDescending = false);

	/**
	 * Sorts a string array in lexicographic order.
	 * @param StringArray The source array
	 * @param SortedArray The resulting sorted copy
	 * @param bDescending If true, sorts Z-A; A-Z otherwise
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Sort String Array"), Category = "Array|Sort")
	static void SortStringArray(const TArray<FString>& StringArray, TArray<FString>& SortedArray, bool bDescending = false);

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< SHUFFLE >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Randomly shuffles the elements of any array in-place using the Fisher-Yates algorithm.
	 * Works with arrays of any type via CustomThunk.
	 * @param TargetArray The array to shuffle (modified in-place)
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (DisplayName = "Shuffle Array", CompactNodeTitle = "Shuffle", ArrayParm = "TargetArray"), Category = "Array|Library")
	static void ShuffleArray(const TArray<int32>& TargetArray);

	/** CustomThunk implementation for ShuffleArray. */
	DECLARE_FUNCTION(execShuffleArray);

	/** Internal generic shuffle implementation operating on raw script array data. */
	static void GenericArray_Shuffle(void* TargetArray, const FArrayProperty* ArrayProperty);

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< FILTER >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Filters a float array, keeping only values within the [Min, Max] range (inclusive).
	 * @param SourceArray The array to filter
	 * @param FilteredArray The resulting filtered array
	 * @param Min Minimum value (inclusive)
	 * @param Max Maximum value (inclusive)
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Filter Float Array By Range"), Category = "Array|Filter")
	static void FilterFloatArrayByRange(const TArray<float>& SourceArray, TArray<float>& FilteredArray, float Min, float Max);

	/**
	 * Filters an integer array, keeping only values within the [Min, Max] range (inclusive).
	 * @param SourceArray The array to filter
	 * @param FilteredArray The resulting filtered array
	 * @param Min Minimum value (inclusive)
	 * @param Max Maximum value (inclusive)
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Filter Int Array By Range"), Category = "Array|Filter")
	static void FilterIntArrayByRange(const TArray<int32>& SourceArray, TArray<int32>& FilteredArray, int32 Min, int32 Max);

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< CONDITION >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	
	/**
	 * Outputs True and the Item if the index is valid in the array, False otherwise.
	 * @param Array The array to check
	 * @param index The index to validate
	 * @param OutPins Execution pin - True if valid, False otherwise
	 * @param Item The item at the given index (only meaningful when OutPins is True)
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Get Value If Index Valid", ArrayParm = "Array", ArrayTypeDependentParams = "Item" , ExpandEnumAsExecs = "OutPins"), Category="Math|Library")
	static void ExtendedIsValidIndex(const TArray<int32>& Array, const int32 index, TEnumAsByte<EFConditionOutput>& OutPins, int32& Item );

	/**
	 * Outputs True if the array contains at least one element, False if empty.
	 * @param Array The array to check
	 * @param OutPins Execution pin - True if not empty, False if empty
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Is Array Not Empty", CompactNodeTitle = "Is Array Not Empty", ArrayParm = "Array" , ExpandEnumAsExecs = "OutPins"), Category="Math|Library")
	static void IsArrayNotEmpty(const TArray<int32>& Array, TEnumAsByte<EFConditionOutput>& OutPins );

};
