// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "UEExtendedArrayLibrary.generated.h"


UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedArrayLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Random Array Member", CompactNodeTitle = "Random Member", ArrayParm = "TargetArray", ArrayTypeDependentParams = "Item", BlueprintThreadSafe), Category="Math|Library")
	static int32 GetRandomArrayMember(const TArray<UProperty*>& TargetArray, UProperty*& Item);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Array Last Element", CompactNodeTitle = "Last Element", ArrayParm = "TargetArray", ArrayTypeDependentParams = "Item", BlueprintThreadSafe), Category="Math|Library")
	static void GetArrayLastElement(const TArray<UProperty*>& TargetArray, UProperty*& Item);


	
};
