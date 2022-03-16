// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "EFJsonObject.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDBACKEND_API UEFJsonObject : public UObject
{
	GENERATED_BODY()
	
public:
	
	UEFJsonObject(const FObjectInitializer& ObjectInitializer);
	


	void SetupJSonObject(FString jsonString);

	/*
	UFUNCTION(BlueprintCallable , Category="ExtendedFramework|JsonObject")
	FString GetJsonStringField(FString ObjectField , FString StringField);

	UFUNCTION(BlueprintCallable , Category="ExtendedFramework|JsonObject")
	float GetJSonFloatField(FString ObjectField , FString FloatField);

	UFUNCTION(BlueprintCallable , Category="ExtendedFramework|JsonObject")
	bool GetJSonBoolField(FString ObjectField , FString BoolField);

	UFUNCTION(BlueprintCallable , Category="ExtendedFramework|JsonObject")
	int32 GetJsonIntegerField(FString ObjectField , FString IntegerField);

	
	UFUNCTION(BlueprintCallable , Category="ExtendedFramework|JsonObject")
	TArray<FString> GetJSonStringArrayField(FString ObjectField , FString StringField);

	UFUNCTION(BlueprintCallable , Category="ExtendedFramework|JsonObject")
	TArray<float> GetJSonFloatArrayField(FString ObjectField , FString FloatField);

	UFUNCTION(BlueprintCallable , Category="ExtendedFramework|JsonObject")
	TArray<bool> GetJSonBoolArrayField(FString ObjectField , FString BoolField);

	UFUNCTION(BlueprintCallable , Category="ExtendedFramework|JsonObject")
	TArray<int32> GetJSonIntegerArrayField(FString ObjectField , FString IntegerField);
	*/

	bool bCanDeserialize = false;

	/*
	TSharedPtr<FJsonObject> ObjectJsonObject;
	FString ObjectJsonString;
	TSharedRef<TJsonReader<>> ObjectJsonReader;
	*/
	
};
