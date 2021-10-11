// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedArrayLibrary.h"
#include "Kismet/KismetMathLibrary.h"


int32 UUEExtendedArrayLibrary::GetRandomArrayMember(const TArray<UProperty*>& TargetArray, UProperty*& Item)
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

void UUEExtendedArrayLibrary::GetArrayLastElement(const TArray<FProperty*>& TargetArray, FProperty*& Item)
{
	if(TargetArray.Num()>0)
	{
		Item =  TargetArray.Last();
	}
}


void UUEExtendedArrayLibrary::InsertionSortFloatArray(TArray<float> FloatArray, TArray<float>& SortedArray)
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

TArray<FVector> UUEExtendedArrayLibrary::FloatArrayToVectorArray(const TArray<float> FArray)
{
	TArray<FVector> OutArray;
	for (const auto i : FArray)
	{OutArray.Add(FVector(i,i,i));}
	return OutArray;
}

TArray<FVector> UUEExtendedArrayLibrary::IntArrayToVectorArray(const TArray<int32> FArray)
{
	TArray<FVector> OutArray;
	for (const auto i : FArray)
	{OutArray.Add(FVector(i,i,i));}
	return OutArray;
}

TArray<FVector> UUEExtendedArrayLibrary::ByteArrayToVectorArray(const TArray<uint8> FArray)
{
	TArray<FVector> OutArray;
	for (const auto i : FArray)
	{OutArray.Add(FVector(i,i,i));}
	return OutArray;
}

TArray<int32> UUEExtendedArrayLibrary::FloatArrayToIntArray(const TArray<float> FArray)
{
	TArray<int32> OutArray;
	for (const auto i : FArray)
	{OutArray.Add(i);}
	return OutArray;
}

TArray<float> UUEExtendedArrayLibrary::IntArrayToFloatArray(const TArray<int32> FArray)
{
	TArray<float> OutArray;
	for (const auto i : FArray)
	{OutArray.Add(i);}
	return OutArray;
}
