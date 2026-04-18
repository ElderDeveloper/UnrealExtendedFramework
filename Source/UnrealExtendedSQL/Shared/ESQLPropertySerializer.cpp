// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Shared/ESQLPropertySerializer.h"

#include "Internationalization/Text.h"
#include "JsonObjectConverter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/PropertyOptional.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

#include "GameplayTagContainer.h"

namespace
{
const FName DateTimeStructName(TEXT("DateTime"));
const FName TimespanStructName(TEXT("Timespan"));
const FName GuidStructName(TEXT("Guid"));
const FName SoftObjectPathStructName(TEXT("SoftObjectPath"));
const FName SoftClassPathStructName(TEXT("SoftClassPath"));
const FName PrimaryAssetTypeStructName(TEXT("PrimaryAssetType"));
const FName PrimaryAssetIdStructName(TEXT("PrimaryAssetId"));

bool IsNamedStruct(const UStruct* Struct, const FName StructName)
{
	return Struct && Struct->GetFName() == StructName;
}

bool IsGameplayTagStruct(const UStruct* Struct)
{
	return Struct == FGameplayTag::StaticStruct();
}

bool IsGameplayTagContainerStruct(const UStruct* Struct)
{
	return Struct == FGameplayTagContainer::StaticStruct();
}

bool IsPlainTextStruct(const FStructProperty* StructProperty)
{
	if (!StructProperty || !StructProperty->Struct)
	{
		return false;
	}

	if (IsGameplayTagStruct(StructProperty->Struct))
	{
		return true;
	}

	return IsNamedStruct(StructProperty->Struct, DateTimeStructName)
		|| IsNamedStruct(StructProperty->Struct, TimespanStructName)
		|| IsNamedStruct(StructProperty->Struct, GuidStructName)
		|| IsNamedStruct(StructProperty->Struct, SoftObjectPathStructName)
		|| IsNamedStruct(StructProperty->Struct, SoftClassPathStructName)
		|| IsNamedStruct(StructProperty->Struct, PrimaryAssetTypeStructName)
		|| IsNamedStruct(StructProperty->Struct, PrimaryAssetIdStructName);
}

bool IsJsonTextProperty(const FProperty* Property)
{
	if (!Property)
	{
		return false;
	}

	if (const FOptionalProperty* OptionalProperty = CastField<FOptionalProperty>(Property))
	{
		return IsJsonTextProperty(OptionalProperty->GetValueProperty());
	}

	if (CastField<FArrayProperty>(Property) || CastField<FSetProperty>(Property) || CastField<FMapProperty>(Property))
	{
		return true;
	}

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		return !IsPlainTextStruct(StructProperty);
	}

	return false;
}

TSharedPtr<FJsonValue> ExportJsonProperty(FProperty* Property, const void* Value)
{
	if (!Property || !Value)
	{
		return nullptr;
	}

	if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
	{
		const FSoftObjectPtr& SoftObjectPtr = SoftObjectProperty->GetPropertyValue(Value);
		return MakeShared<FJsonValueString>(SoftObjectPtr.ToSoftObjectPath().ToString());
	}

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (IsGameplayTagStruct(StructProperty->Struct))
		{
			const FGameplayTag& TagValue = *static_cast<const FGameplayTag*>(Value);
			return MakeShared<FJsonValueString>(TagValue.ToString());
		}

		if (IsGameplayTagContainerStruct(StructProperty->Struct))
		{
			const FGameplayTagContainer& TagContainer = *static_cast<const FGameplayTagContainer*>(Value);
			TArray<TSharedPtr<FJsonValue>> TagValues;
			for (const FGameplayTag& Tag : TagContainer)
			{
				TagValues.Add(MakeShared<FJsonValueString>(Tag.ToString()));
			}
			return MakeShared<FJsonValueArray>(TagValues);
		}
	}

	return nullptr;
}

bool ImportJsonProperty(const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property, void* Value)
{
	if (!JsonValue.IsValid() || !Property || !Value)
	{
		return false;
	}

	if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
	{
		FString PathString;
		if (!JsonValue->TryGetString(PathString))
		{
			return false;
		}

		SoftObjectProperty->SetPropertyValue(Value, FSoftObjectPtr(FSoftObjectPath(PathString)));
		return true;
	}

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (IsGameplayTagStruct(StructProperty->Struct))
		{
			FString TagString;
			if (!JsonValue->TryGetString(TagString))
			{
				return false;
			}

			FGameplayTag& TagValue = *static_cast<FGameplayTag*>(Value);
			TagValue = TagString.IsEmpty()
				? FGameplayTag::EmptyTag
				: FGameplayTag::RequestGameplayTag(FName(*TagString), false);
			return true;
		}

		if (IsGameplayTagContainerStruct(StructProperty->Struct))
		{
			const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
			if (!JsonValue->TryGetArray(JsonArray) || !JsonArray)
			{
				return false;
			}

			FGameplayTagContainer& TagContainer = *static_cast<FGameplayTagContainer*>(Value);
			TagContainer = FGameplayTagContainer();

			for (const TSharedPtr<FJsonValue>& Entry : *JsonArray)
			{
				if (!Entry.IsValid())
				{
					continue;
				}

				FString TagString;
				if (!Entry->TryGetString(TagString) || TagString.IsEmpty())
				{
					continue;
				}

				const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
				if (Tag.IsValid())
				{
					TagContainer.AddTag(Tag);
				}
			}

			return true;
		}
	}

	return false;
}

bool SerializeJsonPropertyToString(FProperty* Property, const void* ValuePtr, FString& OutValue)
{
	FJsonObjectConverter::CustomExportCallback ExportCallback;
	ExportCallback.BindStatic(&ExportJsonProperty);

	const TSharedPtr<FJsonValue> JsonValue = FJsonObjectConverter::UPropertyToJsonValue(
		Property,
		ValuePtr,
		0,
		0,
		&ExportCallback,
		nullptr,
		EJsonObjectConversionFlags::WriteTextAsComplexString);

	if (!JsonValue.IsValid())
	{
		return false;
	}

	OutValue.Reset();
	if (JsonValue->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject> JsonObject = JsonValue->AsObject();
		if (!JsonObject.IsValid())
		{
			return false;
		}

		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutValue);
		return FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	}

	if (JsonValue->Type == EJson::Array)
	{
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutValue);
		return FJsonSerializer::Serialize(JsonValue->AsArray(), Writer);
	}

	return false;
}

bool DeserializeJsonPropertyFromString(FProperty* Property, void* ValuePtr, const FString& InValue)
{
	if (InValue.IsEmpty())
	{
		return true;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InValue);
	TSharedPtr<FJsonValue> JsonValue;
	if (!FJsonSerializer::Deserialize(Reader, JsonValue) || !JsonValue.IsValid())
	{
		return false;
	}

	FJsonObjectConverter::CustomImportCallback ImportCallback;
	ImportCallback.BindStatic(&ImportJsonProperty);

	FText FailReason;
	return FJsonObjectConverter::JsonValueToUProperty(JsonValue, Property, ValuePtr, 0, 0, false, &FailReason, &ImportCallback);
}
}

FString FESQLPropertySerializer::GetSQLiteType(const FProperty* Property)
{
	if (!Property)
	{
		return FString();
	}

	if (CastField<FBoolProperty>(Property))
	{
		return TEXT("INTEGER");
	}

	if (CastField<FIntProperty>(Property)
		|| CastField<FInt64Property>(Property)
		|| CastField<FUInt32Property>(Property)
		|| CastField<FInt16Property>(Property)
		|| CastField<FUInt16Property>(Property)
		|| CastField<FByteProperty>(Property)
		|| CastField<FEnumProperty>(Property))
	{
		return TEXT("INTEGER");
	}

	if (CastField<FFloatProperty>(Property) || CastField<FDoubleProperty>(Property))
	{
		return TEXT("REAL");
	}

	if (CastField<FStrProperty>(Property)
		|| CastField<FNameProperty>(Property)
		|| CastField<FTextProperty>(Property)
		|| CastField<FSoftObjectProperty>(Property)
		|| CastField<FSoftClassProperty>(Property)
		|| CastField<FArrayProperty>(Property)
		|| CastField<FSetProperty>(Property)
		|| CastField<FMapProperty>(Property))
	{
		return TEXT("TEXT");
	}

	if (const FOptionalProperty* OptionalProperty = CastField<FOptionalProperty>(Property))
	{
		return GetSQLiteType(OptionalProperty->GetValueProperty());
	}

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (StructProperty->Struct)
		{
			if (IsGameplayTagStruct(StructProperty->Struct)
				|| IsGameplayTagContainerStruct(StructProperty->Struct)
				|| IsNamedStruct(StructProperty->Struct, DateTimeStructName)
				|| IsNamedStruct(StructProperty->Struct, TimespanStructName)
				|| IsNamedStruct(StructProperty->Struct, GuidStructName)
				|| IsNamedStruct(StructProperty->Struct, SoftObjectPathStructName)
				|| IsNamedStruct(StructProperty->Struct, SoftClassPathStructName)
				|| IsNamedStruct(StructProperty->Struct, PrimaryAssetTypeStructName)
				|| IsNamedStruct(StructProperty->Struct, PrimaryAssetIdStructName))
			{
				return TEXT("TEXT");
			}

			return TEXT("TEXT");
		}
	}

	return FString();
}

bool FESQLPropertySerializer::SerializePropertyToBindingValue(const FProperty* Property, const void* ContainerData, FESQLBindingValue& OutValue)
{
	if (!Property || !ContainerData)
	{
		return false;
	}

	return SerializePropertyValueToBindingValue(
		const_cast<FProperty*>(Property),
		Property->ContainerPtrToValuePtr<void>(ContainerData),
		OutValue);
}

bool FESQLPropertySerializer::SerializePropertyToString(const FProperty* Property, const void* ContainerData, FString& OutValue)
{
	FESQLBindingValue BindingValue;
	if (!SerializePropertyToBindingValue(Property, ContainerData, BindingValue))
	{
		return false;
	}

	OutValue = BindingValue.bIsNull ? FString() : MoveTemp(BindingValue.TextValue);
	return true;
}

bool FESQLPropertySerializer::DeserializePropertyFromString(const FProperty* Property, void* ContainerData, const FString& InValue, bool bIsNull)
{
	if (!Property || !ContainerData)
	{
		return false;
	}

	return DeserializePropertyValueFromString(
		const_cast<FProperty*>(Property),
		Property->ContainerPtrToValuePtr<void>(ContainerData),
		InValue,
		bIsNull);
}

bool FESQLPropertySerializer::SerializePropertyValueToBindingValue(FProperty* Property, const void* ValuePtr, FESQLBindingValue& OutValue)
{
	if (!Property || !ValuePtr)
	{
		return false;
	}

	if (FOptionalProperty* OptionalProperty = CastField<FOptionalProperty>(Property))
	{
		const FOptionalPropertyLayout OptionalLayout(OptionalProperty->GetValueProperty());
		if (!OptionalLayout.IsSet(ValuePtr))
		{
			OutValue = FESQLBindingValue::Null();
			return true;
		}

		return SerializePropertyValueToBindingValue(
			OptionalProperty->GetValueProperty(),
			OptionalLayout.GetValuePointerForRead(ValuePtr),
			OutValue);
	}

	if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		OutValue = FESQLBindingValue::FromInteger(BoolProperty->GetPropertyValue(ValuePtr) ? 1 : 0);
		return true;
	}

	if (const FIntProperty* IntProperty = CastField<FIntProperty>(Property))
	{
		OutValue = FESQLBindingValue::FromInteger(IntProperty->GetPropertyValue(ValuePtr));
		return true;
	}

	if (const FInt64Property* Int64Property = CastField<FInt64Property>(Property))
	{
		OutValue = FESQLBindingValue::FromInteger(Int64Property->GetPropertyValue(ValuePtr));
		return true;
	}

	if (const FUInt32Property* UInt32Property = CastField<FUInt32Property>(Property))
	{
		OutValue = FESQLBindingValue::FromInteger(UInt32Property->GetPropertyValue(ValuePtr));
		return true;
	}

	if (const FInt16Property* Int16Property = CastField<FInt16Property>(Property))
	{
		OutValue = FESQLBindingValue::FromInteger(Int16Property->GetPropertyValue(ValuePtr));
		return true;
	}

	if (const FUInt16Property* UInt16Property = CastField<FUInt16Property>(Property))
	{
		OutValue = FESQLBindingValue::FromInteger(UInt16Property->GetPropertyValue(ValuePtr));
		return true;
	}

	if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		OutValue = FESQLBindingValue::FromInteger(ByteProperty->GetPropertyValue(ValuePtr));
		return true;
	}

	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		if (const FNumericProperty* NumericProperty = EnumProperty->GetUnderlyingProperty())
		{
			OutValue = FESQLBindingValue::FromInteger(NumericProperty->GetSignedIntPropertyValue(ValuePtr));
			return true;
		}
	}

	if (const FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
	{
		OutValue = FESQLBindingValue::FromFloat(FloatProperty->GetPropertyValue(ValuePtr));
		return true;
	}

	if (const FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
	{
		OutValue = FESQLBindingValue::FromFloat(DoubleProperty->GetPropertyValue(ValuePtr));
		return true;
	}

	if (const FStrProperty* StrProperty = CastField<FStrProperty>(Property))
	{
		OutValue = FESQLBindingValue::FromText(StrProperty->GetPropertyValue(ValuePtr));
		return true;
	}

	if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
	{
		OutValue = FESQLBindingValue::FromText(NameProperty->GetPropertyValue(ValuePtr).ToString());
		return true;
	}

	if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
	{
		FString TextValue;
		FTextStringHelper::WriteToBuffer(TextValue, TextProperty->GetPropertyValue(ValuePtr));
		OutValue = FESQLBindingValue::FromText(MoveTemp(TextValue));
		return true;
	}

	if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
	{
		OutValue = FESQLBindingValue::FromText(SoftObjectProperty->GetPropertyValue(ValuePtr).ToSoftObjectPath().ToString());
		return true;
	}

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (StructProperty->Struct && IsGameplayTagStruct(StructProperty->Struct))
		{
			OutValue = FESQLBindingValue::FromText(static_cast<const FGameplayTag*>(ValuePtr)->ToString());
			return true;
		}

		if (IsPlainTextStruct(StructProperty))
		{
			FString TextValue;
			Property->ExportTextItem_Direct(TextValue, ValuePtr, nullptr, nullptr, PPF_None);
			OutValue = FESQLBindingValue::FromText(MoveTemp(TextValue));
			return true;
		}
	}

	if (IsJsonTextProperty(Property))
	{
		FString JsonValue;
		if (!SerializeJsonPropertyToString(Property, ValuePtr, JsonValue))
		{
			return false;
		}

		OutValue = FESQLBindingValue::FromText(MoveTemp(JsonValue));
		return true;
	}

	return false;
}

bool FESQLPropertySerializer::DeserializePropertyValueFromString(FProperty* Property, void* ValuePtr, const FString& InValue, bool bIsNull)
{
	if (!Property || !ValuePtr)
	{
		return false;
	}

	if (FOptionalProperty* OptionalProperty = CastField<FOptionalProperty>(Property))
	{
		const FOptionalPropertyLayout OptionalLayout(OptionalProperty->GetValueProperty());
		if (bIsNull)
		{
			OptionalLayout.MarkUnset(ValuePtr);
			return true;
		}

		void* InnerValuePtr = OptionalLayout.MarkSetAndGetInitializedValuePointerToReplace(ValuePtr);
		return DeserializePropertyValueFromString(OptionalProperty->GetValueProperty(), InnerValuePtr, InValue, false);
	}

	if (bIsNull)
	{
		return true;
	}

	if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		BoolProperty->SetPropertyValue(ValuePtr, InValue == TEXT("1") || InValue.ToBool());
		return true;
	}

	if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
	{
		IntProperty->SetPropertyValue(ValuePtr, FCString::Atoi(*InValue));
		return true;
	}

	if (FInt64Property* Int64Property = CastField<FInt64Property>(Property))
	{
		Int64Property->SetPropertyValue(ValuePtr, FCString::Atoi64(*InValue));
		return true;
	}

	if (FUInt32Property* UInt32Property = CastField<FUInt32Property>(Property))
	{
		UInt32Property->SetPropertyValue(ValuePtr, (uint32)FCString::Strtoui64(*InValue, nullptr, 10));
		return true;
	}

	if (FInt16Property* Int16Property = CastField<FInt16Property>(Property))
	{
		Int16Property->SetPropertyValue(ValuePtr, (int16)FCString::Atoi(*InValue));
		return true;
	}

	if (FUInt16Property* UInt16Property = CastField<FUInt16Property>(Property))
	{
		UInt16Property->SetPropertyValue(ValuePtr, (uint16)FCString::Strtoui64(*InValue, nullptr, 10));
		return true;
	}

	if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		ByteProperty->SetPropertyValue(ValuePtr, (uint8)FCString::Atoi(*InValue));
		return true;
	}

	if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		if (FNumericProperty* NumericProperty = EnumProperty->GetUnderlyingProperty())
		{
			NumericProperty->SetIntPropertyValue(ValuePtr, FCString::Atoi64(*InValue));
			return true;
		}
	}

	if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
	{
		FloatProperty->SetPropertyValue(ValuePtr, FCString::Atof(*InValue));
		return true;
	}

	if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
	{
		DoubleProperty->SetPropertyValue(ValuePtr, FCString::Atod(*InValue));
		return true;
	}

	if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
	{
		StrProperty->SetPropertyValue(ValuePtr, InValue);
		return true;
	}

	if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
	{
		NameProperty->SetPropertyValue(ValuePtr, FName(*InValue));
		return true;
	}

	if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
	{
		TextProperty->SetPropertyValue(ValuePtr, FTextStringHelper::CreateFromBuffer(*InValue));
		return true;
	}

	if (FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
	{
		SoftObjectProperty->SetPropertyValue(ValuePtr, FSoftObjectPtr(FSoftObjectPath(InValue)));
		return true;
	}

	if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (StructProperty->Struct && IsGameplayTagStruct(StructProperty->Struct))
		{
			FGameplayTag& TagValue = *static_cast<FGameplayTag*>(ValuePtr);
			TagValue = InValue.IsEmpty() ? FGameplayTag::EmptyTag : FGameplayTag::RequestGameplayTag(FName(*InValue), false);
			return true;
		}

		if (IsPlainTextStruct(StructProperty))
		{
			return Property->ImportText_Direct(*InValue, ValuePtr, nullptr, PPF_None) != nullptr;
		}
	}

	if (IsJsonTextProperty(Property))
	{
		return DeserializeJsonPropertyFromString(Property, ValuePtr, InValue);
	}

	return false;
}