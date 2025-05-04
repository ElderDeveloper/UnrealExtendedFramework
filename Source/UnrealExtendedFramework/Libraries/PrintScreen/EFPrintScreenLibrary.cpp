﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "EFPrintScreenLibrary.h"

#include "Kismet/KismetSystemLibrary.h"
#include "UnrealExtendedFramework/Libraries/Conversion/EFConversionLibrary.h"


void UEFPrintScreenLibrary::PrintZeroString(const UObject* WorldContextObject, FString String, float Time,FLinearColor Color, const bool bPrintToLog)
{
	if (!WorldContextObject) return;
	UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),String,true,bPrintToLog,Color,Time);
}
