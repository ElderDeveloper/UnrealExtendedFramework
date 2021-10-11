// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "UEExtendedArrayLibrary.generated.h"


UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedArrayLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Random Array Member", CompactNodeTitle = "Random Member", ArrayParm = "TargetArray", ArrayTypeDependentParams = "Item", BlueprintThreadSafe), Category="Math|Library")
	static int32 GetRandomArrayMember(const TArray<UProperty*>& TargetArray, UProperty*& Item);

	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Array Last Element", CompactNodeTitle = "Last Element", ArrayParm = "TargetArray", ArrayTypeDependentParams = "Item", BlueprintThreadSafe), Category="Math|Library")
	static void GetArrayLastElement(const TArray<UProperty*>& TargetArray, UProperty*& Item);

	
	UFUNCTION(BlueprintCallable)
	static void InsertionSortFloatArray(TArray<float> FloatArray , TArray<float>& SortedArray);


	UFUNCTION(BlueprintPure, meta=(DisplayName = "Float Array To Vector Array", CompactNodeTitle = "***"), Category="Array|Vector")
	static TArray<FVector> FloatArrayToVectorArray(const TArray<float> FArray);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Int Array To Vector Array", CompactNodeTitle = "***"), Category="Array|Vector")
	static TArray<FVector> IntArrayToVectorArray(const TArray<int32> FArray);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte Array To Vector Array", CompactNodeTitle = "***"), Category="Array|Vector")
	static TArray<FVector> ByteArrayToVectorArray(const TArray<uint8> FArray);


	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Float Array To Int Array", CompactNodeTitle = "***"), Category="Array|Int")
	static TArray<int32> FloatArrayToIntArray(const TArray<float> FArray);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Int Array To Float Array", CompactNodeTitle = "***"), Category="Array|Float")
	static TArray<float> IntArrayToFloatArray(const TArray<int32> FArray);

	
};
