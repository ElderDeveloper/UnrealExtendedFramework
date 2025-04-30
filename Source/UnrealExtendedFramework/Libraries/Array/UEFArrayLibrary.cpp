﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "UEFArrayLibrary.h"

#include "Kismet/KismetMathLibrary.h"

int32 UUEFArrayLibrary::GetRandomArrayMember(const TArray<int32>& TargetArray, int32& Item)
{
	if (TargetArray.Num() > 0)
	{
		// Generate a random index
		int32 RandomIndex = UKismetMathLibrary::RandomIntegerInRange(0, TargetArray.Num() - 1);
		Item = TargetArray[RandomIndex];
		return RandomIndex;
	}
	return -1;
}


void UUEFArrayLibrary::GetRandomArrayMembers(const TArray<int32>& TargetArray, TArray<int32>& Items,const int32 Amount, const bool bUnique)
{

	if(TargetArray.Num() > 0)
	{
		if(TargetArray.Num() >= Amount && bUnique)
		{
			TArray<int32> TempArray = TargetArray;
			
			while(Items.Num() < Amount)
			{
				const int32 j= UKismetMathLibrary::RandomIntegerInRange(0,TempArray.Max());
				
				if (TempArray.IsValidIndex(j))
				{
                    // TODO: There could be more performant way to do this
					Items.AddUnique(TempArray[j]);
				}
			}
		}
		else
		{
			for (int32 i = 0; i < Amount; i++)
			{
				int32 j= UKismetMathLibrary::RandomIntegerInRange(0,TargetArray.Max());
				if (TargetArray.IsValidIndex(j))
				{
					Items.Add(TargetArray[j]);
				}
			}
		}
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

void UUEFArrayLibrary::ExtendedIsValidIndex(const TArray<int32>& Array, const int32 index,TEnumAsByte<EFConditionOutput>& OutPins, int32& Item)
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

