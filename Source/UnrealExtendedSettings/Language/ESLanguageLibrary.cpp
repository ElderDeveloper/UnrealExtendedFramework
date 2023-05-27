// Fill out your copyright notice in the Description page of Project Settings.


#include "ESLanguageLibrary.h"
#include "XmlFile.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializerMacros.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Subsystem/ESLanguageSubsystem.h"
#include "UnrealExtendedSettings/Subtitles/Data/ESSubtitleData.h"

#include "XmlParser/Public/FastXml.h"



FText UESLanguageLibrary::GetLanguageString(const UObject* WorldContextObject, FString Key)
{
	if (const auto LanguageSubsystem = Cast<UESLanguageSubsystem>(WorldContextObject->GetWorld()->GetGameInstance()->GetSubsystem<UESLanguageSubsystem>()))
	{
		const FString FilePath = LanguageSubsystem->GetCurrentLanguageFileDirection();

		FXmlFile XmlFile;
		
		if (!XmlFile.LoadFile(FilePath))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to load XML file: %s"), *FilePath);
		}
		
		const FXmlNode* RootNode = XmlFile.GetRootNode();
		
		for (const FXmlNode* ChildNode : RootNode->GetChildrenNodes())
		{
			if(Key == ChildNode->GetAttribute(TEXT("key")))
			{
				return  FText::FromString(ChildNode->GetAttribute(TEXT("string")));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load UMoonPunchLanguageSubsystem"));
	}
	return FText();
}

void UESLanguageLibrary::ReadXmlFile(const FString& FilePath, TArray<FString>& Keys, TArray<FString>& Values)
{
	// Create a new parser instance
	FXmlFile XmlFile;
    
	// Load the XML file from disk
	if (!XmlFile.LoadFile(FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load XML file: %s"), *FilePath);
	}
    
	// Get the root element of the XML document
	const FXmlNode* RootNode = XmlFile.GetRootNode();
    
	// Iterate through all child elements of the root node
	for (const FXmlNode* ChildNode : RootNode->GetChildrenNodes())
	{
		// Check if the child node is a "text" element
		if (ChildNode->GetTag() == TEXT("text"))
		{
			// Get the value of the "key" attribute
			FString Key = ChildNode->GetAttribute(TEXT("key"));
			Keys.Add(Key);
			// Get the value of the "string" attribute
			FString StringValue = ChildNode->GetAttribute(TEXT("string"));
			Values.Add(StringValue);
		}
	}
}

FString UESLanguageLibrary::ConvertXmlToJsonString(const FString& XmlFilePath)
{
	TArray<FString> Keys;
	TArray<FString> Values;
	ReadXmlFile(XmlFilePath ,Keys,Values);
	
	// Convert the root node to a JSON object
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

	for (int32 i = 0; i<Keys.Num() ; i++)
	{
		if (Keys.IsValidIndex(i) && Values.IsValidIndex(i))
		{
			TSharedPtr<FJsonObject> RowJsonObject = MakeShareable(new FJsonObject());
			RowJsonObject->SetStringField("Subtitle" ,Values[i] );
			JsonObject->SetObjectField(Keys[i] ,RowJsonObject);
		}
	}
	
	// Convert the JSON object to a string
	FString JsonString;
	const TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
	if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), JsonWriter))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to serialize JSON object"));
		return "";
	}

	return JsonString;
}

void UESLanguageLibrary::FillSubtitleDataTable(UDataTable* DataTable, const FString& XmlFilePath)
{
	if (!DataTable)
	{
		return;
	}

	TArray<FString> Keys;
	TArray<FString> Values;
	ReadXmlFile(XmlFilePath ,Keys,Values);

	FExtendedSubtitle Subtitle;

	for (const auto Row : DataTable->GetRowNames())
	{
		//DataTable->RemoveRow(Row);
	}

	for (int32 i = 0; i<Keys.Num() ; i++)
	{
		if (Keys.IsValidIndex(i) && Values.IsValidIndex(i))
		{
			Subtitle.Subtitle = Values[i];
			DataTable->AddRow(*Keys[i] , Subtitle);
		}
	}
}



