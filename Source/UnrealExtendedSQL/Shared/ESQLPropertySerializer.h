// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESQLTypes.h"

class FProperty;

class UNREALEXTENDEDSQL_API FESQLPropertySerializer
{
public:
	/** Return the SQLite storage type for a property, or empty if unsupported. */
	static FString GetSQLiteType(const FProperty* Property);

	/** Serialize a property value from container memory into a nullable SQL binding. */
	static bool SerializePropertyToBindingValue(const FProperty* Property, const void* ContainerData, FESQLBindingValue& OutValue);

	/** Serialize a raw property value pointer into a nullable SQL binding. */
	static bool SerializePropertyValueToBindingValue(FProperty* Property, const void* ValuePtr, FESQLBindingValue& OutValue);

	/** Serialize a property value from container memory to a SQL string. */
	static bool SerializePropertyToString(const FProperty* Property, const void* ContainerData, FString& OutValue);

	/** Deserialize a SQL string into a property value in container memory. */
	static bool DeserializePropertyFromString(const FProperty* Property, void* ContainerData, const FString& InValue, bool bIsNull = false);

private:
	static bool DeserializePropertyValueFromString(FProperty* Property, void* ValuePtr, const FString& InValue, bool bIsNull);
};