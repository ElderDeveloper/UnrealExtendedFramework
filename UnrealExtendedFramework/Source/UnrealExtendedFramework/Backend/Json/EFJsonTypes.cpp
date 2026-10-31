// Copyright Moon Punch Games. All Rights Reserved.

#include "Backend/Json/EFJsonTypes.h"

#include "Policies/CondensedJsonPrintPolicy.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace EFJsonPrivate
{
	template <class PrintPolicy>
	FString SerializeObject(const TSharedRef<FJsonObject>& Object)
	{
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, PrintPolicy>> Writer = TJsonWriterFactory<TCHAR, PrintPolicy>::Create(&Out);
		FJsonSerializer::Serialize(Object, Writer);
		return Out;
	}

	template <class PrintPolicy>
	FString SerializeArray(const TArray<TSharedPtr<FJsonValue>>& Array)
	{
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, PrintPolicy>> Writer = TJsonWriterFactory<TCHAR, PrintPolicy>::Create(&Out);
		FJsonSerializer::Serialize(Array, Writer);
		return Out;
	}

	/** Array elements must never be null pointers: the serializer expects a value in every slot. */
	TArray<TSharedPtr<FJsonValue>> SanitizeArray(const TArray<TSharedPtr<FJsonValue>>& Array)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		Out.Reserve(Array.Num());
		for (const TSharedPtr<FJsonValue>& Element : Array)
		{
			Out.Add(Element.IsValid() ? Element : TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
		}
		return Out;
	}

	bool ParseUnsignedIndex(const FString& Text, int32& OutIndex)
	{
		if (Text.IsEmpty() || Text.Len() > 9)
		{
			return false;
		}
		for (const TCHAR Char : Text)
		{
			if (!FChar::IsDigit(Char))
			{
				return false;
			}
		}
		OutIndex = FCString::Atoi(*Text);
		return true;
	}
}

EEFJsonType FEFJsonValue::GetType() const
{
	return EFJson::ToType(Value);
}

EEFJsonType EFJson::ToType(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return EEFJsonType::None;
	}

	switch (Value->Type)
	{
	case EJson::Null:    return EEFJsonType::Null;
	case EJson::String:  return EEFJsonType::String;
	case EJson::Number:  return EEFJsonType::Number;
	case EJson::Boolean: return EEFJsonType::Boolean;
	case EJson::Array:   return EEFJsonType::Array;
	case EJson::Object:  return EEFJsonType::Object;
	case EJson::None:
	default:             return EEFJsonType::None;
	}
}

TSharedPtr<FJsonValue> EFJson::DeepCopyValue(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return nullptr;
	}

	switch (Value->Type)
	{
	case EJson::Array:
	{
		TArray<TSharedPtr<FJsonValue>> Copy;
		const TArray<TSharedPtr<FJsonValue>>* Source = nullptr;
		if (Value->TryGetArray(Source) && Source)
		{
			Copy.Reserve(Source->Num());
			for (const TSharedPtr<FJsonValue>& Element : *Source)
			{
				Copy.Add(DeepCopyValue(Element));
			}
		}
		return MakeShared<FJsonValueArray>(EFJsonPrivate::SanitizeArray(Copy));
	}
	case EJson::Object:
	{
		const TSharedPtr<FJsonObject>* Source = nullptr;
		if (Value->TryGetObject(Source) && Source && Source->IsValid())
		{
			return MakeShared<FJsonValueObject>(DeepCopyObject(*Source));
		}
		return MakeShared<FJsonValueNull>();
	}
	default:
		// Strings, numbers, booleans and null have no mutators, so sharing them is safe.
		return Value;
	}
}

TSharedPtr<FJsonObject> EFJson::DeepCopyObject(const TSharedPtr<FJsonObject>& Object)
{
	if (!Object.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Copy = MakeShared<FJsonObject>();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
	{
		Copy->SetField(Pair.Key, Pair.Value.IsValid() ? DeepCopyValue(Pair.Value) : TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
	}
	return Copy;
}

bool EFJson::ResolvePath(const TSharedPtr<FJsonObject>& Root, const FString& Path, TSharedPtr<FJsonValue>& OutValue)
{
	OutValue.Reset();
	if (!Root.IsValid())
	{
		return false;
	}

	TSharedPtr<FJsonValue> Current = MakeShared<FJsonValueObject>(Root);
	const int32 Length = Path.Len();
	int32 Cursor = 0;

	while (Cursor < Length)
	{
		if (Path[Cursor] == TEXT('['))
		{
			const int32 Close = Path.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
			int32 Index = 0;
			if (Close == INDEX_NONE || !EFJsonPrivate::ParseUnsignedIndex(Path.Mid(Cursor + 1, Close - Cursor - 1), Index))
			{
				return false;
			}

			const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
			if (Current->Type != EJson::Array || !Current->TryGetArray(Array) || !Array || !Array->IsValidIndex(Index))
			{
				return false;
			}

			Current = (*Array)[Index];
			Cursor = Close + 1;
		}
		else
		{
			int32 End = Cursor;
			while (End < Length && Path[End] != TEXT('.') && Path[End] != TEXT('['))
			{
				++End;
			}

			const FString Key = Path.Mid(Cursor, End - Cursor);
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (Key.IsEmpty() || Current->Type != EJson::Object || !Current->TryGetObject(Object) || !Object || !Object->IsValid())
			{
				return false;
			}

			const TSharedPtr<FJsonValue>* Found = (*Object)->Values.Find(Key);
			if (!Found)
			{
				return false;
			}

			Current = *Found;
			Cursor = End;
		}

		if (!Current.IsValid())
		{
			return false;
		}

		// A dot separates segments; it may not end the path or follow another dot.
		if (Cursor < Length && Path[Cursor] == TEXT('.'))
		{
			++Cursor;
			if (Cursor >= Length || Path[Cursor] == TEXT('.') || Path[Cursor] == TEXT('['))
			{
				return false;
			}
		}
	}

	OutValue = Current;
	return true;
}

bool EFJson::ParseObject(const FString& JsonText, TSharedPtr<FJsonObject>& OutObject)
{
	OutObject.Reset();

	TSharedPtr<FJsonObject> Parsed;
	const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
	{
		return false;
	}

	OutObject = Parsed;
	return true;
}

bool EFJson::ParseValue(const FString& JsonText, TSharedPtr<FJsonValue>& OutValue)
{
	OutValue.Reset();

	const FString Trimmed = JsonText.TrimStartAndEnd();
	if (Trimmed.StartsWith(TEXT("{")))
	{
		TSharedPtr<FJsonObject> Object;
		if (!ParseObject(Trimmed, Object))
		{
			return false;
		}
		OutValue = MakeShared<FJsonValueObject>(Object);
		return true;
	}

	if (Trimmed.StartsWith(TEXT("[")))
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Trimmed);
		if (!FJsonSerializer::Deserialize(Reader, Array))
		{
			return false;
		}
		OutValue = MakeShared<FJsonValueArray>(EFJsonPrivate::SanitizeArray(Array));
		return true;
	}

	return false;
}

FString EFJson::ObjectToString(const TSharedPtr<FJsonObject>& Object, bool bPrettyPrint)
{
	if (!Object.IsValid())
	{
		return FString();
	}

	return bPrettyPrint
		? EFJsonPrivate::SerializeObject<TPrettyJsonPrintPolicy<TCHAR>>(Object.ToSharedRef())
		: EFJsonPrivate::SerializeObject<TCondensedJsonPrintPolicy<TCHAR>>(Object.ToSharedRef());
}

FString EFJson::ValueToString(const TSharedPtr<FJsonValue>& Value, bool bPrettyPrint)
{
	if (!Value.IsValid())
	{
		return FString();
	}

	switch (Value->Type)
	{
	case EJson::Object:
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		return Value->TryGetObject(Object) && Object ? ObjectToString(*Object, bPrettyPrint) : FString(TEXT("null"));
	}
	case EJson::Array:
	{
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (!Value->TryGetArray(Array) || !Array)
		{
			return TEXT("[]");
		}
		const TArray<TSharedPtr<FJsonValue>> Sanitized = EFJsonPrivate::SanitizeArray(*Array);
		return bPrettyPrint
			? EFJsonPrivate::SerializeArray<TPrettyJsonPrintPolicy<TCHAR>>(Sanitized)
			: EFJsonPrivate::SerializeArray<TCondensedJsonPrintPolicy<TCHAR>>(Sanitized);
	}
	default:
	{
		// The writer only accepts objects and arrays at the root, so write the primitive as the
		// single element of an array and strip the brackets. This reuses the engine's escaping
		// and number formatting rather than duplicating them.
		const FString Wrapped = EFJsonPrivate::SerializeArray<TCondensedJsonPrintPolicy<TCHAR>>({ Value });
		return Wrapped.Len() >= 2 ? Wrapped.Mid(1, Wrapped.Len() - 2) : FString();
	}
	}
}
