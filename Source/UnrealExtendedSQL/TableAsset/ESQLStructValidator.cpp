// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESQLStructValidator.h"
#include "Shared/ESQLPropertySerializer.h"
#include "UnrealExtendedSQL.h"
#include "GameplayTagContainer.h"
#include "UObject/PropertyOptional.h"
#include "UObject/UnrealType.h"

namespace
{
const FName DateTimeStructName(TEXT("DateTime"));
const FName TimespanStructName(TEXT("Timespan"));
const FName GuidStructName(TEXT("Guid"));
const FName SoftObjectPathStructName(TEXT("SoftObjectPath"));
const FName SoftClassPathStructName(TEXT("SoftClassPath"));
const FName PrimaryAssetTypeStructName(TEXT("PrimaryAssetType"));
const FName PrimaryAssetIdStructName(TEXT("PrimaryAssetId"));

bool IsPlainTextStruct(const UStruct* Struct)
{
	if (!Struct)
	{
		return false;
	}

	const FName StructName = Struct->GetFName();
	return Struct == FGameplayTag::StaticStruct()
		|| StructName == DateTimeStructName
		|| StructName == TimespanStructName
		|| StructName == GuidStructName
		|| StructName == SoftObjectPathStructName
		|| StructName == SoftClassPathStructName
		|| StructName == PrimaryAssetTypeStructName
		|| StructName == PrimaryAssetIdStructName;
}

bool IsSupportedMapKeyProperty(const FProperty* Property)
{
	if (!Property)
	{
		return false;
	}

	if (CastField<FBoolProperty>(Property)
		|| CastField<FIntProperty>(Property)
		|| CastField<FInt64Property>(Property)
		|| CastField<FUInt32Property>(Property)
		|| CastField<FInt16Property>(Property)
		|| CastField<FUInt16Property>(Property)
		|| CastField<FByteProperty>(Property)
		|| CastField<FEnumProperty>(Property)
		|| CastField<FStrProperty>(Property)
		|| CastField<FNameProperty>(Property))
	{
		return true;
	}

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		return IsPlainTextStruct(StructProperty->Struct);
	}

	return false;
}

bool IsRecursivelySupportedProperty(const FProperty* Property)
{
	if (!Property)
	{
		return false;
	}

	if (const FOptionalProperty* OptionalProperty = CastField<FOptionalProperty>(Property))
	{
		return IsRecursivelySupportedProperty(OptionalProperty->GetValueProperty());
	}

	if (CastField<FDelegateProperty>(Property) || CastField<FMulticastDelegateProperty>(Property))
	{
		return false;
	}

	if (CastField<FObjectPropertyBase>(Property) && !CastField<FSoftObjectProperty>(Property) && !CastField<FSoftClassProperty>(Property))
	{
		return false;
	}

	if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		return IsRecursivelySupportedProperty(ArrayProperty->Inner);
	}

	if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	{
		return IsRecursivelySupportedProperty(SetProperty->ElementProp);
	}

	if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		return IsSupportedMapKeyProperty(MapProperty->KeyProp)
			&& IsRecursivelySupportedProperty(MapProperty->ValueProp);
	}

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (!StructProperty->Struct)
		{
			return false;
		}

		if (IsPlainTextStruct(StructProperty->Struct))
		{
			return true;
		}

		for (TFieldIterator<FProperty> It(StructProperty->Struct); It; ++It)
		{
			if (!IsRecursivelySupportedProperty(*It))
			{
				return false;
			}
		}

		return true;
	}

	return !FESQLPropertySerializer::GetSQLiteType(Property).IsEmpty();
}
}


// ── Validation ───────────────────────────────────────────────────────────────

bool FESQLStructValidator::Validate(
	const UScriptStruct* Struct,
	TArray<FFieldResult>& OutResults)
{
	OutResults.Empty();

	if (!Struct)
	{
		return false;
	}

	bool bAllValid = true;

	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		FProperty* Property = *It;
		FFieldResult FieldResult;
		FieldResult.FieldName = Property->GetName();
		FieldResult.UETypeName = GetPropertyTypeName(Property);
		FieldResult.SQLiteType = MapPropertyToSQLiteType(Property);
		FieldResult.bIsValid = !FieldResult.SQLiteType.IsEmpty() && IsRecursivelySupportedProperty(Property);

		if (!FieldResult.bIsValid)
		{
			bAllValid = false;

			// Build error reason
			if (CastField<FOptionalProperty>(Property))
			{
				FieldResult.ErrorReason = TEXT("Optional properties use nullable underlying columns, but this optional wraps an unsupported nested field type.");
			}
			if (CastField<FArrayProperty>(Property))
			{
				FieldResult.ErrorReason = TEXT("Array types are stored as JSON TEXT, but this array contains unsupported nested field types.");
			}
			else if (CastField<FMapProperty>(Property))
			{
				FieldResult.ErrorReason = TEXT("Map types are stored as JSON TEXT, but this map uses an unsupported key type or contains unsupported nested value types.");
			}
			else if (CastField<FSetProperty>(Property))
			{
				FieldResult.ErrorReason = TEXT("Set types are stored as JSON TEXT, but this set contains unsupported nested field types.");
			}
			else if (CastField<FStructProperty>(Property))
			{
				FieldResult.ErrorReason = TEXT("Struct types are stored as JSON TEXT, but this struct contains unsupported nested field types.");
			}
			else if (CastField<FObjectPropertyBase>(Property) && !CastField<FSoftObjectProperty>(Property) && !CastField<FSoftClassProperty>(Property))
			{
				FieldResult.ErrorReason = TEXT("Hard object references cannot be stored in SQLite. Use a soft reference or store an asset path as text.");
			}
			else if (CastField<FDelegateProperty>(Property) || CastField<FMulticastDelegateProperty>(Property))
			{
				FieldResult.ErrorReason = TEXT("Delegate properties cannot be stored in SQLite.");
			}
			else
			{
				FieldResult.ErrorReason = FString::Printf(TEXT("Property type '%s' is not supported for SQLite storage."), *FieldResult.UETypeName);
			}
		}

		OutResults.Add(FieldResult);
	}

	return bAllValid;
}


// ── Type Mapping ─────────────────────────────────────────────────────────────

FString FESQLStructValidator::MapPropertyToSQLiteType(const FProperty* Property)
{
	return FESQLPropertySerializer::GetSQLiteType(Property);
}

FString FESQLStructValidator::GetPropertyTypeName(const FProperty* Property)
{
	if (!Property) return TEXT("unknown");

	if (CastField<FBoolProperty>(Property)) return TEXT("bool");
	if (CastField<FIntProperty>(Property)) return TEXT("int32");
	if (CastField<FInt64Property>(Property)) return TEXT("int64");
	if (CastField<FFloatProperty>(Property)) return TEXT("float");
	if (CastField<FDoubleProperty>(Property)) return TEXT("double");
	if (CastField<FStrProperty>(Property)) return TEXT("FString");
	if (CastField<FNameProperty>(Property)) return TEXT("FName");
	if (CastField<FTextProperty>(Property)) return TEXT("FText");
	if (CastField<FByteProperty>(Property)) return TEXT("uint8");
	if (const FOptionalProperty* OptionalProp = CastField<FOptionalProperty>(Property))
	{
		return FString::Printf(TEXT("TOptional<%s>"), *GetPropertyTypeName(OptionalProp->GetValueProperty()));
	}

	if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		if (const UEnum* Enum = EnumProp->GetEnum())
		{
			return Enum->GetName();
		}
		return TEXT("enum");
	}

	if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		return FString::Printf(TEXT("TArray<%s>"), *GetPropertyTypeName(ArrayProp->Inner));
	}

	if (CastField<FMapProperty>(Property)) return TEXT("TMap<>");
	if (CastField<FSetProperty>(Property)) return TEXT("TSet<>");

	if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct)
		{
			return StructProp->Struct->GetName();
		}
		return TEXT("UStruct");
	}

	if (CastField<FObjectPropertyBase>(Property)) return TEXT("UObject*");
	if (CastField<FSoftClassProperty>(Property)) return TEXT("TSoftClassPtr");
	if (CastField<FSoftObjectProperty>(Property)) return TEXT("TSoftObjectPtr");
	if (CastField<FDelegateProperty>(Property)) return TEXT("Delegate");
	if (CastField<FMulticastDelegateProperty>(Property)) return TEXT("MulticastDelegate");

	return Property->GetCPPType();
}


// ── Column Definition Builder ────────────────────────────────────────────────

FString FESQLStructValidator::BuildColumnDefinition(const UScriptStruct* Struct)
{
	if (!Struct) return FString();

	TArray<FFieldResult> Results;
	if (!Validate(Struct, Results))
	{
		UE_LOG(LogExtendedSQL, Warning, TEXT("BuildColumnDefinition: Struct '%s' has invalid fields"), *Struct->GetName());
		return FString();
	}

	FString Definition;
	bool bFirst = true;

	for (const FFieldResult& Field : Results)
	{
		if (!Field.bIsValid) continue;

		if (!bFirst) Definition += TEXT(", ");
		Definition += FString::Printf(TEXT("\"%s\" %s"), *Field.FieldName, *Field.SQLiteType);
		bFirst = false;
	}

	return Definition;
}

TMap<FString, FString> FESQLStructValidator::BuildColumnMap(const UScriptStruct* Struct)
{
	TMap<FString, FString> Result;
	if (!Struct) return Result;

	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		FProperty* Property = *It;
		FString SQLType = MapPropertyToSQLiteType(Property);
		if (!SQLType.IsEmpty())
		{
			Result.Add(Property->GetName(), SQLType);
		}
	}

	return Result;
}
