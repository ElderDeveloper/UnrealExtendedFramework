// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedPrintScreenLibrary.h"

#include "Kismet/KismetSystemLibrary.h"
#include "UEExpandedFramework/Libraries/Conversion/UEExtendedConversionLibrary.h"


void UUEExtendedPrintScreenLibrary::PrintZeroString(const UObject* WorldContextObject, FString String, float Time,FLinearColor Color)
{
	if (!WorldContextObject) return;
	UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),String,true,true,Color,Time);
}

void UUEExtendedPrintScreenLibrary::PrintStringAllObjectArray(const UObject* WorldContextObject,TArray<UObject*>& Array, FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
	{
		if(IsValid(i)) UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+i->GetName()+Postfix),true,true,Color,Time);
		else
			UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+"Object Is Invalid "+Postfix),true,true,Color,Time);
	}
		
}

void UUEExtendedPrintScreenLibrary::PrintStringAllIntArray(const UObject* WorldContextObject, TArray<int32>& Array,FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+FString::FromInt(i)+Postfix),true,true,Color,Time);
}

void UUEExtendedPrintScreenLibrary::PrintStringAllFloatArray(const UObject* WorldContextObject, TArray<float>& Array,FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+FString::SanitizeFloat(i)+Postfix),true,true,Color,Time);
}

void UUEExtendedPrintScreenLibrary::PrintStringAllStringArray(const UObject* WorldContextObject, TArray<FString>& Array,FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+i+Postfix),true,true,Color,Time);
}

void UUEExtendedPrintScreenLibrary::PrintStringAllNameArray(const UObject* WorldContextObject, TArray<FName>& Array,FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+ i.ToString() +Postfix),true,true,Color,Time);
}

void UUEExtendedPrintScreenLibrary::PrintStringAllTextArray(const UObject* WorldContextObject, TArray<FText>& Array,FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+i.ToString()+Postfix),true,true,Color,Time);
}

void UUEExtendedPrintScreenLibrary::PrintStringAllBoolArray(const UObject* WorldContextObject, TArray<bool>& Array,FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+UUEExtendedConversionLibrary::BoolToFString(i)+Postfix),true,true,Color,Time);
}

void UUEExtendedPrintScreenLibrary::PrintStringAllVectorArray(const UObject* WorldContextObject, TArray<FVector>& Array,FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+i.ToString()+Postfix),true,true,Color,Time);
}

void UUEExtendedPrintScreenLibrary::PrintStringAllRotatorArray(const UObject* WorldContextObject,TArray<FRotator>& Array, FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+i.ToString()+Postfix),true,true,Color,Time);
}

void UUEExtendedPrintScreenLibrary::PrintStringAllTransformArray(const UObject* WorldContextObject,TArray<FTransform>& Array, FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+i.ToString()+Postfix),true,true,Color,Time);
}

