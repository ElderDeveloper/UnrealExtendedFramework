// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESaveSerialization.h"

#include "ExtendedSaveModule.h"
#include "UObject/UnrealType.h"

namespace ESaveSerializationPrivate
{
	FName MakeKey(const FProperty* Property, int32 ArrayIndex)
	{
		return Property->ArrayDim > 1 ? FName(*FString::Printf(TEXT("%s[%d]"), *Property->GetName(), ArrayIndex)) : Property->GetFName();
	}

	/** "Slots[2]" -> ("Slots", 2); "Health" -> ("Health", 0). */
	void SplitKey(const FName Key, FString& OutName, int32& OutIndex)
	{
		OutName = Key.ToString();
		OutIndex = 0;
		int32 Open = INDEX_NONE;
		if (OutName.EndsWith(TEXT("]")) && OutName.FindLastChar(TEXT('['), Open))
		{
			LexFromString(OutIndex, *OutName.Mid(Open + 1, OutName.Len() - Open - 2));
			OutName.LeftInline(Open);
		}
	}
}

bool ESaveSerialization::HasSaveGameProperties(const UClass* Class)
{
	if (!Class)
	{
		return false;
	}
	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_SaveGame))
		{
			return true;
		}
	}
	return false;
}

void ESaveSerialization::Capture(const UObject* Object, TMap<FName, FString>& OutProperties)
{
	OutProperties.Reset();
	if (!Object)
	{
		return;
	}

	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		const FProperty* Property = *It;
		if (!Property->HasAnyPropertyFlags(CPF_SaveGame))
		{
			continue;
		}
		for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ++ArrayIndex)
		{
			FString Text;
			const void* Value = Property->ContainerPtrToValuePtr<void>(Object, ArrayIndex);
			// No default to compare against: every value is written, so loading resets what changed since.
			Property->ExportTextItem_Direct(Text, Value, nullptr, const_cast<UObject*>(Object), PPF_None);
			OutProperties.Add(ESaveSerializationPrivate::MakeKey(Property, ArrayIndex), MoveTemp(Text));
		}
	}
}

int32 ESaveSerialization::Restore(UObject* Object, const TMap<FName, FString>& Properties)
{
	if (!Object)
	{
		return 0;
	}

	int32 Restored = 0;
	for (const TPair<FName, FString>& Pair : Properties)
	{
		FString PropertyName;
		int32 ArrayIndex = 0;
		ESaveSerializationPrivate::SplitKey(Pair.Key, PropertyName, ArrayIndex);

		FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), FName(*PropertyName));
		if (!Property || !Property->HasAnyPropertyFlags(CPF_SaveGame) || ArrayIndex < 0 || ArrayIndex >= Property->ArrayDim)
		{
			// Removed or no longer saved since this save was made.
			UE_LOG(LogExtendedSave, Verbose, TEXT("%s: the saved property %s no longer exists or is not SaveGame; skipped."), *GetNameSafe(Object), *Pair.Key.ToString());
			continue;
		}

		void* Value = Property->ContainerPtrToValuePtr<void>(Object, ArrayIndex);
		if (Property->ImportText_Direct(*Pair.Value, Value, Object, PPF_None) == nullptr)
		{
			UE_LOG(LogExtendedSave, Warning, TEXT("%s: the saved value of %s could not be read ('%s'); it keeps its current value."),
				*GetNameSafe(Object), *Pair.Key.ToString(), *Pair.Value.Left(64));
			continue;
		}
		++Restored;
	}
	return Restored;
}
