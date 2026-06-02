// Fill out your copyright notice in the Description page of Project Settings.


#include "UEFArrayLibrary.h"

#include "UObject/UnrealType.h"


// ================================ RANDOM ================================



void UUEFArrayLibrary::GetRandomArrayMembers(const TArray<int32>& TargetArray, TArray<int32>& Items, const int32 Amount, const bool bUnique)
{
	Items.Empty();

	if (TargetArray.Num() <= 0)
	{
		return;
	}

	if (bUnique)
	{
		// Copy source and remove-swap chosen elements to guarantee uniqueness
		TArray<int32> TempArray = TargetArray;
		const int32 ClampedAmount = FMath::Min(Amount, TempArray.Num());
		
		for (int32 i = 0; i < ClampedAmount; i++)
		{
			const int32 RandomIndex = FMath::RandRange(0, TempArray.Num() - 1);
			Items.Add(TempArray[RandomIndex]);
			TempArray.RemoveAtSwap(RandomIndex);
		}
	}
	else
	{
		for (int32 i = 0; i < Amount; i++)
		{
			const int32 RandomIndex = FMath::RandRange(0, TargetArray.Num() - 1);
			Items.Add(TargetArray[RandomIndex]);
		}
	}
}


// ================================ SORTING ================================

void UUEFArrayLibrary::SortFloatArray(const TArray<float>& FloatArray, TArray<float>& SortedArray)
{
	SortedArray = FloatArray;
	SortedArray.Sort();
}

void UUEFArrayLibrary::SortIntArray(const TArray<int32>& IntArray, TArray<int32>& SortedArray, bool bDescending)
{
	SortedArray = IntArray;
	if (bDescending)
	{
		SortedArray.Sort([](const int32& A, const int32& B) { return A > B; });
	}
	else
	{
		SortedArray.Sort();
	}
}

void UUEFArrayLibrary::SortStringArray(const TArray<FString>& StringArray, TArray<FString>& SortedArray, bool bDescending)
{
	SortedArray = StringArray;
	if (bDescending)
	{
		SortedArray.Sort([](const FString& A, const FString& B) { return A > B; });
	}
	else
	{
		SortedArray.Sort();
	}
}


// ================================ FILTER ================================

void UUEFArrayLibrary::FilterFloatArrayByRange(const TArray<float>& SourceArray, TArray<float>& FilteredArray, float Min, float Max)
{
	FilteredArray.Empty(SourceArray.Num());
	for (const float Value : SourceArray)
	{
		if (Value >= Min && Value <= Max)
		{
			FilteredArray.Add(Value);
		}
	}
}

void UUEFArrayLibrary::FilterIntArrayByRange(const TArray<int32>& SourceArray, TArray<int32>& FilteredArray, int32 Min, int32 Max)
{
	FilteredArray.Empty(SourceArray.Num());
	for (const int32 Value : SourceArray)
	{
		if (Value >= Min && Value <= Max)
		{
			FilteredArray.Add(Value);
		}
	}
}



bool UUEFArrayLibrary::FindMapValueByKey(const TMap<int32, int32>& TargetMap, const int32& Key, int32& Value)
{
	checkNoEntry();
	return false;
}

DEFINE_FUNCTION(UUEFArrayLibrary::execFindMapValueByKey)
{
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FMapProperty>(nullptr);
	const void* TargetMapPtr = Stack.MostRecentPropertyAddress;
	const FMapProperty* MapProperty = CastField<FMapProperty>(Stack.MostRecentProperty);

	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	const void* KeyPtr = Stack.MostRecentPropertyAddress;
	const FProperty* KeyProperty = Stack.MostRecentProperty;

	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	void* ValuePtr = Stack.MostRecentPropertyAddress;
	const FProperty* ValueProperty = Stack.MostRecentProperty;

	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)RESULT_PARAM = GenericFindMapValueByKey(TargetMapPtr, MapProperty, KeyPtr, KeyProperty, ValuePtr, ValueProperty);
	P_NATIVE_END;
}

bool UUEFArrayLibrary::GenericFindMapValueByKey(const void* TargetMap, const FMapProperty* MapProperty,
	const void* KeyPtr, const FProperty* KeyProperty, void* ValuePtr, const FProperty* ValueProperty)
{
	if (TargetMap == nullptr || MapProperty == nullptr || KeyPtr == nullptr || ValuePtr == nullptr || KeyProperty == nullptr || ValueProperty == nullptr)
	{
		return false;
	}

	if (!MapProperty->KeyProp->SameType(KeyProperty) || !MapProperty->ValueProp->SameType(ValueProperty))
	{
		return false;
	}

	FScriptMapHelper MapHelper(MapProperty, TargetMap);
	const int32 MapIndex = MapHelper.FindMapIndexWithKey(KeyPtr);
	if (!MapHelper.IsValidIndex(MapIndex))
	{
		MapProperty->ValueProp->InitializeValue(ValuePtr);
		return false;
	}

	MapProperty->ValueProp->CopyCompleteValue(ValuePtr, MapHelper.GetValuePtr(MapIndex));
	return true;
}
