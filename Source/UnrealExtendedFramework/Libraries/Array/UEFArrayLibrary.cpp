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

void UUEFArrayLibrary::GetArrayLastElement(const TArray<UProperty*>& TargetArray, UProperty*& Item)
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

void UUEFArrayLibrary::ExtendedIsValidIndex(const TArray<UProperty*>& Array, const int32 index,TEnumAsByte<EFConditionOutput>& OutPins, UProperty*& Item)
{
	if (Array.IsValidIndex(index))
	{
		Item = Array[0];
		OutPins = UEF_True;
		return;
	}
	OutPins = UEF_False;
}

void UUEFArrayLibrary::IsArrayNotEmpty(const TArray<UProperty*>& Array, TEnumAsByte<EFConditionOutput>& OutPins)
{
	if (Array.IsValidIndex(0))
	{
		OutPins = UEF_True;
		return;
	}
	OutPins = UEF_False;
}
