// Copyright Moon Punch Games. All Rights Reserved.

#include "Backend/Json/EFJsonLibrary.h"

#include "Blueprint/BlueprintExceptionInfo.h"
#include "HAL/FileManager.h"
#include "Libraries/File/EFFileLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealExtendedFramework.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "EFJsonLibrary"

namespace EFJsonLibraryPrivate
{
	/** The object to write to: the input when it is valid, otherwise a new one. */
	TSharedPtr<FJsonObject> ObjectForWrite(const FEFJsonObject& JsonObject)
	{
		return JsonObject.Object.IsValid() ? JsonObject.Object : TSharedPtr<FJsonObject>(MakeShared<FJsonObject>());
	}

	TSharedPtr<FJsonValue> ValueOrNull(const FEFJsonValue& Value)
	{
		return Value.Value.IsValid() ? Value.Value : TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>());
	}

	TArray<TSharedPtr<FJsonValue>> ToEngineArray(const TArray<FEFJsonValue>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		Out.Reserve(Values.Num());
		for (const FEFJsonValue& Value : Values)
		{
			Out.Add(ValueOrNull(Value));
		}
		return Out;
	}

	TArray<FEFJsonValue> FromEngineArray(const TArray<TSharedPtr<FJsonValue>>& Values)
	{
		TArray<FEFJsonValue> Out;
		Out.Reserve(Values.Num());
		for (const TSharedPtr<FJsonValue>& Value : Values)
		{
			Out.Emplace(Value.IsValid() ? Value : TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
		}
		return Out;
	}

	const TSharedPtr<FJsonValue>* FindField(const FEFJsonObject& JsonObject, const FString& FieldName)
	{
		return JsonObject.Object.IsValid() ? JsonObject.Object->Values.Find(FieldName) : nullptr;
	}

	/**
	 * Absolute paths are used as they are. A path starting with "../" is the engine's own relative
	 * form (what FPaths::ProjectSavedDir() and friends return) and is resolved the way the engine
	 * resolves it. Any other relative path, such as "Saved/Data/Profile.json", is project-relative.
	 */
	FString ResolveFilePath(const FString& FilePath)
	{
		if (!FPaths::IsRelative(FilePath) || FilePath.StartsWith(TEXT("../")) || FilePath.StartsWith(TEXT("..\\")))
		{
			return FPaths::ConvertRelativePathToFull(FilePath);
		}
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), FilePath));
	}
}

// ─── Create, parse, serialize ───────────────────────────────────────────────────

FEFJsonObject UEFJsonLibrary::MakeJsonObject()
{
	return FEFJsonObject::MakeEmpty();
}

FEFJsonObject UEFJsonLibrary::CopyJsonObject(const FEFJsonObject& JsonObject)
{
	return FEFJsonObject(EFJson::DeepCopyObject(JsonObject.Object));
}

bool UEFJsonLibrary::ParseJsonObject(const FString& JsonString, FEFJsonObject& OutObject)
{
	TSharedPtr<FJsonObject> Parsed;
	const bool bParsed = EFJson::ParseObject(JsonString, Parsed);
	OutObject = FEFJsonObject(Parsed);
	return bParsed;
}

bool UEFJsonLibrary::ParseJsonValue(const FString& JsonString, FEFJsonValue& OutValue)
{
	TSharedPtr<FJsonValue> Parsed;
	const bool bParsed = EFJson::ParseValue(JsonString, Parsed);
	OutValue = FEFJsonValue(Parsed);
	return bParsed;
}

FString UEFJsonLibrary::JsonObjectToString(const FEFJsonObject& JsonObject, bool bPrettyPrint)
{
	return EFJson::ObjectToString(JsonObject.Object, bPrettyPrint);
}

FString UEFJsonLibrary::JsonValueToString(const FEFJsonValue& JsonValue, bool bPrettyPrint)
{
	return EFJson::ValueToString(JsonValue.Value, bPrettyPrint);
}

bool UEFJsonLibrary::LoadJsonObjectFromFile(const FString& FilePath, FEFJsonObject& OutObject)
{
	OutObject = FEFJsonObject();

	FString Text;
	if (FilePath.IsEmpty() || !FFileHelper::LoadFileToString(Text, *EFJsonLibraryPrivate::ResolveFilePath(FilePath)))
	{
		return false;
	}

	return ParseJsonObject(Text, OutObject);
}

bool UEFJsonLibrary::SaveJsonObjectToFile(const FEFJsonObject& JsonObject, const FString& FilePath, bool bPrettyPrint)
{
	if (!JsonObject.IsValid() || FilePath.IsEmpty())
	{
		return false;
	}

	const FString FullPath = EFJsonLibraryPrivate::ResolveFilePath(FilePath);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullPath), /*Tree*/ true);
	return FFileHelper::SaveStringToFile(EFJson::ObjectToString(JsonObject.Object, bPrettyPrint), *FullPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

// ─── Inspect ────────────────────────────────────────────────────────────────────

bool UEFJsonLibrary::IsValidJsonObject(const FEFJsonObject& JsonObject)
{
	return JsonObject.IsValid();
}

bool UEFJsonLibrary::HasField(const FEFJsonObject& JsonObject, const FString& FieldName)
{
	return EFJsonLibraryPrivate::FindField(JsonObject, FieldName) != nullptr;
}

TArray<FString> UEFJsonLibrary::GetFieldNames(const FEFJsonObject& JsonObject)
{
	TArray<FString> Names;
	if (JsonObject.IsValid())
	{
		JsonObject.Object->Values.GetKeys(Names);
	}
	return Names;
}

EEFJsonType UEFJsonLibrary::GetFieldType(const FEFJsonObject& JsonObject, const FString& FieldName)
{
	const TSharedPtr<FJsonValue>* Field = EFJsonLibraryPrivate::FindField(JsonObject, FieldName);
	return Field ? EFJson::ToType(*Field) : EEFJsonType::None;
}

// ─── Read fields ────────────────────────────────────────────────────────────────

bool UEFJsonLibrary::TryGetStringField(const FEFJsonObject& JsonObject, const FString& FieldName, FString& OutValue)
{
	OutValue.Reset();
	const TSharedPtr<FJsonValue>* Field = EFJsonLibraryPrivate::FindField(JsonObject, FieldName);
	return Field && Field->IsValid() && (*Field)->TryGetString(OutValue);
}

bool UEFJsonLibrary::TryGetNumberField(const FEFJsonObject& JsonObject, const FString& FieldName, double& OutValue)
{
	OutValue = 0.0;
	const TSharedPtr<FJsonValue>* Field = EFJsonLibraryPrivate::FindField(JsonObject, FieldName);
	return Field && Field->IsValid() && (*Field)->TryGetNumber(OutValue);
}

bool UEFJsonLibrary::TryGetIntegerField(const FEFJsonObject& JsonObject, const FString& FieldName, int64& OutValue)
{
	OutValue = 0;
	const TSharedPtr<FJsonValue>* Field = EFJsonLibraryPrivate::FindField(JsonObject, FieldName);
	return Field && Field->IsValid() && (*Field)->TryGetNumber(OutValue);
}

bool UEFJsonLibrary::TryGetBoolField(const FEFJsonObject& JsonObject, const FString& FieldName, bool& OutValue)
{
	OutValue = false;
	const TSharedPtr<FJsonValue>* Field = EFJsonLibraryPrivate::FindField(JsonObject, FieldName);
	return Field && Field->IsValid() && (*Field)->TryGetBool(OutValue);
}

bool UEFJsonLibrary::TryGetObjectField(const FEFJsonObject& JsonObject, const FString& FieldName, FEFJsonObject& OutValue)
{
	OutValue = FEFJsonObject();
	const TSharedPtr<FJsonValue>* Field = EFJsonLibraryPrivate::FindField(JsonObject, FieldName);
	return Field && TryGetObjectValue(FEFJsonValue(*Field), OutValue);
}

bool UEFJsonLibrary::TryGetArrayField(const FEFJsonObject& JsonObject, const FString& FieldName, TArray<FEFJsonValue>& OutValue)
{
	OutValue.Reset();
	const TSharedPtr<FJsonValue>* Field = EFJsonLibraryPrivate::FindField(JsonObject, FieldName);
	return Field && TryGetArrayValue(FEFJsonValue(*Field), OutValue);
}

bool UEFJsonLibrary::TryGetStringArrayField(const FEFJsonObject& JsonObject, const FString& FieldName, TArray<FString>& OutValue)
{
	OutValue.Reset();

	TArray<FEFJsonValue> Elements;
	if (!TryGetArrayField(JsonObject, FieldName, Elements))
	{
		return false;
	}

	OutValue.Reserve(Elements.Num());
	for (const FEFJsonValue& Element : Elements)
	{
		const EEFJsonType Type = Element.GetType();
		FString Text;
		if ((Type != EEFJsonType::String && Type != EEFJsonType::Number && Type != EEFJsonType::Boolean) || !Element.Value->TryGetString(Text))
		{
			OutValue.Reset();
			return false;
		}
		OutValue.Add(MoveTemp(Text));
	}
	return true;
}

bool UEFJsonLibrary::TryGetObjectArrayField(const FEFJsonObject& JsonObject, const FString& FieldName, TArray<FEFJsonObject>& OutValue)
{
	OutValue.Reset();

	TArray<FEFJsonValue> Elements;
	if (!TryGetArrayField(JsonObject, FieldName, Elements))
	{
		return false;
	}

	OutValue.Reserve(Elements.Num());
	for (const FEFJsonValue& Element : Elements)
	{
		FEFJsonObject ElementObject;
		if (!TryGetObjectValue(Element, ElementObject))
		{
			OutValue.Reset();
			return false;
		}
		OutValue.Add(MoveTemp(ElementObject));
	}
	return true;
}

bool UEFJsonLibrary::TryGetField(const FEFJsonObject& JsonObject, const FString& FieldName, FEFJsonValue& OutValue)
{
	const TSharedPtr<FJsonValue>* Field = EFJsonLibraryPrivate::FindField(JsonObject, FieldName);
	OutValue = FEFJsonValue(Field ? *Field : TSharedPtr<FJsonValue>());
	return OutValue.IsValid();
}

bool UEFJsonLibrary::TryGetFieldByPath(const FEFJsonObject& JsonObject, const FString& Path, FEFJsonValue& OutValue)
{
	TSharedPtr<FJsonValue> Resolved;
	const bool bResolved = EFJson::ResolvePath(JsonObject.Object, Path, Resolved);
	OutValue = FEFJsonValue(Resolved);
	return bResolved;
}

// ─── Write fields ───────────────────────────────────────────────────────────────

FEFJsonObject UEFJsonLibrary::SetStringField(const FEFJsonObject& JsonObject, const FString& FieldName, const FString& Value)
{
	const TSharedPtr<FJsonObject> Target = EFJsonLibraryPrivate::ObjectForWrite(JsonObject);
	Target->SetField(FieldName, MakeShared<FJsonValueString>(Value));
	return FEFJsonObject(Target);
}

FEFJsonObject UEFJsonLibrary::SetNumberField(const FEFJsonObject& JsonObject, const FString& FieldName, double Value)
{
	const TSharedPtr<FJsonObject> Target = EFJsonLibraryPrivate::ObjectForWrite(JsonObject);
	Target->SetField(FieldName, MakeShared<FJsonValueNumber>(Value));
	return FEFJsonObject(Target);
}

FEFJsonObject UEFJsonLibrary::SetIntegerField(const FEFJsonObject& JsonObject, const FString& FieldName, int64 Value)
{
	// JSON numbers are doubles; integers beyond +/-2^53 lose precision, as they would in any JSON reader.
	return SetNumberField(JsonObject, FieldName, static_cast<double>(Value));
}

FEFJsonObject UEFJsonLibrary::SetBoolField(const FEFJsonObject& JsonObject, const FString& FieldName, bool Value)
{
	const TSharedPtr<FJsonObject> Target = EFJsonLibraryPrivate::ObjectForWrite(JsonObject);
	Target->SetField(FieldName, MakeShared<FJsonValueBoolean>(Value));
	return FEFJsonObject(Target);
}

FEFJsonObject UEFJsonLibrary::SetNullField(const FEFJsonObject& JsonObject, const FString& FieldName)
{
	const TSharedPtr<FJsonObject> Target = EFJsonLibraryPrivate::ObjectForWrite(JsonObject);
	Target->SetField(FieldName, MakeShared<FJsonValueNull>());
	return FEFJsonObject(Target);
}

FEFJsonObject UEFJsonLibrary::SetObjectField(const FEFJsonObject& JsonObject, const FString& FieldName, const FEFJsonObject& Value)
{
	const TSharedPtr<FJsonObject> Target = EFJsonLibraryPrivate::ObjectForWrite(JsonObject);
	if (Value.IsValid())
	{
		Target->SetField(FieldName, MakeShared<FJsonValueObject>(Value.Object));
	}
	else
	{
		Target->SetField(FieldName, MakeShared<FJsonValueNull>());
	}
	return FEFJsonObject(Target);
}

FEFJsonObject UEFJsonLibrary::SetArrayField(const FEFJsonObject& JsonObject, const FString& FieldName, const TArray<FEFJsonValue>& Value)
{
	const TSharedPtr<FJsonObject> Target = EFJsonLibraryPrivate::ObjectForWrite(JsonObject);
	Target->SetField(FieldName, MakeShared<FJsonValueArray>(EFJsonLibraryPrivate::ToEngineArray(Value)));
	return FEFJsonObject(Target);
}

FEFJsonObject UEFJsonLibrary::SetStringArrayField(const FEFJsonObject& JsonObject, const FString& FieldName, const TArray<FString>& Value)
{
	TArray<TSharedPtr<FJsonValue>> Elements;
	Elements.Reserve(Value.Num());
	for (const FString& Element : Value)
	{
		Elements.Add(MakeShared<FJsonValueString>(Element));
	}

	const TSharedPtr<FJsonObject> Target = EFJsonLibraryPrivate::ObjectForWrite(JsonObject);
	Target->SetField(FieldName, MakeShared<FJsonValueArray>(Elements));
	return FEFJsonObject(Target);
}

FEFJsonObject UEFJsonLibrary::SetField(const FEFJsonObject& JsonObject, const FString& FieldName, const FEFJsonValue& Value)
{
	const TSharedPtr<FJsonObject> Target = EFJsonLibraryPrivate::ObjectForWrite(JsonObject);
	Target->SetField(FieldName, EFJsonLibraryPrivate::ValueOrNull(Value));
	return FEFJsonObject(Target);
}

FEFJsonObject UEFJsonLibrary::RemoveField(const FEFJsonObject& JsonObject, const FString& FieldName)
{
	if (JsonObject.IsValid())
	{
		JsonObject.Object->RemoveField(FieldName);
	}
	return JsonObject;
}

// ─── Values ─────────────────────────────────────────────────────────────────────

FEFJsonValue UEFJsonLibrary::MakeJsonStringValue(const FString& Value)
{
	return FEFJsonValue(MakeShared<FJsonValueString>(Value));
}

FEFJsonValue UEFJsonLibrary::MakeJsonNumberValue(double Value)
{
	return FEFJsonValue(MakeShared<FJsonValueNumber>(Value));
}

FEFJsonValue UEFJsonLibrary::MakeJsonIntegerValue(int64 Value)
{
	return FEFJsonValue(MakeShared<FJsonValueNumber>(static_cast<double>(Value)));
}

FEFJsonValue UEFJsonLibrary::MakeJsonBoolValue(bool Value)
{
	return FEFJsonValue(MakeShared<FJsonValueBoolean>(Value));
}

FEFJsonValue UEFJsonLibrary::MakeJsonNullValue()
{
	return FEFJsonValue(MakeShared<FJsonValueNull>());
}

FEFJsonValue UEFJsonLibrary::MakeJsonObjectValue(const FEFJsonObject& Value)
{
	if (!Value.IsValid())
	{
		return MakeJsonNullValue();
	}
	return FEFJsonValue(MakeShared<FJsonValueObject>(Value.Object));
}

FEFJsonValue UEFJsonLibrary::MakeJsonArrayValue(const TArray<FEFJsonValue>& Value)
{
	return FEFJsonValue(MakeShared<FJsonValueArray>(EFJsonLibraryPrivate::ToEngineArray(Value)));
}

EEFJsonType UEFJsonLibrary::GetJsonValueType(const FEFJsonValue& JsonValue)
{
	return JsonValue.GetType();
}

bool UEFJsonLibrary::TryGetStringValue(const FEFJsonValue& JsonValue, FString& OutValue)
{
	OutValue.Reset();
	return JsonValue.IsValid() && JsonValue.Value->TryGetString(OutValue);
}

bool UEFJsonLibrary::TryGetNumberValue(const FEFJsonValue& JsonValue, double& OutValue)
{
	OutValue = 0.0;
	return JsonValue.IsValid() && JsonValue.Value->TryGetNumber(OutValue);
}

bool UEFJsonLibrary::TryGetIntegerValue(const FEFJsonValue& JsonValue, int64& OutValue)
{
	OutValue = 0;
	return JsonValue.IsValid() && JsonValue.Value->TryGetNumber(OutValue);
}

bool UEFJsonLibrary::TryGetBoolValue(const FEFJsonValue& JsonValue, bool& OutValue)
{
	OutValue = false;
	return JsonValue.IsValid() && JsonValue.Value->TryGetBool(OutValue);
}

bool UEFJsonLibrary::TryGetObjectValue(const FEFJsonValue& JsonValue, FEFJsonObject& OutValue)
{
	OutValue = FEFJsonObject();
	if (JsonValue.GetType() != EEFJsonType::Object)
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* Object = nullptr;
	if (!JsonValue.Value->TryGetObject(Object) || !Object || !Object->IsValid())
	{
		return false;
	}

	OutValue = FEFJsonObject(*Object);
	return true;
}

bool UEFJsonLibrary::TryGetArrayValue(const FEFJsonValue& JsonValue, TArray<FEFJsonValue>& OutValue)
{
	OutValue.Reset();
	if (JsonValue.GetType() != EEFJsonType::Array)
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (!JsonValue.Value->TryGetArray(Array) || !Array)
	{
		return false;
	}

	OutValue = EFJsonLibraryPrivate::FromEngineArray(*Array);
	return true;
}

// ─── Structs ────────────────────────────────────────────────────────────────────

bool UEFJsonLibrary::StructToJsonObjectGeneric(const UScriptStruct* StructType, const void* StructData, FEFJsonObject& OutObject)
{
	OutObject = FEFJsonObject();
	if (!StructType || !StructData)
	{
		return false;
	}

	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	if (!FJsonObjectConverter::UStructToJsonObject(StructType, StructData, Object, 0, 0, nullptr, EJsonObjectConversionFlags::SkipStandardizeCase))
	{
		return false;
	}

	OutObject = FEFJsonObject(Object);
	return true;
}

bool UEFJsonLibrary::JsonObjectToStructGeneric(const FEFJsonObject& JsonObject, const UScriptStruct* StructType, void* StructData)
{
	if (!JsonObject.IsValid() || !StructType || !StructData)
	{
		return false;
	}

	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.Object.ToSharedRef(), StructType, StructData);
}

DEFINE_FUNCTION(UEFJsonLibrary::execStructToJsonObject)
{
	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	const void* StructData = Stack.MostRecentPropertyAddress;

	P_GET_STRUCT_REF(FEFJsonObject, OutObject);
	P_FINISH;

	if (!StructProperty || !StructData)
	{
		const FBlueprintExceptionInfo ExceptionInfo(EBlueprintExceptionType::AccessViolation,
			LOCTEXT("StructToJsonObject_NoStruct", "Struct To Json Object needs a struct connected to its Struct pin."));
		FBlueprintCoreDelegates::ThrowScriptException(Context, Stack, ExceptionInfo);
		*static_cast<bool*>(RESULT_PARAM) = false;
		return;
	}

	P_NATIVE_BEGIN;
	*static_cast<bool*>(RESULT_PARAM) = StructToJsonObjectGeneric(StructProperty->Struct, StructData, OutObject);
	P_NATIVE_END;
}

DEFINE_FUNCTION(UEFJsonLibrary::execJsonObjectToStruct)
{
	P_GET_STRUCT_REF(FEFJsonObject, JsonObject);

	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* StructData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	if (!StructProperty || !StructData)
	{
		const FBlueprintExceptionInfo ExceptionInfo(EBlueprintExceptionType::AccessViolation,
			LOCTEXT("JsonObjectToStruct_NoStruct", "Json Object To Struct needs a struct variable connected to its Out Struct pin."));
		FBlueprintCoreDelegates::ThrowScriptException(Context, Stack, ExceptionInfo);
		*static_cast<bool*>(RESULT_PARAM) = false;
		return;
	}

	P_NATIVE_BEGIN;
	*static_cast<bool*>(RESULT_PARAM) = JsonObjectToStructGeneric(JsonObject, StructProperty->Struct, StructData);
	P_NATIVE_END;
}

// ─── Deprecated ─────────────────────────────────────────────────────────────────

FEFJsonObject UEFJsonLibrary::ReadJsonFile(EFProjectDirectory DirectoryType, FString FileDirectory, bool& Success, bool DebugDirectory)
{
	const FString Source = UEFFileLibrary::GetProjectDirectory(DirectoryType) + FileDirectory;
	if (DebugDirectory)
	{
		UE_LOG(LogExtendedFramework, Display, TEXT("ReadJsonFile: %s"), *Source);
	}

	FEFJsonObject Result;
	Success = LoadJsonObjectFromFile(FPaths::ConvertRelativePathToFull(Source), Result);
	return Result;
}

FString UEFJsonLibrary::ReadExtendedJsonAsString(EFProjectDirectory DirectoryType, FString FileDirectory, bool& Success)
{
	FString Text;
	Success = FFileHelper::LoadFileToString(Text, *(UEFFileLibrary::GetProjectDirectory(DirectoryType) + FileDirectory));
	return Text;
}

void UEFJsonLibrary::GetExtendedStringField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, FString& OutString)
{
	Success = TryGetStringField(ExtendedJson, FieldName, OutString);
}

void UEFJsonLibrary::GetExtendedBoolField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, bool& OutBool)
{
	Success = TryGetBoolField(ExtendedJson, FieldName, OutBool);
}

void UEFJsonLibrary::GetExtendedNumberField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, int32& OutNumber)
{
	int64 Value = 0;
	Success = TryGetIntegerField(ExtendedJson, FieldName, Value);
	OutNumber = Success ? static_cast<int32>(FMath::Clamp<int64>(Value, MIN_int32, MAX_int32)) : -1;
}

void UEFJsonLibrary::GetExtendedFloatField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, float& OutFloat)
{
	double Value = 0.0;
	Success = TryGetNumberField(ExtendedJson, FieldName, Value);
	OutFloat = Success ? static_cast<float>(Value) : -1.f;
}

void UEFJsonLibrary::GetExtendedObjectField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, FEFJsonObject& OutObjectField)
{
	Success = TryGetObjectField(ExtendedJson, FieldName, OutObjectField);
}

void UEFJsonLibrary::GetExtendedArrayField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, TArray<FEFJsonObject>& OutObjectField)
{
	// The old node returned the object elements and reported success only for a non-empty result.
	OutObjectField.Reset();
	TArray<FEFJsonValue> Elements;
	if (TryGetArrayField(ExtendedJson, FieldName, Elements))
	{
		for (const FEFJsonValue& Element : Elements)
		{
			FEFJsonObject ElementObject;
			if (TryGetObjectValue(Element, ElementObject))
			{
				OutObjectField.Add(MoveTemp(ElementObject));
			}
		}
	}
	Success = OutObjectField.Num() > 0;
}

void UEFJsonLibrary::GetExtendedJsonValueKeys(bool& Success, const FEFJsonObject& ExtendedJson, TArray<FString>& GeneratedKeyArray)
{
	GeneratedKeyArray = GetFieldNames(ExtendedJson);
	Success = GeneratedKeyArray.Num() > 0;
}

#undef LOCTEXT_NAMESPACE
