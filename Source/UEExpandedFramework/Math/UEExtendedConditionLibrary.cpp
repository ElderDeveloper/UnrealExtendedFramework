// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedConditionLibrary.h"

void UUEExtendedConditionLibrary::IsBiggerThanZero(const float& Value, TEnumAsByte<EConditionOutput>& OutPins)
{
	if (Value > 0)
		OutPins = OutTrue;
	else
		OutPins = OutIsFalse;
}
