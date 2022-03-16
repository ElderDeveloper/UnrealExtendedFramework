// Fill out your copyright notice in the Description page of Project Settings.


#include "EFJsonObject.h"
#include "Json.h"



UEFJsonObject::UEFJsonObject(const FObjectInitializer& ObjectInitializer)
{
}



void UEFJsonObject::SetupJSonObject(FString jsonString)
{
	/*
	ObjectJsonString = jsonString;
	ObjectJsonObject = MakeShareable(new FJsonObject());
	ObjectJsonReader = TJsonReaderFactory<>::Create(ObjectJsonString);
	
	if (FJsonSerializer::Deserialize(ObjectJsonReader, ObjectJsonObject) && ObjectJsonObject.IsValid())
	{
		bCanDeserialize = true;
	}
	*/
}
/*

FString UEFJsonObject::GetJsonStringField(FString ObjectField, FString StringField)
{
	if (ObjectJsonObject && bCanDeserialize)
	{
		if(const auto ObjectFieldRef = ObjectJsonObject->GetObjectField(ObjectField))
		{
			return ObjectFieldRef->GetStringField(StringField);
		}
	}
	UE_LOG(LogJson , Error , TEXT("JSon String Error"));
	return "";
}

float UEFJsonObject::GetJSonFloatField(FString ObjectField, FString FloatField)
{
	if (ObjectJsonObject && bCanDeserialize)
	{
		if(const auto ObjectFieldRef = ObjectJsonObject->GetObjectField(ObjectField))
		{
			return ObjectFieldRef->GetNumberField(FloatField);
		}
	}
	UE_LOG(LogJson , Error , TEXT("JSon Float Error"));
	return 0;
}

bool UEFJsonObject::GetJSonBoolField(FString ObjectField, FString BoolField)
{
	if (ObjectJsonObject && bCanDeserialize)
	{
		if(const auto ObjectFieldRef = ObjectJsonObject->GetObjectField(ObjectField))
		{
			return ObjectFieldRef->GetBoolField(BoolField);
		}
	}
	UE_LOG(LogJson , Error , TEXT("JSon Bool Error"));
	return false;
}

int32 UEFJsonObject::GetJsonIntegerField(FString ObjectField, FString IntegerField)
{
	if (ObjectJsonObject && bCanDeserialize)
	{
		if(const auto ObjectFieldRef = ObjectJsonObject->GetObjectField(ObjectField))
		{
			return ObjectFieldRef->GetIntegerField(IntegerField);
		}
	}
	UE_LOG(LogJson , Error , TEXT("JSon Integer Error"));
	return 0;
}

TArray<FString> UEFJsonObject::GetJSonStringArrayField(FString ObjectField, FString StringField)
{
	TArray<FString> SArray;
	if (ObjectJsonObject && bCanDeserialize)
	{
		if(const auto ObjectFieldRef = ObjectJsonObject->GetObjectField(ObjectField))
		{
			const auto array = ObjectFieldRef->GetArrayField(StringField);
			for (const auto i : array)
				SArray.Add(i->AsString());
			
			return SArray;
		}
	}
	UE_LOG(LogJson , Error , TEXT("JSon String Array Error"));
	return SArray;
}

TArray<float> UEFJsonObject::GetJSonFloatArrayField(FString ObjectField, FString FloatField)
{
	TArray<float> FArray;
	if (ObjectJsonObject && bCanDeserialize)
	{
		if(const auto ObjectFieldRef = ObjectJsonObject->GetObjectField(ObjectField))
		{
			const auto array = ObjectFieldRef->GetArrayField(FloatField);
			for (const auto i : array)
				FArray.Add(i->AsNumber());
			
			return FArray;
		}
	}
	UE_LOG(LogJson , Error , TEXT("JSon Float Array Error"));
	return FArray;
}



TArray<bool> UEFJsonObject::GetJSonBoolArrayField(FString ObjectField, FString BoolField)
{
	TArray<bool> BArray;
	if (ObjectJsonObject && bCanDeserialize)
	{
		if(const auto ObjectFieldRef = ObjectJsonObject->GetObjectField(ObjectField))
		{
			const auto array = ObjectFieldRef->GetArrayField(BoolField);
			for (const auto i : array)
				BArray.Add(i->AsBool());
			
			return BArray;
		}
	}
	UE_LOG(LogJson , Error , TEXT("JSon Bool Array Error"));
	return BArray;
}



TArray<int32> UEFJsonObject::GetJSonIntegerArrayField(FString ObjectField, FString IntegerField)
{
	TArray<int32> IArray;
	if (ObjectJsonObject && bCanDeserialize)
	{
		if(const auto ObjectFieldRef = ObjectJsonObject->GetObjectField(ObjectField))
		{
			const auto array = ObjectFieldRef->GetArrayField(IntegerField);
			for (const auto i : array)
				IArray.Add(i->AsNumber());
			
			return IArray;
		}
	}
	UE_LOG(LogJson , Error , TEXT("JSon Integer Array Error"));
	return IArray;
}
*/