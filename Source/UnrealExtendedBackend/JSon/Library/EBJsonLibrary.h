// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedBackend/JSon/EBJsonData.h"
#include "UnrealExtendedFramework/Data/EFEnums.h"
#include "EBJsonLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDBACKEND_API UEBJsonLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable ,meta=(ExpandBoolAsExecs = "Success"), Category="Extended Backend | Json")
	static FExtendedJson ReadJsonFile(EFProjectDirectory DirectoryType , FString FileDirectory, bool& Success ,bool DebugDirectory = false);


	UFUNCTION(BlueprintCallable,meta=(ExpandBoolAsExecs = "Success") , Category="Extended Backend | Json")
	static FString ReadExtendedJsonAsString(EFProjectDirectory DirectoryType , FString FileDirectory , bool& Success);


	
	UFUNCTION(BlueprintCallable,meta=(ExpandBoolAsExecs = "Success") , Category="Extended Backend | Json")
	static void GetExtendedStringField(bool& Success , const FExtendedJson& ExtendedJson , const FString& FieldName, FString& OutString);

	
	UFUNCTION(BlueprintCallable,meta=(ExpandBoolAsExecs = "Success") , Category="Extended Backend | Json")
	static void GetExtendedBoolField(bool& Success , const FExtendedJson& ExtendedJson , const FString& FieldName, bool& OutBool);

	
	UFUNCTION(BlueprintCallable,meta=(ExpandBoolAsExecs = "Success") , Category="Extended Backend | Json")
	static void GetExtendedNumberField(bool& Success , const FExtendedJson& ExtendedJson , const FString& FieldName, int32& OutNumber);

	
	UFUNCTION(BlueprintCallable,meta=(ExpandBoolAsExecs = "Success") , Category="Extended Backend | Json")
	static void GetExtendedFloatField(bool& Success , const FExtendedJson& ExtendedJson , const FString& FieldName, float& OutFloat);

	
	UFUNCTION(BlueprintCallable,meta=(ExpandBoolAsExecs = "Success") , Category="Extended Backend | Json")
	static void GetExtendedObjectField(bool& Success , const FExtendedJson& ExtendedJson , const FString& FieldName, FExtendedJson& OutObjectField);

	
	UFUNCTION(BlueprintCallable,meta=(ExpandBoolAsExecs = "Success") , Category="Extended Backend | Json")
	static void GetExtendedArrayField(bool& Success , const FExtendedJson& ExtendedJson , const FString& FieldName, TArray<FExtendedJson>& OutObjectField);

	
	UFUNCTION(BlueprintCallable,meta=(ExpandBoolAsExecs = "Success") , Category="Extended Backend | Json")
	static void GetExtendedJsonValueKeys(bool& Success , const FExtendedJson& ExtendedJson , TArray<FString>& GeneratedKeyArray);
	
};
