// Fill out your copyright notice in the Description page of Project Settings.


#include "EBJsonLibrary.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealExtendedFramework/Libraries/File/EFFileLibrary.h"

FExtendedJson UEBJsonLibrary::ReadJsonFile(TEnumAsByte<EFProjectDirectory> DirectoryType , FString FileDirectory , bool& Success ,bool DebugDirectory)
{
	const FString Source = UEFFileLibrary::ExtendedProjectDirectory(DirectoryType) + FileDirectory;
	Success = true;
	
	if(DebugDirectory)	UE_LOG(LogBlueprint , Warning , TEXT("ReadJsonFile: Directory = %s") , *Source);
	
	FString JsonStr;
	if(!FFileHelper::LoadFileToString(JsonStr , *FString(Source)))
		UE_LOG(LogBlueprint , Error , TEXT("ReadJsonFile : LoadFileToString Received error"));
	
	TSharedRef<TJsonReader<TCHAR>> JSonReader = TJsonReaderFactory<TCHAR>::Create(JsonStr);
	TSharedPtr<FJsonObject> JSonObject = MakeShareable(new FJsonObject);
	
	if (FJsonSerializer::Deserialize(JSonReader , JSonObject) && JSonObject.IsValid())
		return FExtendedJson(JSonObject);
	
	UE_LOG(LogBlueprint , Error , TEXT("ReadJsonFile :  Json Deserialize Received Error , Must Check If JSon Valid"));
	Success = false;
	return FExtendedJson();
	
}


FString UEBJsonLibrary::ReadExtendedJsonAsString(TEnumAsByte<EFProjectDirectory> DirectoryType, FString FileDirectory,bool& Success)
{
	const FString Source = UEFFileLibrary::ExtendedProjectDirectory(DirectoryType) + FileDirectory;
	Success = false;
	FString JsonStr;
	if(FFileHelper::LoadFileToString(JsonStr , *FString(Source)))
	{
		Success = true;
		return JsonStr;
	}
	return "";
}



void UEBJsonLibrary::GetExtendedStringField(bool& Success, const FExtendedJson& ExtendedJson, const FString& FieldName,FString& OutString)
{
	Success = ExtendedJson.GetExtendedStringField(FieldName , OutString);
}



void UEBJsonLibrary::GetExtendedBoolField(bool& Success, const FExtendedJson& ExtendedJson, const FString& FieldName,bool& OutBool)
{
	Success = ExtendedJson.GetExtendedBoolField(FieldName , OutBool);
}



void UEBJsonLibrary::GetExtendedNumberField(bool& Success, const FExtendedJson& ExtendedJson, const FString& FieldName,int32& OutNumber)
{
	Success = ExtendedJson.GetExtendedNumberField(FieldName , OutNumber);
}



void UEBJsonLibrary::GetExtendedFloatField(bool& Success, const FExtendedJson& ExtendedJson, const FString& FieldName,float& OutFloat)
{
	Success = ExtendedJson.GetExtendedFloatField(FieldName , OutFloat);
}



void UEBJsonLibrary::GetExtendedObjectField(bool& Success, const FExtendedJson& ExtendedJson, const FString& FieldName,FExtendedJson& OutObjectField)
{
	Success = false;
	if(const auto Object = ExtendedJson.GetJsonObject()->GetField<EJson::Object>(FieldName))
	{
		OutObjectField = FExtendedJson(Object.Get()->AsObject());
		Success = true;
	}
}



void UEBJsonLibrary::GetExtendedArrayField(bool& Success, const FExtendedJson& ExtendedJson, const FString& FieldName,TArray<FExtendedJson>& OutObjectField)
{
	Success = false;
	const auto arr = ExtendedJson.GetJsonObject()->GetArrayField(FieldName);
	for (const auto i : arr)
	{
		const auto item = FExtendedJson(i.Get()->AsObject());
		OutObjectField.Add(item);
	}
	if (OutObjectField.Num()>0)
		Success = true;

}



void UEBJsonLibrary::GetExtendedJsonValueKeys(bool& Success, const FExtendedJson& ExtendedJson,TArray<FString>& GeneratedKeyArray)
{
	Success = false;
	ExtendedJson.GetExtendedJsonKeys(GeneratedKeyArray);
	if (GeneratedKeyArray.Num() > 0)
		Success = true;
}

