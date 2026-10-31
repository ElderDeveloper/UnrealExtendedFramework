// Copyright Moon Punch Games. All Rights Reserved.

#include "Backend/EFLegacyJsonLibrary.h"

#include "Json/EFJsonLibrary.h"
#include "Libraries/File/EFFileLibrary.h"
#include "Libraries/File/EFFileLibraryPrivate.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealExtendedFramework.h"

FEFJsonObject UEFLegacyJsonLibrary::ReadJsonFile(EFProjectDirectory DirectoryType, FString FileDirectory, bool& Success, bool DebugDirectory)
{
	const FString Source = UEFFileLibrary::GetProjectDirectory(DirectoryType) + FileDirectory;
	if (DebugDirectory)
	{
		UE_LOG(LogExtendedFramework, Display, TEXT("ReadJsonFile: %s"), *Source);
	}

	FEFJsonObject Result;
	if (!EFFileLibraryAccess::CheckRead(TEXT("UEFLegacyJsonLibrary::ReadJsonFile"), Source))
	{
		Success = false;
		return Result;
	}
	Success = UEFJsonLibrary::LoadJsonObjectFromFile(FPaths::ConvertRelativePathToFull(Source), Result);
	return Result;
}

FString UEFLegacyJsonLibrary::ReadExtendedJsonAsString(EFProjectDirectory DirectoryType, FString FileDirectory, bool& Success)
{
	FString Text;
	const FString Source = UEFFileLibrary::GetProjectDirectory(DirectoryType) + FileDirectory;
	if (!EFFileLibraryAccess::CheckRead(TEXT("UEFLegacyJsonLibrary::ReadExtendedJsonAsString"), Source))
	{
		Success = false;
		return Text;
	}
	Success = FFileHelper::LoadFileToString(Text, *Source);
	return Text;
}

void UEFLegacyJsonLibrary::GetExtendedStringField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, FString& OutString)
{
	Success = UEFJsonLibrary::TryGetStringField(ExtendedJson, FieldName, OutString);
}

void UEFLegacyJsonLibrary::GetExtendedBoolField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, bool& OutBool)
{
	Success = UEFJsonLibrary::TryGetBoolField(ExtendedJson, FieldName, OutBool);
}

void UEFLegacyJsonLibrary::GetExtendedNumberField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, int32& OutNumber)
{
	int64 Value = 0;
	Success = UEFJsonLibrary::TryGetIntegerField(ExtendedJson, FieldName, Value);
	OutNumber = Success ? static_cast<int32>(FMath::Clamp<int64>(Value, MIN_int32, MAX_int32)) : -1;
}

void UEFLegacyJsonLibrary::GetExtendedFloatField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, float& OutFloat)
{
	double Value = 0.0;
	Success = UEFJsonLibrary::TryGetNumberField(ExtendedJson, FieldName, Value);
	OutFloat = Success ? static_cast<float>(Value) : -1.f;
}

void UEFLegacyJsonLibrary::GetExtendedObjectField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, FEFJsonObject& OutObjectField)
{
	Success = UEFJsonLibrary::TryGetObjectField(ExtendedJson, FieldName, OutObjectField);
}

void UEFLegacyJsonLibrary::GetExtendedArrayField(bool& Success, const FEFJsonObject& ExtendedJson, const FString& FieldName, TArray<FEFJsonObject>& OutObjectField)
{
	// The old node returned the object elements and reported success only for a non-empty result.
	OutObjectField.Reset();
	TArray<FEFJsonValue> Elements;
	if (UEFJsonLibrary::TryGetArrayField(ExtendedJson, FieldName, Elements))
	{
		for (const FEFJsonValue& Element : Elements)
		{
			FEFJsonObject ElementObject;
			if (UEFJsonLibrary::TryGetObjectValue(Element, ElementObject))
			{
				OutObjectField.Add(MoveTemp(ElementObject));
			}
		}
	}
	Success = OutObjectField.Num() > 0;
}

void UEFLegacyJsonLibrary::GetExtendedJsonValueKeys(bool& Success, const FEFJsonObject& ExtendedJson, TArray<FString>& GeneratedKeyArray)
{
	GeneratedKeyArray = UEFJsonLibrary::GetFieldNames(ExtendedJson);
	Success = GeneratedKeyArray.Num() > 0;
}
