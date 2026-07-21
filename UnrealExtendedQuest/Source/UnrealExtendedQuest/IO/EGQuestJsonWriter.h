// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Logging/LogMacros.h"
#include "UObject/UnrealType.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"

#include "IEGQuestWriter.h"

DECLARE_LOG_CATEGORY_EXTERN(LogQuestJsonWriter, All, All);


struct UNREALEXTENDEDQUEST_API EGQuestJsonWriterOptions
{
	int32 InitialIndent = 0;
	bool bPrettyPrint = true;
};

/**
 * @brief The QuestJsonWriter class mostly adapted for Quests, copied from FJsonObjectConverter
 * See IEGQuestWriter for properties and METADATA specifiers.
 */
class UNREALEXTENDEDQUEST_API FEGQuestJsonWriter : public IEGQuestWriter
{
	/**
	 * Call Order and possible calls:
	 *  - QuestJsonWriter
	 *		- UStructToJsonString
	 *			- UStructToJsonObject
	 *				- UStructToJsonAttributes
	 *					- PropertyToJsonValue
	 *						- ConvertScalarPropertyToJsonValue
	 *							- PropertyToJsonValue
	 *							- UStructToJsonObject
	 */
public:

	FEGQuestJsonWriter() {};

	// IEGQuestWriter Interface
	void Write(const UStruct* StructDefinition, const void* ContainerPtr) override;

	/**
	 * Save the config string to a text file
	 * @param FullName: Full path + file name + extension
	 * @return	False on failure to write
	 */
	bool ExportToFile(const FString& FileName) override
	{
		return FFileHelper::SaveStringToFile(JsonString, *FileName, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	const FString& GetAsString() const override
	{
		return JsonString;
	}

private: // UStruct -> JSON
	/**
	 * Convert property to JSON, assuming either the property is not an array or the value is an individual array element
	 * Used by PropertyToJsonValue
	 */
	TSharedPtr<FJsonValue> ConvertScalarPropertyToJsonValue(const FProperty* Property, const void* const ContainerPtr, const void* const ValuePtr);

	/**
	 * Converts from a Property to a Json Value using exportText
	 *
	 * @param Property			The property to export
	 * @param ValuePtr			Pointer to the value of the property
	 *
	 * @return					The constructed JsonValue from the property
	 */
	TSharedPtr<FJsonValue> PropertyToJsonValue(const FProperty* Property, const void* const ContainerPtr, const void* const ValuePtr);

	/**
	 * Converts from a UStruct to a set of json attributes (possibly from within a JsonObject)
	 *
	 * @param StructDefinition UStruct definition that is looked over for properties
	 * @param ContainerPtr	   The object the UStruct represents.
	 * @param JsonAttributes   Map of attributes to copy in to
	 *
	 * @return False if any properties failed to write
	 */
	bool UStructToJsonAttributes(const UStruct* StructDefinition, const void* const ContainerPtr,
								 TMap<FString, TSharedPtr<FJsonValue>>& OutJsonAttributes);

	/**
	 * Converts from a UStruct to a JSON Object
	 *
	 * @param StructDefinition UStruct definition that is looked over for properties
	 * @param ContainerPtr	   The object the UStruct represents.
	 * @param OutJsonObject	Json Object to be filled in with data from the ustruct
	 *
	 * @return False if faile to fill properties
	 */
	bool UStructToJsonObject(const UStruct* StructDefinition, const void* const ContainerPtr, TSharedRef<FJsonObject>& OutJsonObject)
	{
		// UE 5.8: FJsonObject::Values keys are UE::FSharedString, so fill a local FString map and copy per field.
		TMap<FString, TSharedPtr<FJsonValue>> JsonAttributes;
		if (!UStructToJsonAttributes(StructDefinition, ContainerPtr, JsonAttributes))
		{
			return false;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : JsonAttributes)
		{
			OutJsonObject->SetField(Pair.Key, Pair.Value);
		}
		return true;
	}

	/**
	 * Converts from a UStruct to a JSON string containing an object, using exportText
	 *
	 * @param StructDefinition UStruct definition that is looked over for properties
	 * @param ContainerPtr	   The object the UStruct represents.
	 * @param OutJsonString	Json Object to be filled in with data from the ustruct
	 *
	 * @return False if failed to serialize to string
	 */
	bool UStructToJsonString(const UStruct* StructDefinition, const void* const ContainerPtr, const EGQuestJsonWriterOptions& Options,
							 FString& OutJsonString);

	void ResetState()
	{
		IndexInArray = INDEX_NONE;
		bIsPropertyMapKey = false;
	}

private:
	// Final output string
	FString JsonString;

	/** Only properties that have these flags will be written. */
	static constexpr int64 CheckFlags = ~CPF_ParmFlags; // all properties except those who have these flags? TODO is this ok?

	// Used when parsing

	// If it is in an array this is != INDEX_NONE
	int32 IndexInArray = INDEX_NONE;

	// Is the current property used as a map key?
	bool bIsPropertyMapKey = false;
};
