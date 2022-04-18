// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "JsonUtilities/Public/JsonUtilities.h"
#include "EBJsonData.generated.h"


USTRUCT(BlueprintType)
struct FExtendedJson
{
	GENERATED_BODY()

public:
	
	 TSharedPtr<FJsonObject> JsonObject = nullptr;


	FExtendedJson()
	{}
	FExtendedJson(TSharedPtr<FJsonObject> jsonObject)
	{
		JsonObject = jsonObject;
	}

	
	TSharedPtr<FJsonObject> GetJsonObject() const { return JsonObject; }

	bool GetExtendedStringField(const FString& FieldName, FString& OutString) const
	{
		OutString = "";
		if(JsonObject)
			return  JsonObject->TryGetStringField(FieldName , OutString);
		return false;
	}

	bool GetExtendedBoolField(const FString& FieldName, bool& OutBool) const
	{
		OutBool = false;
		if(JsonObject)
			return  JsonObject->TryGetBoolField(FieldName , OutBool);
		return false;
	}

	bool GetExtendedNumberField(const FString& FieldName, int32& OutNumber) const
	{
		OutNumber = -1;
		if(JsonObject)
			return  JsonObject->TryGetNumberField(FieldName , OutNumber);
		return false;
	}

	bool GetExtendedFloatField(const FString& FieldName, float& OutFloat) const
	{
		OutFloat = -1;
		if(JsonObject)
			return  JsonObject->TryGetNumberField(FieldName , OutFloat);
		return false;
	}

	bool GetExtendedObjectField(const FString& FieldName,  const TSharedPtr<FJsonObject>*& OutObjectField) const
	{
		if(JsonObject)
			return JsonObject->TryGetObjectField(FieldName , OutObjectField);
		return false;
	}

	bool GetExtendedArrayField(const FString& FieldName, const TArray<TSharedPtr<FJsonValue>>*& OutArrayField) const
	{
		if(JsonObject)
			return JsonObject->TryGetArrayField(FieldName , OutArrayField);
		return false;
	}

	void GetExtendedJsonKeys(TArray<FString>& GeneratedKeys) const
	{
		if(JsonObject)
		JsonObject->Values.GenerateKeyArray(GeneratedKeys);
	}
	
};





UCLASS()
class UNREALEXTENDEDBACKEND_API UEBJsonData : public UObject
{
	GENERATED_BODY()
};
