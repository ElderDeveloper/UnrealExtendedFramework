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
	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Random Array Member", CompactNodeTitle = "Random Member", ArrayParm = "TargetArray", ArrayTypeDependentParams = "Item", BlueprintThreadSafe), Category="Math|Library")
	static int32 GetRandomArrayMember(const TArray<UProperty*>& TargetArray, UProperty*& Item);
	static int32 GetRandomArrayIndex(const int32 ArrayLenght , const int32 StartIndex = 0);
	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Array Last Element", CompactNodeTitle = "Last Element", ArrayParm = "TargetArray", ArrayTypeDependentParams = "Item", BlueprintThreadSafe), Category="Math|Library")
	static void GetArrayLastElement(const TArray<UProperty*>& TargetArray, UProperty*& Item);

	
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< SORTING >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable, Category = "Array|Sort")
	static void InsertionSortFloatArray(TArray<float> FloatArray , TArray<float>& SortedArray);

	/**
	 *	FunctionName is a function resided in calling object that needs to have two struct input and bool return for sorting like Health variables in structs
	 *	return Struct1.Health > Struct2.Health
	 **/
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (DisplayName = "Sort Any Struct", DefaultToSelf = "Object", ArrayParm = "CustomStruct", AdvancedDisplay = "Object"), Category = "Utilities|UserDefinedStruct")
	static void SortUserDefinedStructArray(const TArray<int32>& CustomStruct, UObject* Object, FName FunctionName);
	static void Generic_SortUserDefinedStructArray(void* TargetArray, const UArrayProperty* ArrayProp, UObject* OwnerObject, UFunction* SortRuleFunc);
	DECLARE_FUNCTION(execSortUserDefinedStructArray)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<UArrayProperty>(NULL);
		void* ArrayAAddr = Stack.MostRecentPropertyAddress;
		UArrayProperty* ArrayProperty = CastField<UArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_GET_OBJECT(UObject, OwnerObject);
		P_GET_PROPERTY(UNameProperty, FunctionName);
		if (!OwnerObject)
		{
			return;
		}
		UFunction* const Func = OwnerObject->FindFunction(FunctionName);
		if ((!Func || (Func->NumParms != 3)))
		{
			return;
		}

		P_FINISH;

		P_NATIVE_BEGIN;
		Generic_SortUserDefinedStructArray(ArrayAAddr, ArrayProperty, OwnerObject, Func);
		P_NATIVE_END;
	}

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

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Experiment >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/*
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Array Last Element", CompactNodeTitle = "Last Element", ArrayParm = "TargetArray", ArrayTypeDependentParams = "Item", BlueprintThreadSafe), Category="Math|Library")
	static void GetIsValidArray(const TArray<UProperty*>& TargetArray, UProperty*& Item);
	*/

	/*
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Array Last Element", CompactNodeTitle = "Last Element", ArrayParm = "TargetArray", ArrayTypeDependentParams = "Item", BlueprintThreadSafe), Category="Math|Library")
	static bool GetIsValidArray(const TArray<UProperty*>& TargetArray, UProperty*& Item);
	*/

	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Condition >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Get Value If Index Valid", ArrayParm = "Array", ArrayTypeDependentParams = "Item" , ExpandEnumAsExecs = "OutPins"), Category="Math|Library")
	static void ExtendedIsValidIndex(const TArray<UProperty*>& Array, const int32 index, TEnumAsByte<EFConditionOutput>& OutPins, UProperty*& Item );

	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Is Array Not Empty", CompactNodeTitle = "Is Array Not Empty", ArrayParm = "Array" , ExpandEnumAsExecs = "OutPins"), Category="Math|Library")
	static void IsArrayNotEmpty(const TArray<UProperty*>& Array, TEnumAsByte<EFConditionOutput>& OutPins );
	

};
