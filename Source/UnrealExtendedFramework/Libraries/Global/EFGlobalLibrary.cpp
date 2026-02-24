// Fill out your copyright notice in the Description page of Project Settings.


#include "EFGlobalLibrary.h"
#include "EFGlobalSubsystem.h"


// ================================ STATIC STORAGE ================================
// BUG FIX: Previously these were file-scope static in the HEADER, causing each
// translation unit to get its own copy (ODR violation). Now they live in the .cpp
// file, ensuring a single shared instance across the entire process.

static TMap<FGameplayTag, TSubclassOf<UObject>> EFGlobalClass;
static TMap<FGameplayTag, bool> EFGlobalBool;
static TMap<FGameplayTag, uint8> EFGlobalByte;
static TMap<FGameplayTag, float> EFGlobalFloat;
static TMap<FGameplayTag, int32> EFGlobalInt;
static TMap<FGameplayTag, int64> EFGlobalInt64;
static TMap<FGameplayTag, FName> EFGlobalName;
static TMap<FGameplayTag, FString> EFGlobalString;
static TMap<FGameplayTag, FText> EFGlobalText;
static TMap<FGameplayTag, FVector> EFGlobalVector;
static TMap<FGameplayTag, FRotator> EFGlobalRotator;
static TMap<FGameplayTag, FTransform> EFGlobalTransform;


// ================================ TEMPLATE HELPERS ================================

namespace EFGlobalHelpers
{
	// BUG FIX: No longer auto-inserts default values on read — returns DefaultValue
	// without modifying the map. Previously every read of a non-existent tag would
	// permanently pollute the map with default entries.
	template<typename V>
	static V GetValue(const TMap<FGameplayTag, V>& Map, FGameplayTag Tag, const V& DefaultValue)
	{
		if (const V* Found = Map.Find(Tag))
		{
			return *Found;
		}
		return DefaultValue;
	}

	template<typename V>
	static V SetValue(TMap<FGameplayTag, V>& Map, FGameplayTag Tag, const V& Value)
	{
		Map.Emplace(Tag, Value);
		return Value;
	}

	template<typename V>
	static bool RemoveValue(TMap<FGameplayTag, V>& Map, FGameplayTag Tag)
	{
		return Map.Remove(Tag) > 0;
	}

	template<typename V>
	static bool ContainsValue(const TMap<FGameplayTag, V>& Map, FGameplayTag Tag)
	{
		return Map.Contains(Tag);
	}

	template<typename V>
	static void GetAll(const TMap<FGameplayTag, V>& Map, TArray<FGameplayTag>& Tags, TArray<V>& Values)
	{
		Tags.Reserve(Map.Num());
		Values.Reserve(Map.Num());
		for (const auto& Pair : Map)
		{
			Tags.Add(Pair.Key);
			Values.Add(Pair.Value);
		}
	}
}


// ================================ GETTERS ================================

TSubclassOf<UObject> UEFGlobalLibrary::GetGlobalClass(FGameplayTag Tag)   { return EFGlobalHelpers::GetValue(EFGlobalClass, Tag, TSubclassOf<UObject>(nullptr)); }
bool UEFGlobalLibrary::GetGlobalBool(FGameplayTag Tag)                    { return EFGlobalHelpers::GetValue(EFGlobalBool, Tag, false); }
uint8 UEFGlobalLibrary::GetGlobalByte(FGameplayTag Tag)                   { return EFGlobalHelpers::GetValue(EFGlobalByte, Tag, (uint8)0); }
float UEFGlobalLibrary::GetGlobalFloat(FGameplayTag Tag)                  { return EFGlobalHelpers::GetValue(EFGlobalFloat, Tag, 0.f); }
int32 UEFGlobalLibrary::GetGlobalInt(FGameplayTag Tag)                    { return EFGlobalHelpers::GetValue(EFGlobalInt, Tag, 0); }
int64 UEFGlobalLibrary::GetGlobalInt64(FGameplayTag Tag)                  { return EFGlobalHelpers::GetValue(EFGlobalInt64, Tag, (int64)0); }
FName UEFGlobalLibrary::GetGlobalName(FGameplayTag Tag)                   { return EFGlobalHelpers::GetValue(EFGlobalName, Tag, FName()); }
FString UEFGlobalLibrary::GetGlobalString(FGameplayTag Tag)               { return EFGlobalHelpers::GetValue(EFGlobalString, Tag, FString()); }
FText UEFGlobalLibrary::GetGlobalText(FGameplayTag Tag)                   { return EFGlobalHelpers::GetValue(EFGlobalText, Tag, FText()); }
FVector UEFGlobalLibrary::GetGlobalVector(FGameplayTag Tag)               { return EFGlobalHelpers::GetValue(EFGlobalVector, Tag, FVector::ZeroVector); }
FRotator UEFGlobalLibrary::GetGlobalRotator(FGameplayTag Tag)             { return EFGlobalHelpers::GetValue(EFGlobalRotator, Tag, FRotator::ZeroRotator); }
FTransform UEFGlobalLibrary::GetGlobalTransform(FGameplayTag Tag)         { return EFGlobalHelpers::GetValue(EFGlobalTransform, Tag, FTransform()); }


// ================================ SETTERS ================================

TSubclassOf<UObject> UEFGlobalLibrary::SetGlobalClass(FGameplayTag Tag, TSubclassOf<UObject> Value)  { return EFGlobalHelpers::SetValue(EFGlobalClass, Tag, Value); }
bool UEFGlobalLibrary::SetGlobalBool(FGameplayTag Tag, bool Value)                                   { return EFGlobalHelpers::SetValue(EFGlobalBool, Tag, Value); }
uint8 UEFGlobalLibrary::SetGlobalByte(FGameplayTag Tag, uint8 Value)                                 { return EFGlobalHelpers::SetValue(EFGlobalByte, Tag, Value); }
float UEFGlobalLibrary::SetGlobalFloat(FGameplayTag Tag, float Value)                                { return EFGlobalHelpers::SetValue(EFGlobalFloat, Tag, Value); }
int32 UEFGlobalLibrary::SetGlobalInt(FGameplayTag Tag, int32 Value)                                  { return EFGlobalHelpers::SetValue(EFGlobalInt, Tag, Value); }
int64 UEFGlobalLibrary::SetGlobalInt64(FGameplayTag Tag, int64 Value)                                { return EFGlobalHelpers::SetValue(EFGlobalInt64, Tag, Value); }
FName UEFGlobalLibrary::SetGlobalName(FGameplayTag Tag, FName Value)                                 { return EFGlobalHelpers::SetValue(EFGlobalName, Tag, Value); }
FString UEFGlobalLibrary::SetGlobalString(FGameplayTag Tag, FString Value)                           { return EFGlobalHelpers::SetValue(EFGlobalString, Tag, Value); }
FText UEFGlobalLibrary::SetGlobalText(FGameplayTag Tag, FText Value)                                 { return EFGlobalHelpers::SetValue(EFGlobalText, Tag, Value); }
FVector UEFGlobalLibrary::SetGlobalVector(FGameplayTag Tag, FVector Value)                           { return EFGlobalHelpers::SetValue(EFGlobalVector, Tag, Value); }
FRotator UEFGlobalLibrary::SetGlobalRotator(FGameplayTag Tag, FRotator Value)                        { return EFGlobalHelpers::SetValue(EFGlobalRotator, Tag, Value); }
FTransform UEFGlobalLibrary::SetGlobalTransform(FGameplayTag Tag, FTransform Value)                  { return EFGlobalHelpers::SetValue(EFGlobalTransform, Tag, Value); }


// ================================ REMOVE ================================

bool UEFGlobalLibrary::RemoveGlobalClass(FGameplayTag Tag)     { return EFGlobalHelpers::RemoveValue(EFGlobalClass, Tag); }
bool UEFGlobalLibrary::RemoveGlobalBool(FGameplayTag Tag)      { return EFGlobalHelpers::RemoveValue(EFGlobalBool, Tag); }
bool UEFGlobalLibrary::RemoveGlobalByte(FGameplayTag Tag)      { return EFGlobalHelpers::RemoveValue(EFGlobalByte, Tag); }
bool UEFGlobalLibrary::RemoveGlobalFloat(FGameplayTag Tag)     { return EFGlobalHelpers::RemoveValue(EFGlobalFloat, Tag); }
bool UEFGlobalLibrary::RemoveGlobalInt(FGameplayTag Tag)       { return EFGlobalHelpers::RemoveValue(EFGlobalInt, Tag); }
bool UEFGlobalLibrary::RemoveGlobalInt64(FGameplayTag Tag)     { return EFGlobalHelpers::RemoveValue(EFGlobalInt64, Tag); }
bool UEFGlobalLibrary::RemoveGlobalName(FGameplayTag Tag)      { return EFGlobalHelpers::RemoveValue(EFGlobalName, Tag); }
bool UEFGlobalLibrary::RemoveGlobalString(FGameplayTag Tag)    { return EFGlobalHelpers::RemoveValue(EFGlobalString, Tag); }
bool UEFGlobalLibrary::RemoveGlobalText(FGameplayTag Tag)      { return EFGlobalHelpers::RemoveValue(EFGlobalText, Tag); }
bool UEFGlobalLibrary::RemoveGlobalVector(FGameplayTag Tag)    { return EFGlobalHelpers::RemoveValue(EFGlobalVector, Tag); }
bool UEFGlobalLibrary::RemoveGlobalRotator(FGameplayTag Tag)   { return EFGlobalHelpers::RemoveValue(EFGlobalRotator, Tag); }
bool UEFGlobalLibrary::RemoveGlobalTransform(FGameplayTag Tag) { return EFGlobalHelpers::RemoveValue(EFGlobalTransform, Tag); }


// ================================ CONTAINS ================================

bool UEFGlobalLibrary::ContainsGlobalClass(FGameplayTag Tag)     { return EFGlobalHelpers::ContainsValue(EFGlobalClass, Tag); }
bool UEFGlobalLibrary::ContainsGlobalBool(FGameplayTag Tag)      { return EFGlobalHelpers::ContainsValue(EFGlobalBool, Tag); }
bool UEFGlobalLibrary::ContainsGlobalByte(FGameplayTag Tag)      { return EFGlobalHelpers::ContainsValue(EFGlobalByte, Tag); }
bool UEFGlobalLibrary::ContainsGlobalFloat(FGameplayTag Tag)     { return EFGlobalHelpers::ContainsValue(EFGlobalFloat, Tag); }
bool UEFGlobalLibrary::ContainsGlobalInt(FGameplayTag Tag)       { return EFGlobalHelpers::ContainsValue(EFGlobalInt, Tag); }
bool UEFGlobalLibrary::ContainsGlobalInt64(FGameplayTag Tag)     { return EFGlobalHelpers::ContainsValue(EFGlobalInt64, Tag); }
bool UEFGlobalLibrary::ContainsGlobalName(FGameplayTag Tag)      { return EFGlobalHelpers::ContainsValue(EFGlobalName, Tag); }
bool UEFGlobalLibrary::ContainsGlobalString(FGameplayTag Tag)    { return EFGlobalHelpers::ContainsValue(EFGlobalString, Tag); }
bool UEFGlobalLibrary::ContainsGlobalText(FGameplayTag Tag)      { return EFGlobalHelpers::ContainsValue(EFGlobalText, Tag); }
bool UEFGlobalLibrary::ContainsGlobalVector(FGameplayTag Tag)    { return EFGlobalHelpers::ContainsValue(EFGlobalVector, Tag); }
bool UEFGlobalLibrary::ContainsGlobalRotator(FGameplayTag Tag)   { return EFGlobalHelpers::ContainsValue(EFGlobalRotator, Tag); }
bool UEFGlobalLibrary::ContainsGlobalTransform(FGameplayTag Tag) { return EFGlobalHelpers::ContainsValue(EFGlobalTransform, Tag); }


// ================================ GET ALL ================================

void UEFGlobalLibrary::GetAllGlobalClass(TArray<FGameplayTag>& Tags, TArray<TSubclassOf<UObject>>& Values)  { EFGlobalHelpers::GetAll(EFGlobalClass, Tags, Values); }
void UEFGlobalLibrary::GetAllGlobalBool(TArray<FGameplayTag>& Tags, TArray<bool>& Values)                   { EFGlobalHelpers::GetAll(EFGlobalBool, Tags, Values); }
void UEFGlobalLibrary::GetAllGlobalByte(TArray<FGameplayTag>& Tags, TArray<uint8>& Values)                  { EFGlobalHelpers::GetAll(EFGlobalByte, Tags, Values); }
void UEFGlobalLibrary::GetAllGlobalFloat(TArray<FGameplayTag>& Tags, TArray<float>& Values)                 { EFGlobalHelpers::GetAll(EFGlobalFloat, Tags, Values); }
void UEFGlobalLibrary::GetAllGlobalInt(TArray<FGameplayTag>& Tags, TArray<int32>& Values)                   { EFGlobalHelpers::GetAll(EFGlobalInt, Tags, Values); }
void UEFGlobalLibrary::GetAllGlobalInt64(TArray<FGameplayTag>& Tags, TArray<int64>& Values)                 { EFGlobalHelpers::GetAll(EFGlobalInt64, Tags, Values); }
void UEFGlobalLibrary::GetAllGlobalName(TArray<FGameplayTag>& Tags, TArray<FName>& Values)                  { EFGlobalHelpers::GetAll(EFGlobalName, Tags, Values); }
void UEFGlobalLibrary::GetAllGlobalString(TArray<FGameplayTag>& Tags, TArray<FString>& Values)              { EFGlobalHelpers::GetAll(EFGlobalString, Tags, Values); }
void UEFGlobalLibrary::GetAllGlobalText(TArray<FGameplayTag>& Tags, TArray<FText>& Values)                  { EFGlobalHelpers::GetAll(EFGlobalText, Tags, Values); }
void UEFGlobalLibrary::GetAllGlobalVector(TArray<FGameplayTag>& Tags, TArray<FVector>& Values)              { EFGlobalHelpers::GetAll(EFGlobalVector, Tags, Values); }
void UEFGlobalLibrary::GetAllGlobalRotator(TArray<FGameplayTag>& Tags, TArray<FRotator>& Values)            { EFGlobalHelpers::GetAll(EFGlobalRotator, Tags, Values); }
void UEFGlobalLibrary::GetAllGlobalTransform(TArray<FGameplayTag>& Tags, TArray<FTransform>& Values)        { EFGlobalHelpers::GetAll(EFGlobalTransform, Tags, Values); }


// ================================ ACTOR / OBJECT (via Subsystem) ================================

AActor* UEFGlobalLibrary::GetGlobalActor(const UObject* WorldContextObject, FGameplayTag Tag, bool& Valid)
{
	if (const auto Subsystem = GEngine->GetEngineSubsystem<UEFGlobalSubsystem>())
	{
		return Subsystem->GetGlobalActor(Tag, Valid);
	}
	Valid = false;
	return nullptr;
}

void UEFGlobalLibrary::SetGlobalActor(const UObject* WorldContextObject, FGameplayTag Tag, AActor* Value)
{
	if (const auto Subsystem = GEngine->GetEngineSubsystem<UEFGlobalSubsystem>())
	{
		Subsystem->SetGlobalActor(Tag, Value);
	}
}

UObject* UEFGlobalLibrary::GetGlobalObject(const UObject* WorldContextObject, FGameplayTag Tag, bool& Valid)
{
	if (const auto Subsystem = GEngine->GetEngineSubsystem<UEFGlobalSubsystem>())
	{
		return Subsystem->GetGlobalObject(Tag, Valid);
	}
	Valid = false;
	return nullptr;
}

void UEFGlobalLibrary::SetGlobalObject(const UObject* WorldContextObject, FGameplayTag Tag, UObject* Value)
{
	if (const auto Subsystem = GEngine->GetEngineSubsystem<UEFGlobalSubsystem>())
	{
		Subsystem->SetGlobalObject(Tag, Value);
	}
}
