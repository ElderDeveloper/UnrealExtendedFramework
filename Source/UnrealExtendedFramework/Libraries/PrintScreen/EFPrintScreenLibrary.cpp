// Fill out your copyright notice in the Description page of Project Settings.


#include "EFPrintScreenLibrary.h"

#include "Kismet/KismetSystemLibrary.h"
#include "UnrealExtendedFramework/Libraries/Conversion/EFConversionLibrary.h"


void UEFPrintScreenLibrary::PrintZeroString(const UObject* WorldContextObject, FString String, float Time,FLinearColor Color)
{
	if (!WorldContextObject) return;
	UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),String,true,true,Color,Time);
}




void UEFPrintScreenLibrary::PrintStringAllObjectArray(const UObject* WorldContextObject,TArray<UObject*>& Array, FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
	{
		if(IsValid(i)) UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+i->GetName()+Postfix),true,true,Color,Time);
		else
			UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+"Object Is Invalid "+Postfix),true,true,Color,Time);
	}
		
}




void UEFPrintScreenLibrary::PrintStringAllIntArray(const UObject* WorldContextObject, TArray<int32>& Array,FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+FString::FromInt(i)+Postfix),true,true,Color,Time);
}




void UEFPrintScreenLibrary::PrintStringAllFloatArray(const UObject* WorldContextObject, TArray<float>& Array,FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+FString::SanitizeFloat(i)+Postfix),true,true,Color,Time);
}




void UEFPrintScreenLibrary::PrintStringAllStringArray(const UObject* WorldContextObject, TArray<FString>& Array,FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+i+Postfix),true,true,Color,Time);
}




void UEFPrintScreenLibrary::PrintStringAllNameArray(const UObject* WorldContextObject, TArray<FName>& Array,FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+ i.ToString() +Postfix),true,true,Color,Time);
}




void UEFPrintScreenLibrary::PrintStringAllTextArray(const UObject* WorldContextObject, TArray<FText>& Array,FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+i.ToString()+Postfix),true,true,Color,Time);
}




void UEFPrintScreenLibrary::PrintStringAllBoolArray(const UObject* WorldContextObject, TArray<bool>& Array,FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
	{
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+UEFConversionLibrary::BoolToFString(i)+Postfix),true,true,Color,Time);
	}
		
}




void UEFPrintScreenLibrary::PrintStringAllVectorArray(const UObject* WorldContextObject, TArray<FVector>& Array,FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+i.ToString()+Postfix),true,true,Color,Time);
}




void UEFPrintScreenLibrary::PrintStringAllRotatorArray(const UObject* WorldContextObject,TArray<FRotator>& Array, FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+i.ToString()+Postfix),true,true,Color,Time);
}




void UEFPrintScreenLibrary::PrintStringAllTransformArray(const UObject* WorldContextObject,TArray<FTransform>& Array, FString Prefix, FString Postfix, float Time, FLinearColor Color)
{
	if (!WorldContextObject) return;
	
	for (const auto i : Array)
		UKismetSystemLibrary::PrintString(WorldContextObject->GetWorld(),FString(Prefix+i.ToString()+Postfix),true,true,Color,Time);
}

