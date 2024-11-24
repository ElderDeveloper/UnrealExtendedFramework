// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/Data/EFEnums.h"
#include "UObject/Object.h"
#include "UEFArrayLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UUEFArrayLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Random Array Member", CompactNodeTitle = "Random Member", ArrayParm = "TargetArray", ArrayTypeDependentParams = "Item", BlueprintThreadSafe), Category = "Array|Library")
	static int32 GetRandomArrayMember(const TArray<int32>& TargetArray, int32& Item);
	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Random Array Members", CompactNodeTitle = "Random Members", ArrayParm = "TargetArray", ArrayTypeDependentParams = "Items", BlueprintThreadSafe), Category="Array|Library")
	static void GetRandomArrayMembers(const TArray<int32>& TargetArray, TArray<int32>& Items , const int32 Amount = 1 , const bool bUnique = false); 
	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Array Last Element", CompactNodeTitle = "Last Element", ArrayParm = "TargetArray", ArrayTypeDependentParams = "Item", BlueprintThreadSafe), Category="Array|Library")
	static void GetArrayLastElement(const TArray<int32>& TargetArray, int32& Item);

	
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< SORTING >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable, Category = "Array|Sort")
	static void InsertionSortFloatArray(TArray<float> FloatArray , TArray<float>& SortedArray);
	

	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Translate To Vector >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Float Array To Vector Array", CompactNodeTitle = "***"), Category="Array|Vector")
	static TArray<FVector> FloatArrayToVectorArray(UPARAM(ref) const TArray<float>& FArray);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Int Array To Vector Array", CompactNodeTitle = "***"), Category="Array|Vector")
	static TArray<FVector> IntArrayToVectorArray(UPARAM(ref) const TArray<int32>& FArray);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte Array To Vector Array", CompactNodeTitle = "***"), Category="Array|Vector")
	static TArray<FVector> ByteArrayToVectorArray(UPARAM(ref) const TArray<uint8>& FArray);


	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Translate >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Float Array To Int Array", CompactNodeTitle = "***"), Category="Array|Int")
	static TArray<int32> FloatArrayToIntArray(UPARAM(ref) const TArray<float>& FArray);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Int Array To Float Array", CompactNodeTitle = "***"), Category="Array|Float")
	static TArray<float> IntArrayToFloatArray(UPARAM(ref) const TArray<int32>& FArray);

	
	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Condition >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Get Value If Index Valid", ArrayParm = "Array", ArrayTypeDependentParams = "Item" , ExpandEnumAsExecs = "OutPins"), Category="Math|Library")
	static void ExtendedIsValidIndex(const TArray<int32>& Array, const int32 index, TEnumAsByte<EFConditionOutput>& OutPins, int32& Item );

	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Is Array Not Empty", CompactNodeTitle = "Is Array Not Empty", ArrayParm = "Array" , ExpandEnumAsExecs = "OutPins"), Category="Math|Library")
	static void IsArrayNotEmpty(const TArray<UProperty*>& Array, TEnumAsByte<EFConditionOutput>& OutPins );

};
