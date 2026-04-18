// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


/**
 * Validates a UScriptStruct for SQLite compatibility.
 * Used by the editor module before creating a UESQLTableAsset,
 * and available at runtime for programmatic checks.
 *
 * Type mapping:
 *   bool         → INTEGER (0/1)
 *   int32, int64 → INTEGER
 *   uint8, enum  → INTEGER
 *   float, double→ REAL
 *   FString      → TEXT
 *   FName        → TEXT
 *   FText        → TEXT
 *   TSoftObjectPtr/TSoftClassPtr → TEXT (asset path)
 *   FGameplayTag → TEXT
 *   FDateTime    → TEXT (ISO 8601)
 *   FGuid        → TEXT
 *   Arrays/sets/maps → TEXT (JSON)
 *   Other structs    → TEXT (JSON)
 *
 * NOT supported: UObject* hard references, delegates.
 */
class UNREALEXTENDEDSQL_API FESQLStructValidator
{
public:

	/** Result for a single field. */
	struct FFieldResult
	{
		FString FieldName;
		FString UETypeName;          // "int32", "TArray<FName>", etc.
		FString SQLiteType;          // "INTEGER", "TEXT", "REAL", or "" if invalid
		bool bIsValid = false;
		FString ErrorReason;         // Empty if valid
	};

	/** Validate all fields of a struct. Returns true if ALL fields are valid. */
	static bool Validate(
		const UScriptStruct* Struct,
		TArray<FFieldResult>& OutResults
	);

	/** Map a single FProperty to a SQLite type string.
	    Returns empty string if the property type is unsupported. */
	static FString MapPropertyToSQLiteType(const FProperty* Property);

	/** Get a human-readable UE type name for a property (e.g. "int32", "TArray<FName>"). */
	static FString GetPropertyTypeName(const FProperty* Property);

	/** Returns the SQL column definition string for a validated struct.
	    e.g. "\"ItemId\" TEXT, \"DisplayName\" TEXT, \"Price\" INTEGER, \"Weight\" REAL"
	    Returns empty string if struct has invalid fields. */
	static FString BuildColumnDefinition(const UScriptStruct* Struct);

	/** Returns the column definitions as a TMap (ColumnName → SQLiteType).
	    Includes only valid fields. */
	static TMap<FString, FString> BuildColumnMap(const UScriptStruct* Struct);
};
