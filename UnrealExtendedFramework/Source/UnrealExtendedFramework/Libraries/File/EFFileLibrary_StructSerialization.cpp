// Copyright Moon Punch Games. All Rights Reserved.

// UEFFileLibrary: Wildcard struct conversion to and from JSON and XML (the CustomThunk nodes).

#include "EFFileLibrary.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "JsonObjectConverter.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "XmlFile.h"

bool UEFFileLibrary::JsonStringToAnyStruct(FProperty* Property, void* ValuePtr, const FString& Json)
{
	bool Success = false;
	if (!Property || !ValuePtr || Json.IsEmpty())
	{
		return Success;
	}
	TSharedPtr<FJsonValue> JsonValue;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Json);
	if (FJsonSerializer::Deserialize(JsonReader, JsonValue) && JsonValue.IsValid())
	{
		TSharedRef<FJsonValue> InJsonValue = UEFFileLibrary::JsonValueToAnyStruct(Property, JsonValue);
		if ((InJsonValue->Type == EJson::Array && InJsonValue->AsArray().Num() != 0) || (InJsonValue->Type ==
			EJson::Object && InJsonValue->AsObject()->Values.Num() != 0))
		{
			Success = UEFFileLibrary::JsonValueToAnyStruct(InJsonValue, Property, ValuePtr);
		}
	}
	return Success;
}

TSharedRef<FJsonValue> UEFFileLibrary::JsonValueToAnyStruct(FProperty* Property, TSharedPtr<FJsonValue> Value)
{
	if (!Value.IsValid() || Value->IsNull() || Property == NULL)
	{
		return MakeShareable(new FJsonValueNull());
	}
	// array
	else if (FArrayProperty* arrayProperty = CastField<FArrayProperty>(Property))
	{
		TArray<TSharedPtr<FJsonValue>> OutArray;
		if (Value->Type == EJson::Array)
		{
			auto InArray = Value->AsArray();
			for (int32 ArrayIndex = 0; ArrayIndex < InArray.Num(); ArrayIndex++)
			{
				OutArray.Add(UEFFileLibrary::JsonValueToAnyStruct(arrayProperty->Inner, InArray[ArrayIndex]));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "JsonValueToAnyStruct: error while converting JsonValue to array, type %i is invalid with array property, returning empty array"
			       ), Value->Type);
		}
		return MakeShareable(new FJsonValueArray(OutArray));
	}
	// set
	else if (FSetProperty* setProperty = CastField<FSetProperty>(Property))
	{
		TArray<TSharedPtr<FJsonValue>> OutSet;
		if (Value->Type == EJson::Array)
		{
			auto InSet = Value->AsArray();
			for (int32 ArrayIndex = 0; ArrayIndex < InSet.Num(); ArrayIndex++)
			{
				OutSet.Add(UEFFileLibrary::JsonValueToAnyStruct(setProperty->ElementProp, InSet[ArrayIndex]));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "JsonValueToAnyStruct: error while converting JsonValue to set, type %i is invalid with set property, returning empty set"
			       ), Value->Type);
		}
		return MakeShareable(new FJsonValueArray(OutSet));
	}
	// map
	else if (FMapProperty* mapProperty = CastField<FMapProperty>(Property))
	{
		TSharedRef<FJsonObject> OutMap = MakeShared<FJsonObject>();
		if (Value->Type == EJson::Object)
		{
			auto InMap = Value->AsObject();
			if (InMap)
			{
				for (auto Val : InMap->Values)
				{
					OutMap->SetField(Val.Key, UEFFileLibrary::JsonValueToAnyStruct(mapProperty->ValueProp, Val.Value));
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "JsonValueToAnyStruct: error while converting JsonValue to map, type %i is invalid with map property, returning empty object"
			       ), Value->Type);
		}
		return MakeShareable(new FJsonValueObject(OutMap));
	}
	// struct
	else if (FStructProperty* structProperty = CastField<FStructProperty>(Property))
	{
		TSharedRef<FJsonObject> OutObject = MakeShared<FJsonObject>();
		if (Value->Type == EJson::Object)
		{
			auto InObject = Value->AsObject();
			if (InObject)
			{
				for (TFieldIterator<FProperty> It(structProperty->Struct); It; ++It)
				{
					FProperty* Prop = *It;
					if (InObject->HasField(Prop->GetAuthoredName()))
					{
						OutObject->SetField(Prop->GetName(),
						                    UEFFileLibrary::JsonValueToAnyStruct(
							                    Prop, InObject->TryGetField(Prop->GetAuthoredName())));
					}
					else if (InObject->HasField(Prop->GetName()))
					{
						OutObject->SetField(Prop->GetName(),
						                    UEFFileLibrary::JsonValueToAnyStruct(
							                    Prop, InObject->TryGetField(Prop->GetName())));
					}
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "JsonValueToAnyStruct: error while converting JsonValue to struct, type %i is invalid with struct property, returning empty object"
			       ), Value->Type);
		}
		return MakeShareable(new FJsonValueObject(OutObject));
	}
	// object
	else if (FObjectProperty* objectProperty = CastField<FObjectProperty>(Property))
	{
		if (objectProperty->PropertyClass->IsNative())
		{
			return Value.ToSharedRef();
		}
		if (Value->Type == EJson::Object)
		{
			auto InObject = Value->AsObject();
			if (InObject.IsValid())
			{
				TSharedRef<FJsonObject> OutObject = MakeShared<FJsonObject>();
				for (TFieldIterator<FProperty> It(objectProperty->PropertyClass); It; ++It)
				{
					FProperty* Prop = *It;
					if (InObject->HasField(Prop->GetAuthoredName()))
					{
						OutObject->SetField(Prop->GetName(),
						                    UEFFileLibrary::JsonValueToAnyStruct(
							                    Prop, InObject->TryGetField(Prop->GetAuthoredName())));
					}
					else if (InObject->HasField(Prop->GetName()))
					{
						OutObject->SetField(Prop->GetName(),
						                    UEFFileLibrary::JsonValueToAnyStruct(
							                    Prop, InObject->TryGetField(Prop->GetName())));
					}
				}
				return MakeShareable(new FJsonValueObject(OutObject));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "JsonValueToAnyStruct: error while converting JsonValue to object, type %i is invalid with object property, returning empty object"
			       ), Value->Type);
		}
		return MakeShareable(new FJsonValueNull());
	}
	// scalar
	else
	{
		return Value.ToSharedRef();
	}
}

bool UEFFileLibrary::JsonValueToAnyStruct(TSharedPtr<FJsonValue> JsonValue, FProperty* Property, void* ValuePtr)
{
	if (!JsonValue.IsValid() || Property == NULL)
	{
		return false;
	}
	// array
	else if (FArrayProperty* arrayProperty = CastField<FArrayProperty>(Property))
	{
		if (JsonValue->Type == EJson::Array)
		{
			auto JsonArray = JsonValue->AsArray();
			auto Helper = FScriptArrayHelper::CreateHelperFormInnerProperty(arrayProperty->Inner, ValuePtr);
			Helper.Resize(JsonArray.Num());
			for (int32 i = 0; i < JsonArray.Num(); i++)
			{
				if (!UEFFileLibrary::JsonValueToAnyStruct(JsonArray[i], arrayProperty->Inner, Helper.GetRawPtr(i)))
				{
					return false;
				}
			}
		}
	}
	// set
	else if (const FSetProperty* setProperty = CastField<FSetProperty>(Property))
	{
		if (JsonValue->Type == EJson::Array)
		{
			auto JsonArray = JsonValue->AsArray();
			auto Helper = FScriptSetHelper::CreateHelperFormElementProperty(setProperty->ElementProp, ValuePtr);
			for (int32 i = 0; i < JsonArray.Num(); i++)
			{
				int32 Idx = Helper.AddDefaultValue_Invalid_NeedsRehash();
				if (!UEFFileLibrary::JsonValueToAnyStruct(JsonArray[i], setProperty->ElementProp,
				                                          Helper.GetElementPtr(Idx)))
				{
					return false;
				}
			}
			Helper.Rehash();
		}
	}
	// map
	else if (const FMapProperty* mapProperty = CastField<FMapProperty>(Property))
	{
		if (JsonValue->Type == EJson::Object)
		{
			const auto JsonObject = JsonValue->AsObject();
			auto Helper = FScriptMapHelper::CreateHelperFormInnerProperties(
				mapProperty->KeyProp, mapProperty->ValueProp, ValuePtr);
			Helper.EmptyValues(JsonObject->Values.Num());

			for (const auto& Entry : JsonObject->Values)
			{
				const int32 Idx = Helper.AddDefaultValue_Invalid_NeedsRehash();
				TSharedPtr<FJsonValueString> StrKeyVal = MakeShared<FJsonValueString>(FString(Entry.Key.Len(), *Entry.Key));
				if (!UEFFileLibrary::JsonValueToAnyStruct(StrKeyVal, mapProperty->KeyProp, Helper.GetKeyPtr(Idx)) || !
					UEFFileLibrary::JsonValueToAnyStruct(Entry.Value, mapProperty->ValueProp, Helper.GetValuePtr(Idx)))
				{
					return false;
				}
			}

			Helper.Rehash();
		}
	}
	// struct
	else if (const FStructProperty* structProperty = CastField<FStructProperty>(Property))
	{
		if (JsonValue->Type == EJson::String)
		{
			return FJsonObjectConverter::JsonValueToUProperty(JsonValue, Property, ValuePtr, 0, 0);
		}
		if (JsonValue->Type == EJson::Object)
		{
			const auto JsonObject = JsonValue->AsObject();
			for (TFieldIterator<FProperty> It(structProperty->Struct); It; ++It)
			{
				FProperty* Prop = *It;
				if (JsonObject->HasField(Prop->GetName()))
				{
					void* Value = Prop->ContainerPtrToValuePtr<uint8>(ValuePtr);
					if (!UEFFileLibrary::JsonValueToAnyStruct(JsonObject->TryGetField(Prop->GetName()), Prop, Value))
					{
						return false;
					}
				}
			}
		}
	}
	// object
	else if (const FObjectProperty* objectProperty = CastField<FObjectProperty>(Property))
	{
		if (objectProperty->PropertyClass->IsNative())
		{
			return FJsonObjectConverter::JsonValueToUProperty(JsonValue, Property, ValuePtr, 0, 0);
		}
		if (JsonValue->Type == EJson::Object && ValuePtr)
		{
			UObject* PropValue = objectProperty->GetObjectPropertyValue(ValuePtr);
			if (!PropValue)
			{
				PropValue = StaticAllocateObject(objectProperty->PropertyClass, GetTransientPackage(), NAME_None,
				                                 EObjectFlags::RF_NoFlags, EInternalObjectFlags::None, false);
				(*objectProperty->PropertyClass->ClassConstructor)(
				FObjectInitializer(PropValue,objectProperty->PropertyClass->GetDefaultObject(),EObjectInitializerOptions::InitializeProperties));
				objectProperty->SetObjectPropertyValue(ValuePtr, PropValue);
			}
			const auto JsonObject = JsonValue->AsObject();
			if (!JsonObject->HasTypedField<EJson::String>(TEXT("_ClassName")))
			{
				JsonObject->SetStringField(TEXT("_ClassName"), PropValue->GetClass()->GetFName().ToString());
			}
			return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), objectProperty->PropertyClass,
			                                                 PropValue, 0, 0);
		}
	}
	// scalar
	else
	{
		return FJsonObjectConverter::JsonValueToUProperty(JsonValue, Property, ValuePtr, 0, 0);
	}
	return true;
}

bool UEFFileLibrary::AnyStructToJsonString(FProperty* Property, void* ValuePtr, FString& Json)
{
	bool Success = false;
	if (!Property || ValuePtr == nullptr)
	{
		return Success;
	}
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<
		TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json, 0);
	// convert to json
	const auto JsonValue = UEFFileLibrary::AnyStructToJsonValue(Property, ValuePtr);
	// serialize
	if (JsonValue->Type == EJson::Object)
	{
		Success = FJsonSerializer::Serialize(JsonValue->AsObject().ToSharedRef(), JsonWriter);
	}
	else if (JsonValue->Type == EJson::Array)
	{
		Success = FJsonSerializer::Serialize(JsonValue->AsArray(), JsonWriter);
	}
	JsonWriter->Close();
	return Success;
}

TSharedRef<FJsonValue> UEFFileLibrary::AnyStructToJsonValue(FProperty* Property, void* ValuePtr)
{
	if (ValuePtr == nullptr || Property == nullptr)
	{
		return MakeShareable(new FJsonValueNull());
	}
	// array
	else if (FArrayProperty* arrayProperty = CastField<FArrayProperty>(Property))
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		auto Helper = FScriptArrayHelper::CreateHelperFormInnerProperty(arrayProperty->Inner, ValuePtr);
		for (int32 ArrayIndex = 0; ArrayIndex < Helper.Num(); ArrayIndex++)
		{
			Array.Add(UEFFileLibrary::AnyStructToJsonValue(arrayProperty->Inner, Helper.GetRawPtr(ArrayIndex)));
		}
		return MakeShareable(new FJsonValueArray(Array));
	}
	// set
	else if (FSetProperty* setProperty = CastField<FSetProperty>(Property))
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		auto Helper = FScriptSetHelper::CreateHelperFormElementProperty(setProperty->ElementProp, ValuePtr);
		for (int32 ArrayIndex = 0; ArrayIndex < Helper.Num(); ++ArrayIndex)
		{
			Array.Add(UEFFileLibrary::AnyStructToJsonValue(setProperty->ElementProp, Helper.GetElementPtr(ArrayIndex)));
		}
		return MakeShareable(new FJsonValueArray(Array));
	}
	// map 
	else if (FMapProperty* mapProperty = CastField<FMapProperty>(Property))
	{
		TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
		auto Helper = FScriptMapHelper::CreateHelperFormInnerProperties(mapProperty->KeyProp, mapProperty->ValueProp,
		                                                                ValuePtr);
		for (int32 ArrayIndex = 0; ArrayIndex < Helper.Num(); ++ArrayIndex)
		{
			FString KeyStr;
			auto Key = UEFFileLibrary::AnyStructToJsonValue(mapProperty->KeyProp, Helper.GetKeyPtr(ArrayIndex));
			if (!Key->TryGetString(KeyStr))
			{
				mapProperty->KeyProp->ExportTextItem_Direct(KeyStr, Helper.GetKeyPtr(ArrayIndex), nullptr, nullptr, 0);
				if (KeyStr.IsEmpty())
				{
					UE_LOG(LogTemp, Warning,
					       TEXT(
						       "AnyStructToJsonValue : Error serializing key in map property at index %i, using empty string as key"
					       ), ArrayIndex);
				}
			}
			auto Val = UEFFileLibrary::AnyStructToJsonValue(mapProperty->ValueProp, Helper.GetValuePtr(ArrayIndex));
			JsonObject->SetField(KeyStr, Val);
		}
		return MakeShareable(new FJsonValueObject(JsonObject));
	}
	// struct
	else if (FStructProperty* structProperty = CastField<FStructProperty>(Property))
	{
		TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
		for (TFieldIterator<FProperty> It(structProperty->Struct); It; ++It)
		{
			FProperty* Prop = *It;
			JsonObject->SetField(Prop->GetAuthoredName(),
			                     UEFFileLibrary::AnyStructToJsonValue(
				                     Prop, Prop->ContainerPtrToValuePtr<void*>(ValuePtr)));
		}
		return MakeShareable(new FJsonValueObject(JsonObject));
	}
	// object
	else if (FObjectProperty* objectProperty = CastField<FObjectProperty>(Property))
	{
		void* PropValue = objectProperty->GetObjectPropertyValue(ValuePtr);
		if (PropValue == nullptr)
		{
			return MakeShareable(new FJsonValueNull());
		}
		if (objectProperty->PropertyClass->IsNative())
		{
			auto Value = FJsonObjectConverter::UPropertyToJsonValue(Property, ValuePtr, 0, 0);
			return Value.ToSharedRef();
		}
		TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
		for (TFieldIterator<FProperty> It(objectProperty->PropertyClass); It; ++It)
		{
			FProperty* Prop = *It;
			JsonObject->SetField(Prop->GetAuthoredName(),
			                     UEFFileLibrary::AnyStructToJsonValue(
				                     Prop, Prop->ContainerPtrToValuePtr<void*>(PropValue)));
		}
		return MakeShareable(new FJsonValueObject(JsonObject));
	}
	// scalar
	else
	{
		auto Value = FJsonObjectConverter::UPropertyToJsonValue(Property, ValuePtr, 0, 0);
		return Value.ToSharedRef();
	}
}

bool UEFFileLibrary::AnyStructToXmlString(FProperty* Property, void* ValuePtr, FString& Xml)
{
	if (!Property || ValuePtr == nullptr)
	{
		return false;
	}
	const auto JsonValue = UEFFileLibrary::AnyStructToJsonValue(Property, ValuePtr);
	Xml = UEFFileLibrary::AnyStructToXmlValue(Property, JsonValue, 1);
	UEFFileLibrary::CreateTagNode("root", Xml, 0, true);
	Xml = "<?xml version='1.0' encoding='UTF-8' ?>" + Xml;
	return true;
}

FString& UEFFileLibrary::CreateTagNode(FString Tag, FString& Content, int32 Depth, bool WithCR)
{
	FString Tab;
	for (int32 space = 0; space < (Depth * 2); space++)
	{
		Tab += " ";
	}
	Content = LINE_TERMINATOR + Tab + "<" + Tag + ">" + Content;
	if (WithCR)
	{
		Content += LINE_TERMINATOR + Tab;
	}
	Content += "</" + Tag + ">";
	return Content;
}

FString UEFFileLibrary::AnyStructToXmlValue(FProperty* Property, TSharedPtr<FJsonValue> Value, int32 Depth)
{
	FString Out;
	if (!Property || Value == nullptr)
	{
		return "";
	}
	else if (FArrayProperty* arrayProperty = CastField<FArrayProperty>(Property))
	{
		if (Value->Type == EJson::Array)
		{
			auto Array = Value->AsArray();
			FString Arr;
			for (int32 ArrayIndex = 0; ArrayIndex < Array.Num(); ArrayIndex++)
			{
				auto Val = Array[ArrayIndex];
				Arr = UEFFileLibrary::AnyStructToXmlValue(arrayProperty->Inner, Val, Depth + 1);
				UEFFileLibrary::CreateTagNode("value", Arr, Depth,
				                              (Val->Type == EJson::Array || Val->Type == EJson::Object));
				Out += Arr;
			}
		}
		else
		{
			// error
			return "";
		}
	}
	else if (FSetProperty* setProperty = CastField<FSetProperty>(Property))
	{
		if (Value->Type == EJson::Array)
		{
			auto Array = Value->AsArray();
			FString Arr;
			for (int32 ArrayIndex = 0; ArrayIndex < Array.Num(); ArrayIndex++)
			{
				auto Val = Array[ArrayIndex];
				Arr = UEFFileLibrary::AnyStructToXmlValue(setProperty->ElementProp, Val, Depth + 1);
				UEFFileLibrary::CreateTagNode("item", Arr, Depth,
				                              (Val->Type == EJson::Array || Val->Type == EJson::Object));
				Out += Arr;
			}
		}
		else
		{
			// error
			return "";
		}
	}
	// map
	else if (FMapProperty* mapProperty = CastField<FMapProperty>(Property))
	{
		if (Value->Type == EJson::Object)
		{
			auto Object = Value->AsObject()->Values;
			FString Arr;
			for (auto KeyVal : Object)
			{
				FString Item;
				Arr = KeyVal.Key;
				UEFFileLibrary::CreateTagNode("key", Arr, Depth + 1, false);
				Item += Arr;
				Arr = UEFFileLibrary::AnyStructToXmlValue(mapProperty->ValueProp, KeyVal.Value, Depth + 2);
				UEFFileLibrary::CreateTagNode("value", Arr, Depth + 1,
				                              (KeyVal.Value->Type == EJson::Array || KeyVal.Value->Type ==
					                              EJson::Object));
				Item += Arr;
				UEFFileLibrary::CreateTagNode("item", Item, Depth, true);
				Out += Item;
			}
		}
		else
		{
			// error
			return "";
		}
	}
	// struct
	else if (FStructProperty* structProperty = CastField<FStructProperty>(Property))
	{
		if (Value->Type == EJson::Object)
		{
			auto Object = Value->AsObject();
			FString Arr;
			for (TFieldIterator<FProperty> It(structProperty->Struct); It; ++It)
			{
				FProperty* Prop = *It;
				if (Object->HasField(Prop->GetAuthoredName()))
				{
					auto Val = Object->TryGetField(Prop->GetAuthoredName()).ToSharedRef();
					Arr = UEFFileLibrary::AnyStructToXmlValue(Prop, Val, Depth + 1);
					UEFFileLibrary::CreateTagNode(Prop->GetAuthoredName(), Arr, Depth,
					                              (Val->Type == EJson::Array || Val->Type == EJson::Object));
					Out += Arr;
				}
				else if (Object->HasField(Prop->GetName()))
				{
					auto Val = Object->TryGetField(Prop->GetName()).ToSharedRef();
					Arr = UEFFileLibrary::AnyStructToXmlValue(Prop, Val, Depth + 1);
					UEFFileLibrary::CreateTagNode(Prop->GetName(), Arr, Depth,
					                              (Val->Type == EJson::Array || Val->Type == EJson::Object));
					Out += Arr;
				}
			}
		}
		else
		{
			// error
			return "";
		}
	}
	// object
	else if (FObjectProperty* objectProperty = CastField<FObjectProperty>(Property))
	{
		FString Arr;
		if (objectProperty->PropertyClass->IsNative())
		{
			if (Value->Type == EJson::String)
			{
				Out = Value->AsString();
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AnyStructToXmlString : Error property %s type %i error"),
				       *objectProperty->GetAuthoredName(), Value->Type);
			}
		}
		else if (Value->Type == EJson::Object)
		{
			auto Object = Value->AsObject();
			for (TFieldIterator<FProperty> It(objectProperty->PropertyClass); It; ++It)
			{
				FProperty* Prop = *It;
				if (Object->HasField(Prop->GetAuthoredName()))
				{
					auto Val = Object->TryGetField(Prop->GetAuthoredName()).ToSharedRef();
					Arr = UEFFileLibrary::AnyStructToXmlValue(Prop, Val, Depth + 1);
					UEFFileLibrary::CreateTagNode(Prop->GetAuthoredName(), Arr, Depth,
					                              (Val->Type == EJson::Array || Val->Type == EJson::Object));
					Out += Arr;
				}
				else if (Object->HasField(Prop->GetName()))
				{
					auto Val = Object->TryGetField(Prop->GetName()).ToSharedRef();
					Arr = UEFFileLibrary::AnyStructToXmlValue(Prop, Val, Depth + 1);
					UEFFileLibrary::CreateTagNode(Prop->GetName(), Arr, Depth,
					                              (Val->Type == EJson::Array || Val->Type == EJson::Object));
					Out += Arr;
				}
			}
		}
		else
		{
			// error
			return "";
		}
	}
	// scalar
	else
	{
		switch (Value->Type)
		{
		case(EJson::String):
			Out = UEFFileLibrary::XmlEscapeChars(Value->AsString());
			break;
		case(EJson::Number):
			Out = UEFFileLibrary::XmlEscapeChars(FString::SanitizeFloat(Value->AsNumber()));
			break;
		case(EJson::Boolean):
			Out = UEFFileLibrary::XmlEscapeChars((Value->AsBool() ? "true" : "false"));
			break;
		case(EJson::Null):
			Out = "";
			break;
		}
	}
	return Out;
}

bool UEFFileLibrary::XmlStringToAnyStruct(FProperty* Property, void* ValuePtr, const FString& Xml)
{
	bool Success = false;
	if (!Property || !ValuePtr || Xml.IsEmpty())
	{
		return Success;
	}
	FXmlFile* File = new FXmlFile();
	if (!File->LoadFile(Xml, EConstructMethod::ConstructFromBuffer))
	{
		UE_LOG(LogTemp, Warning, TEXT("XmlStringToAnyStruct : Could not load xml buffer %s"), *(File->GetLastError()));
		File->Clear();
		return Success;
	}
	auto JsonValue = UEFFileLibrary::XmlNodeToAnyStruct(Property, File->GetRootNode());
	if ((JsonValue->Type == EJson::Array && JsonValue->AsArray().Num() != 0) || (JsonValue->Type == EJson::Object &&
		JsonValue->AsObject()->Values.Num() != 0))
	{
		Success = FJsonObjectConverter::JsonValueToUProperty(JsonValue, Property, ValuePtr, 0, 0);
	}
	File->Clear();
	delete File;
	return Success;
}

TSharedRef<FJsonValue> UEFFileLibrary::XmlNodeToAnyStruct(FProperty* Property, FXmlNode* Node)
{
	if (!Property || !Node)
	{
		return MakeShareable(new FJsonValueNull());
	}
	else if (FArrayProperty* arrayProperty = CastField<FArrayProperty>(Property))
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		auto Children = Node->GetChildrenNodes();
		for (int ArrayIndex = 0; ArrayIndex < Children.Num(); ArrayIndex++)
		{
			Array.Add(UEFFileLibrary::XmlNodeToAnyStruct(arrayProperty->Inner, Children[ArrayIndex]));
		}
		return MakeShareable(new FJsonValueArray(Array));
	}
	else if (FSetProperty* setProperty = CastField<FSetProperty>(Property))
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		auto Children = Node->GetChildrenNodes();
		for (int ArrayIndex = 0; ArrayIndex < Children.Num(); ArrayIndex++)
		{
			Array.Add(UEFFileLibrary::XmlNodeToAnyStruct(setProperty->ElementProp, Children[ArrayIndex]));
		}
		return MakeShareable(new FJsonValueArray(Array));
	}
	else if (FMapProperty* mapProperty = CastField<FMapProperty>(Property))
	{
		TSharedRef<FJsonObject> Object = MakeShareable<FJsonObject>(new FJsonObject());
		for (auto Child : Node->GetChildrenNodes())
		{
			if (auto Key = Child->FindChildNode("key"))
			{
				if (auto Val = Child->FindChildNode("value"))
				{
					Object->SetField(Key->GetContent(),
					                 UEFFileLibrary::XmlNodeToAnyStruct(mapProperty->ValueProp, Val));
				}
			}
		}
		return MakeShareable(new FJsonValueObject(Object));
	}
	else if (FStructProperty* structProperty = CastField<FStructProperty>(Property))
	{
		TSharedRef<FJsonObject> Object = MakeShareable<FJsonObject>(new FJsonObject());
		for (TFieldIterator<FProperty> It(structProperty->Struct); It; ++It)
		{
			FProperty* Prop = *It;
			FXmlNode* Result;
			if ((Result = Node->FindChildNode(Prop->GetAuthoredName())))
			{
				Object->SetField(Prop->GetName(), UEFFileLibrary::XmlNodeToAnyStruct(Prop, Result));
			}
			else if ((Result = Node->FindChildNode(Prop->GetName())))
			{
				Object->SetField(Prop->GetName(), UEFFileLibrary::XmlNodeToAnyStruct(Prop, Result));
			}
		}
		return MakeShareable(new FJsonValueObject(Object));
	}
	else if (FObjectProperty* objectProperty = CastField<FObjectProperty>(Property))
	{
		if (Node->GetContent().IsEmpty() && Node->GetChildrenNodes().Num() == 0)
		{
			return MakeShareable(new FJsonValueNull());
		}
		if (objectProperty->PropertyClass->IsNative())
		{
			return MakeShareable(new FJsonValueString(Node->GetContent()));
		}
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		for (TFieldIterator<FProperty> It(objectProperty->PropertyClass); It; ++It)
		{
			FProperty* Prop = *It;
			FXmlNode* Result;
			if ((Result = Node->FindChildNode(Prop->GetAuthoredName())))
			{
				Object->SetField(Prop->GetName(), UEFFileLibrary::XmlNodeToAnyStruct(Prop, Result));
			}
			else if ((Result = Node->FindChildNode(Prop->GetName())))
			{
				Object->SetField(Prop->GetName(), UEFFileLibrary::XmlNodeToAnyStruct(Prop, Result));
			}
		}
		return MakeShareable(new FJsonValueObject(Object));
	}
	// scalar
	else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		auto Prop = MakeShareable<FJsonValueBoolean>(
			new FJsonValueBoolean((Node->GetContent() == "true" ? true : false)));
		return Prop;
	}
	else if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
	{
		auto Prop = MakeShareable<FJsonValueNumber>(new FJsonValueNumber(FCString::Atod(*Node->GetContent())));
		return Prop;
	}
	else if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
	{
		auto Prop = MakeShareable<FJsonValueString>(
			new FJsonValueString(UEFFileLibrary::XmlConvertChars(Node->GetContent())));
		return Prop;
	}
	else
	{
		// other
		auto Prop = MakeShareable<FJsonValueString>(
			new FJsonValueString(UEFFileLibrary::XmlConvertChars(Node->GetContent())));
		return Prop;
	}
}

FString UEFFileLibrary::XmlEscapeChars(FString Source)
{
	Source.ReplaceInline(TEXT("&"), TEXT("&amp;"));
	Source.ReplaceInline(TEXT("<"), TEXT("&lt;"));
	Source.ReplaceInline(TEXT(">"), TEXT("&gt;"));
	return Source;
}

FString UEFFileLibrary::XmlConvertChars(FString Source)
{
	Source.ReplaceInline(TEXT("&gt;"), TEXT(">"));
	Source.ReplaceInline(TEXT("&lt;"), TEXT("<"));
	Source.ReplaceInline(TEXT("&amp;"), TEXT("&"));
	return Source;
}
