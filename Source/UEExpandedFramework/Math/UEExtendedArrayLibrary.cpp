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
