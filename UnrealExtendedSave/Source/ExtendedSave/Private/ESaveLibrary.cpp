// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESaveLibrary.h"

#include "ESaveSubsystem.h"

TArray<FESaveSlotInfo> UESaveLibrary::GetSaveSlots(const UObject* WorldContextObject)
{
	const UESaveSubsystem* Save = UESaveSubsystem::Get(WorldContextObject);
	return Save ? Save->GetSlots() : TArray<FESaveSlotInfo>();
}

bool UESaveLibrary::DoesSaveSlotExist(const UObject* WorldContextObject, const FString& SlotName)
{
	const UESaveSubsystem* Save = UESaveSubsystem::Get(WorldContextObject);
	return Save && Save->DoesSlotExist(SlotName);
}

bool UESaveLibrary::DeleteSaveSlot(const UObject* WorldContextObject, const FString& SlotName)
{
	UESaveSubsystem* Save = UESaveSubsystem::Get(WorldContextObject);
	return Save && Save->DeleteSlot(SlotName);
}

void UESaveLibrary::StartNewSaveGame(const UObject* WorldContextObject)
{
	if (UESaveSubsystem* Save = UESaveSubsystem::Get(WorldContextObject))
	{
		Save->StartNewGame();
	}
}

void UESaveLibrary::RequestAutosave(const UObject* WorldContextObject)
{
	if (UESaveSubsystem* Save = UESaveSubsystem::Get(WorldContextObject))
	{
		Save->RequestAutosave();
	}
}

void UESaveLibrary::SetAutosaveBlocked(const UObject* WorldContextObject, FName Reason, bool bBlocked)
{
	if (UESaveSubsystem* Save = UESaveSubsystem::Get(WorldContextObject))
	{
		Save->SetAutosaveBlocked(Reason, bBlocked);
	}
}

void UESaveLibrary::RegisterSaveable(const UObject* WorldContextObject, UObject* Object)
{
	if (UESaveSubsystem* Save = UESaveSubsystem::Get(WorldContextObject))
	{
		Save->RegisterSaveable(Object);
	}
}

void UESaveLibrary::SetSaveString(const UObject* WorldContextObject, FName Key, const FString& Value)
{
	if (UESaveSubsystem* Save = UESaveSubsystem::Get(WorldContextObject))
	{
		Save->SetValue(Key, Value);
	}
}

FString UESaveLibrary::GetSaveString(const UObject* WorldContextObject, FName Key, const FString& Default)
{
	FString Value;
	const UESaveSubsystem* Save = UESaveSubsystem::Get(WorldContextObject);
	return Save && Save->GetValue(Key, Value) ? Value : Default;
}

void UESaveLibrary::SetSaveInt(const UObject* WorldContextObject, FName Key, int32 Value)
{
	SetSaveString(WorldContextObject, Key, LexToString(Value));
}

int32 UESaveLibrary::GetSaveInt(const UObject* WorldContextObject, FName Key, int32 Default)
{
	FString Text;
	const UESaveSubsystem* Save = UESaveSubsystem::Get(WorldContextObject);
	if (!Save || !Save->GetValue(Key, Text))
	{
		return Default;
	}
	int32 Value = Default;
	LexFromString(Value, *Text);
	return Value;
}

void UESaveLibrary::SetSaveFloat(const UObject* WorldContextObject, FName Key, float Value)
{
	SetSaveString(WorldContextObject, Key, LexToString(Value));
}

float UESaveLibrary::GetSaveFloat(const UObject* WorldContextObject, FName Key, float Default)
{
	FString Text;
	const UESaveSubsystem* Save = UESaveSubsystem::Get(WorldContextObject);
	if (!Save || !Save->GetValue(Key, Text))
	{
		return Default;
	}
	float Value = Default;
	LexFromString(Value, *Text);
	return Value;
}

void UESaveLibrary::SetSaveBool(const UObject* WorldContextObject, FName Key, bool Value)
{
	SetSaveString(WorldContextObject, Key, Value ? TEXT("true") : TEXT("false"));
}

bool UESaveLibrary::GetSaveBool(const UObject* WorldContextObject, FName Key, bool Default)
{
	FString Text;
	const UESaveSubsystem* Save = UESaveSubsystem::Get(WorldContextObject);
	if (!Save || !Save->GetValue(Key, Text))
	{
		return Default;
	}
	return Text.ToBool();
}

bool UESaveLibrary::HasSaveValue(const UObject* WorldContextObject, FName Key)
{
	FString Ignored;
	const UESaveSubsystem* Save = UESaveSubsystem::Get(WorldContextObject);
	return Save && Save->GetValue(Key, Ignored);
}
