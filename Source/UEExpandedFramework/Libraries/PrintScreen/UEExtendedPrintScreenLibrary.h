// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UEExtendedPrintScreenLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedPrintScreenLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject",DisplayName = "Print Zero String"), Category="Math|Library")
	static void PrintZeroString(const UObject* WorldContextObject ,FString String = "" , float Time = 0 , FLinearColor Color = FLinearColor::Blue);

	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject",DisplayName = "Print String All Object Array"), Category="Math|Library")
	static void PrintStringAllObjectArray(const UObject* WorldContextObject ,UPARAM(ref) TArray<UObject*>& Array ,FString Prefix = "" , FString Postfix = "" , float Time = 0 , FLinearColor Color = FLinearColor::Blue);

	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject",DisplayName = "Print String All Int Array"), Category="Math|Library")
	static void PrintStringAllIntArray(const UObject* WorldContextObject ,UPARAM(ref) TArray<int32>& Array ,FString Prefix = "" , FString Postfix = "" , float Time = 0 , FLinearColor Color = FLinearColor::Blue);

	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject",DisplayName = "Print String All Float Array"), Category="Math|Library")
	static void PrintStringAllFloatArray(const UObject* WorldContextObject ,UPARAM(ref) TArray<float>& Array ,FString Prefix = "" , FString Postfix = "" , float Time = 0 , FLinearColor Color = FLinearColor::Blue);

	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject",DisplayName = "Print String All String Array"), Category="Math|Library")
	static void PrintStringAllStringArray(const UObject* WorldContextObject ,UPARAM(ref) TArray<FString>& Array ,FString Prefix = "" , FString Postfix = "" , float Time = 0 , FLinearColor Color = FLinearColor::Blue);

	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject",DisplayName = "Print String All Name Array"), Category="Math|Library")
	static void PrintStringAllNameArray(const UObject* WorldContextObject ,UPARAM(ref) TArray<FName>& Array ,FString Prefix = "" , FString Postfix = "" , float Time = 0 , FLinearColor Color = FLinearColor::Blue);

	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject",DisplayName = "Print String All Text Array"), Category="Math|Library")
	static void PrintStringAllTextArray(const UObject* WorldContextObject ,UPARAM(ref) TArray<FText>& Array ,FString Prefix = "" , FString Postfix = "" , float Time = 0 , FLinearColor Color = FLinearColor::Blue);

	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject",DisplayName = "Print String All Bool Array"), Category="Math|Library")
	static void PrintStringAllBoolArray(const UObject* WorldContextObject ,UPARAM(ref) TArray<bool>& Array ,FString Prefix = "" , FString Postfix = "" , float Time = 0 , FLinearColor Color = FLinearColor::Blue);

	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject",DisplayName = "Print String All Vector Array"), Category="Math|Library")
	static void PrintStringAllVectorArray(const UObject* WorldContextObject ,UPARAM(ref) TArray<FVector>& Array ,FString Prefix = "" , FString Postfix = "" , float Time = 0 , FLinearColor Color = FLinearColor::Blue);

	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject",DisplayName = "Print String All Rotator Array"), Category="Math|Library")
	static void PrintStringAllRotatorArray(const UObject* WorldContextObject ,UPARAM(ref) TArray<FRotator>& Array ,FString Prefix = "" , FString Postfix = "" , float Time = 0 , FLinearColor Color = FLinearColor::Blue);

	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject",DisplayName = "Print String All Transform Array"), Category="Math|Library")
	static void PrintStringAllTransformArray(const UObject* WorldContextObject ,UPARAM(ref) TArray<FTransform>& Array ,FString Prefix = "" , FString Postfix = "" , float Time = 0 , FLinearColor Color = FLinearColor::Blue);
};
