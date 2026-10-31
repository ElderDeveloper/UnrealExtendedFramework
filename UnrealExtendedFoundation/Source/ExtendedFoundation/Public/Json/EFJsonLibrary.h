// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json/EFJsonTypes.h"
#include "JsonObjectConverter.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EFJsonLibrary.generated.h"

/**
 * Blueprint and C++ helpers for reading, building and converting JSON.
 *
 * Setters return the object they wrote to so calls can be chained. When the input handle is empty
 * a setter creates a new object, writes to it and returns it; use the return value in that case.
 */
UCLASS(meta = (ScriptName = "EFJsonLibrary"))
class EXTENDEDFOUNDATION_API UEFJsonLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ─── Create, parse, serialize ───────────────────────────────────────────────

	/** A new, empty JSON object. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static FEFJsonObject MakeJsonObject();

	/** An independent deep copy. Changes to the copy do not affect the original. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static FEFJsonObject CopyJsonObject(const FEFJsonObject& JsonObject);

	/** Parses text whose root is a JSON object. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json")
	static bool ParseJsonObject(const FString& JsonString, FEFJsonObject& OutObject);

	/** Parses text whose root is a JSON object or array. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json")
	static bool ParseJsonValue(const FString& JsonString, FEFJsonValue& OutValue);

	/** Serializes an object. An empty handle produces an empty string. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static FString JsonObjectToString(const FEFJsonObject& JsonObject, bool bPrettyPrint = false);

	/** Serializes any value. A string value includes its quotes. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static FString JsonValueToString(const FEFJsonValue& JsonValue, bool bPrettyPrint = false);

	/**
	 * Reads a JSON object from a file. A relative path such as "Saved/Data/Profile.json" is resolved
	 * against the project directory. With the core framework enabled, the path must pass its Extended
	 * File Access settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json")
	static bool LoadJsonObjectFromFile(const FString& FilePath, FEFJsonObject& OutObject);

	/**
	 * Writes a JSON object to a UTF-8 file, creating folders as needed. A relative path such as
	 * "Saved/Data/Profile.json" is resolved against the project directory. With the core framework
	 * enabled, the path must pass its Extended File Access settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json")
	static bool SaveJsonObjectToFile(const FEFJsonObject& JsonObject, const FString& FilePath, bool bPrettyPrint = true);

	// ─── Inspect ────────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json", meta = (DisplayName = "Is Valid (Json Object)"))
	static bool IsValidJsonObject(const FEFJsonObject& JsonObject);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static bool HasField(const FEFJsonObject& JsonObject, const FString& FieldName);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static TArray<FString> GetFieldNames(const FEFJsonObject& JsonObject);

	/** None when the field does not exist. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static EEFJsonType GetFieldType(const FEFJsonObject& JsonObject, const FString& FieldName);

	// ─── Read fields ────────────────────────────────────────────────────────────

	/** Numbers and booleans are converted to text. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static bool TryGetStringField(const FEFJsonObject& JsonObject, const FString& FieldName, FString& OutValue);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static bool TryGetNumberField(const FEFJsonObject& JsonObject, const FString& FieldName, double& OutValue);

	/** Fractional numbers are rounded to the nearest integer, halves away from zero. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static bool TryGetIntegerField(const FEFJsonObject& JsonObject, const FString& FieldName, int64& OutValue);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static bool TryGetBoolField(const FEFJsonObject& JsonObject, const FString& FieldName, bool& OutValue);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static bool TryGetObjectField(const FEFJsonObject& JsonObject, const FString& FieldName, FEFJsonObject& OutValue);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static bool TryGetArrayField(const FEFJsonObject& JsonObject, const FString& FieldName, TArray<FEFJsonValue>& OutValue);

	/** Fails if any element is not a string, number or boolean. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static bool TryGetStringArrayField(const FEFJsonObject& JsonObject, const FString& FieldName, TArray<FString>& OutValue);

	/** Fails if any element is not an object. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static bool TryGetObjectArrayField(const FEFJsonObject& JsonObject, const FString& FieldName, TArray<FEFJsonObject>& OutValue);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static bool TryGetField(const FEFJsonObject& JsonObject, const FString& FieldName, FEFJsonValue& OutValue);

	/**
	 * Reads a nested value with a path such as "data.items[2].name".
	 * Dots separate keys and [n] indexes arrays. An empty path returns the object itself.
	 */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json")
	static bool TryGetFieldByPath(const FEFJsonObject& JsonObject, const FString& Path, FEFJsonValue& OutValue);

	// ─── Write fields ───────────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json")
	static FEFJsonObject SetStringField(const FEFJsonObject& JsonObject, const FString& FieldName, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json")
	static FEFJsonObject SetNumberField(const FEFJsonObject& JsonObject, const FString& FieldName, double Value);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json")
	static FEFJsonObject SetIntegerField(const FEFJsonObject& JsonObject, const FString& FieldName, int64 Value);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json")
	static FEFJsonObject SetBoolField(const FEFJsonObject& JsonObject, const FString& FieldName, bool Value);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json")
	static FEFJsonObject SetNullField(const FEFJsonObject& JsonObject, const FString& FieldName);

	/** An empty Value handle writes null. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json")
	static FEFJsonObject SetObjectField(const FEFJsonObject& JsonObject, const FString& FieldName, const FEFJsonObject& Value);

	/** Empty value handles in the array are written as null. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json")
	static FEFJsonObject SetArrayField(const FEFJsonObject& JsonObject, const FString& FieldName, const TArray<FEFJsonValue>& Value);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json")
	static FEFJsonObject SetStringArrayField(const FEFJsonObject& JsonObject, const FString& FieldName, const TArray<FString>& Value);

	/** An empty Value handle writes null. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json")
	static FEFJsonObject SetField(const FEFJsonObject& JsonObject, const FString& FieldName, const FEFJsonValue& Value);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Json")
	static FEFJsonObject RemoveField(const FEFJsonObject& JsonObject, const FString& FieldName);

	// ─── Values ─────────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json|Value")
	static FEFJsonValue MakeJsonStringValue(const FString& Value);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json|Value")
	static FEFJsonValue MakeJsonNumberValue(double Value);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json|Value")
	static FEFJsonValue MakeJsonIntegerValue(int64 Value);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json|Value")
	static FEFJsonValue MakeJsonBoolValue(bool Value);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json|Value")
	static FEFJsonValue MakeJsonNullValue();

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json|Value")
	static FEFJsonValue MakeJsonObjectValue(const FEFJsonObject& Value);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json|Value")
	static FEFJsonValue MakeJsonArrayValue(const TArray<FEFJsonValue>& Value);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json|Value")
	static EEFJsonType GetJsonValueType(const FEFJsonValue& JsonValue);

	/** Numbers and booleans are converted to text. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json|Value")
	static bool TryGetStringValue(const FEFJsonValue& JsonValue, FString& OutValue);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json|Value")
	static bool TryGetNumberValue(const FEFJsonValue& JsonValue, double& OutValue);

	/** Fractional numbers are rounded to the nearest integer, halves away from zero. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json|Value")
	static bool TryGetIntegerValue(const FEFJsonValue& JsonValue, int64& OutValue);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json|Value")
	static bool TryGetBoolValue(const FEFJsonValue& JsonValue, bool& OutValue);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json|Value")
	static bool TryGetObjectValue(const FEFJsonValue& JsonValue, FEFJsonObject& OutValue);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Json|Value")
	static bool TryGetArrayValue(const FEFJsonValue& JsonValue, TArray<FEFJsonValue>& OutValue);

	// ─── Structs ────────────────────────────────────────────────────────────────

	/**
	 * Converts any struct to a JSON object. Field names are the struct's property names as authored.
	 * Accepts native and Blueprint structs.
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Extended Framework|Backend|Json", meta = (CustomStructureParam = "Struct"))
	static bool StructToJsonObject(const int32& Struct, FEFJsonObject& OutObject);
	DECLARE_FUNCTION(execStructToJsonObject);

	/**
	 * Fills any struct from a JSON object. Fields are matched to properties by name, ignoring case.
	 * Properties with no matching field keep their current value.
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Extended Framework|Backend|Json", meta = (CustomStructureParam = "OutStruct"))
	static bool JsonObjectToStruct(const FEFJsonObject& JsonObject, int32& OutStruct);
	DECLARE_FUNCTION(execJsonObjectToStruct);

	/** Implementation of StructToJsonObject for callers that already hold the struct type. */
	static bool StructToJsonObjectGeneric(const UScriptStruct* StructType, const void* StructData, FEFJsonObject& OutObject);

	/** Implementation of JsonObjectToStruct for callers that already hold the struct type. */
	static bool JsonObjectToStructGeneric(const FEFJsonObject& JsonObject, const UScriptStruct* StructType, void* StructData);

	/** Typed C++ convenience: UEFJsonLibrary::ToJsonObject(MyStruct). */
	template <typename TStruct>
	static FEFJsonObject ToJsonObject(const TStruct& Struct)
	{
		FEFJsonObject Out;
		StructToJsonObjectGeneric(TStruct::StaticStruct(), &Struct, Out);
		return Out;
	}

	/** Typed C++ convenience: UEFJsonLibrary::FromJsonObject(Object, MyStruct). */
	template <typename TStruct>
	static bool FromJsonObject(const FEFJsonObject& JsonObject, TStruct& OutStruct)
	{
		return JsonObjectToStructGeneric(JsonObject, TStruct::StaticStruct(), &OutStruct);
	}
};
