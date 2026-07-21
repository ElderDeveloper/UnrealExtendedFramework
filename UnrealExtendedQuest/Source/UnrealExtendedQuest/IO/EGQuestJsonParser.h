// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Logging/LogMacros.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"

#include "IEGQuestParser.h"
#include "UnrealExtendedQuest/NYEngineVersionHelpers.h"

DECLARE_LOG_CATEGORY_EXTERN(LogQuestJsonParser, All, All);


/**
 * @brief The QuestJsonParser class mostly adapted for Quests, copied from FJsonObjectConverter
 * See IEGQuestParser for properties and METADATA specifiers.
 */
class UNREALEXTENDEDQUEST_API FEGQuestJsonParser : public IEGQuestParser
{
	/**
	 * Call Order and possible calls:
	 *  - QuestJsonParser
	 *		- InitializeParser
	 *			- JsonObjectStringToUStruct
	 *				- JsonObjectToUStruct
	 *					- JsonAttributesToUStruct
	 *						- JsonValueToProperty
	 *							- ConvertScalarJsonValueToProperty
	 *								- JsonValueToProperty
	 *								- JsonObjectToUStruct
	 */

public:
	FEGQuestJsonParser() {}

	FEGQuestJsonParser(const FString& FilePath)
	{
		InitializeParser(FilePath);
	}

	// IEGQuestParser Interface
	void InitializeParser(const FString& FilePath) override;
	void InitializeParserFromString(const FString& Text) override;
	bool IsValidFile() const override { return bIsValidFile; }
	void ReadAllProperty(const UStruct* ReferenceClass, void* TargetObject, UObject* DefaultObjectOuter = nullptr) override;


private: // JSON -> UStruct

	/**
	 * Convert JSON to property, assuming either the property is not an array or the value is an individual array element
	 * Used by JsonValueToProperty
	 */
	bool ConvertScalarJsonValueToProperty(const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property, void* ContainerPtr, void* ValuePtr);

	/**
	 * Converts a single JsonValue to the corresponding Property (this may recurse if the property is a UStruct for instance).
	 *
	 * @param JsonValue The value to assign to this property
	 * @param Property The Property definition of the property we're setting.
	 * @param ValuePtr Pointer to the property instance to be modified.
	 *
	 * @return False if the property failed to serialize
	 */
	bool JsonValueToProperty(const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property, void* ContainerPtr, void* ValuePtr);

	/**
	 * Converts a set of json attributes (possibly from within a JsonObject) to a UStruct, using importText
	 *
	 * @param JsonAttributes 	Json Object to copy data out of
	 * @param StructDefinition  UStruct definition that is looked over for properties
	 * @param ContainerPtr 		The Pointer instance to copy in to
	 *
	 * @return False if any properties matched but failed to deserialize
	 */
	bool JsonAttributesToUStruct(const TMap<FString, TSharedPtr<FJsonValue>>& JsonAttributes,
								const UStruct* StructDefinition, void* ContainerPtr);

	/**
	 * Converts from a Json Object to a UStruct, using importText
	 *
	 * @param JsonObject 		Json Object to copy data out of
	 * @param StructDefinition  UStruct definition that is looked over for properties
	 * @param ContainerPtr 		The Pointer instance to copy in to
	 *
	 * @return False if any properties matched but failed to deserialize
	 */
	bool JsonObjectToUStruct(const TSharedRef<const FJsonObject>& JsonObject, const UStruct* StructDefinition, void* ContainerPtr)
	{
		// UE 5.8: FJsonObject::Values keys are UE::FSharedString; convert per entry.
		TMap<FString, TSharedPtr<FJsonValue>> JsonAttributes;
		JsonAttributes.Reserve(JsonObject->Values.Num());
		for (const auto& Pair : JsonObject->Values)
		{
			JsonAttributes.Add(FString(Pair.Key.ToView()), Pair.Value);
		}
		return JsonAttributesToUStruct(JsonAttributes, StructDefinition, ContainerPtr);
	}

	/**
	 * Converts from a json string containing an object to a UStruct
	 *
	 * @param OutStruct The UStruct instance to copy in to
	 * @param ContainerPtr 		The Pointer instance to copy in to
	 *
	 * @return False if any properties matched but failed to deserialize
	 */
	bool JsonObjectStringToUStruct(const UStruct* StructDefinition, void* ContainerPtr);

private:
	FString JsonString;
	FString FileName;
	bool bIsValidFile = false;

	/** The default object outer used when creating new objects when using NewObject.  */
	UObject* DefaultObjectOuter = nullptr;

	/** Only properties that have these flags will be read. */
	static constexpr int64 CheckFlags = ~CPF_ParmFlags;
};
