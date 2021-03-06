// Fill out your copyright notice in the Description page of Project Settings.


#include "UEFArrayLibrary.h"

#include "Kismet/KismetMathLibrary.h"


int32 UUEFArrayLibrary::GetRandomArrayMember(const TArray<UProperty*>& TargetArray, UProperty*& Item)
{
	if(TargetArray.Num()>0)
	{
		int32 i= UKismetMathLibrary::RandomIntegerInRange(0,TargetArray.Max());
		if (TargetArray.IsValidIndex(i))
		{
			Item=TargetArray[i];
			return true;
		}
	}
	return false;
}

int32 UUEFArrayLibrary::GetRandomArrayIndex(const int32 ArrayLenght, const int32 StartIndex)
{
	return UKismetMathLibrary::RandomIntegerInRange(StartIndex,ArrayLenght);
}

void UUEFArrayLibrary::GetArrayLastElement(const TArray<FProperty*>& TargetArray, FProperty*& Item)
{
	if(TargetArray.Num()>0)
	{
		Item =  TargetArray.Last();
	}
}


void UUEFArrayLibrary::InsertionSortFloatArray(TArray<float> FloatArray, TArray<float>& SortedArray)
{
	float i , key , j;

	for (i = 1 ; i<FloatArray.Num() ; i++ )
	{
		key = FloatArray[i];
		j = i-1;

		while ( j >= 0  && FloatArray[j] > key )
		{
			FloatArray[j +1] = FloatArray[j];
			j = j-1;
		}
		FloatArray[j+1] = key;
	}

	SortedArray = FloatArray;
}



void UUEFArrayLibrary::Generic_SortUserDefinedStructArray(void* TargetArray, const FArrayProperty* ArrayProp,UObject* OwnerObject, UFunction* SortRuleFunc)
{
	
	if (!SortRuleFunc || !OwnerObject || !TargetArray)
	{
		return;
	}
	
	UBoolProperty* ReturnParam = CastField<UBoolProperty>(SortRuleFunc->GetReturnProperty());
	if (!ReturnParam)
	{
		return;
	}
	// Begin sort array
	FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
	UProperty* InnerProp = ArrayProp->Inner;

	const int32 Len = ArrayHelper.Num();
	const int32 PropertySize = InnerProp->ElementSize * InnerProp->ArrayDim;

	uint8* Parameters = (uint8*)FMemory::Malloc(PropertySize * 2 + 1);

	for (int32 i = 0; i < Len; i++)
	{
		for (int32 j = 0; j < Len - i - 1; j++)
		{
			FMemory::Memzero(Parameters, PropertySize * 2 + 1);
			InnerProp->CopyCompleteValueFromScriptVM(Parameters, ArrayHelper.GetRawPtr(j));
			InnerProp->CopyCompleteValueFromScriptVM(Parameters + PropertySize, ArrayHelper.GetRawPtr(j + 1));
			OwnerObject->ProcessEvent(SortRuleFunc, Parameters);
			if (ReturnParam && ReturnParam->GetPropertyValue(Parameters + PropertySize * 2))
			{
				ArrayHelper.SwapValues(j, j + 1);
			}
		}

	}
	FMemory::Free(Parameters);
	// end sort array
}






TArray<FVector> UUEFArrayLibrary::FloatArrayToVectorArray(UPARAM(ref) const TArray<float>& FArray)
{
	TArray<FVector> OutArray;
	for (const auto i : FArray)
	{OutArray.Add(FVector(i,i,i));}
	return OutArray;
}

TArray<FVector> UUEFArrayLibrary::IntArrayToVectorArray(UPARAM(ref) const TArray<int32>& FArray)
{
	TArray<FVector> OutArray;
	for (const auto i : FArray)
	{OutArray.Add(FVector(i,i,i));}
	return OutArray;
}

TArray<FVector> UUEFArrayLibrary::ByteArrayToVectorArray(UPARAM(ref) const TArray<uint8>& FArray)
{
	TArray<FVector> OutArray;
	for (const auto i : FArray)
	{OutArray.Add(FVector(i,i,i));}
	return OutArray;
}

TArray<int32> UUEFArrayLibrary::FloatArrayToIntArray(UPARAM(ref) const TArray<float>& FArray)
{
	TArray<int32> OutArray;
	for (const auto i : FArray)
	{OutArray.Add(i);}
	return OutArray;
}

TArray<float> UUEFArrayLibrary::IntArrayToFloatArray(UPARAM(ref) const TArray<int32>& FArray)
{
	TArray<float> OutArray;
	for (const auto i : FArray)
	{OutArray.Add(i);}
	return OutArray;
}


void UUEFArrayLibrary::ExtendedIsValidIndex(const TArray<FProperty*>& Array, const int32 index, TEnumAsByte<EFConditionOutput>& OutPins, UProperty*& Item)
{
	if (Array.IsValidIndex(index))
	{
		Item = Array[0];
		OutPins = UEF_True;
		return;
	}
	OutPins = UEF_False;
}

void UUEFArrayLibrary::IsArrayNotEmpty(const TArray<FProperty*>& Array, TEnumAsByte<EFConditionOutput>& OutPins)
{
	if (Array.IsValidIndex(0))
	{
		OutPins = UEF_True;
		return;
	}
	OutPins = UEF_False;
}


