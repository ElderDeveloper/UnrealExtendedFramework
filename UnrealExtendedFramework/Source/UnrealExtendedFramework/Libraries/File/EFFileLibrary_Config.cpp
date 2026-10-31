// Copyright Moon Punch Games. All Rights Reserved.

// UEFFileLibrary: Config (.ini) reading, writing and key removal.

#include "EFFileLibrary.h"

#include "EFFileLibraryPrivate.h"
#include "Math/Color.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UnrealType.h"

bool UEFFileLibrary::WriteConfigFile(FString Filename, FString Section, FString Key, FProperty* Type, void* Value,
                                     bool SingleLineArray)
{
	if (!EFFileLibraryAccess::CheckConfigFile(TEXT("UEFFileLibrary::WriteConfig"), Filename, true))
	{
		return false;
	}
	if (!GConfig)
	{
		return false;
	}
	if (CastField<FBoolProperty>(Type))
	{
		GConfig->SetBool(*Section, *Key, *(static_cast<bool*>(Value)), Filename);
	}
	else if (CastField<FIntProperty>(Type))
	{
		GConfig->SetInt(*Section, *Key, *(static_cast<int32*>(Value)), Filename);
	}
	else if (CastField<FStrProperty>(Type))
	{
		GConfig->SetString(*Section, *Key, **(static_cast<FString*>(Value)), Filename);
	}
	else if (CastField<FFloatProperty>(Type))
	{
		GConfig->SetFloat(*Section, *Key, *(static_cast<float*>(Value)), Filename);
	}
	else if (CastField<FDoubleProperty>(Type))
	{
		GConfig->SetDouble(*Section, *Key, *(static_cast<double*>(Value)), Filename);
	}
	else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Type))
	{
		if (CastField<FStrProperty>(ArrayProperty->Inner))
		{
			TArray<FString>* Arr = (static_cast<TArray<FString>*>(Value));
			if (SingleLineArray)
			{
				GConfig->SetSingleLineArray(*Section, *Key, *Arr, Filename);
			}
			else
			{
				GConfig->SetArray(*Section, *Key, *Arr, Filename);
			}
		}
		else
		{
			return false;
		}
	}
	/* else if (FTextProperty* TextProperty = CastField<FTextProperty>(Type))
	{
		GConfig->SetText(*Section, *Key, *(static_cast<FText*>(Value)), Filename);
	} */
	else if (FStructProperty* StructProperty = CastField<FStructProperty>(Type))
	{
		if (StructProperty->Struct->IsNative())
		{
			FName TypeName = StructProperty->Struct->GetFName();
			if (TypeName == "Rotator")
			{
				GConfig->SetRotator(*Section, *Key, *(static_cast<FRotator*>(Value)), *Filename);
			}
			else if (TypeName == "Vector")
			{
				GConfig->SetVector(*Section, *Key, *(static_cast<FVector*>(Value)), *Filename);
			}
			else if (TypeName == "LinearColor")
			{
				GConfig->SetColor(*Section, *Key, *(static_cast<FColor*>(Value)), *Filename);
			}
			else if (TypeName == "Vector4")
			{
				GConfig->SetVector4(*Section, *Key, *(static_cast<FVector4*>(Value)), *Filename);
			}
			else if (TypeName == "Vector2D")
			{
				GConfig->SetVector2D(*Section, *Key, *(static_cast<FVector2D*>(Value)), *Filename);
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
	GConfig->Flush(false, Filename);
	return true;
}

bool UEFFileLibrary::ReadConfigFile(FString Filename, FString Section, FString Key, FProperty* Type, void* Value,
                                    bool SingleLineArray)
{
	if (!EFFileLibraryAccess::CheckConfigFile(TEXT("UEFFileLibrary::ReadConfig"), Filename, false))
	{
		return false;
	}
	if (!GConfig)
	{
		return false;
	}
	bool Success = false;
	if (CastField<FBoolProperty>(Type))
	{
		Success = GConfig->GetBool(*Section, *Key, *(static_cast<bool*>(Value)), Filename);
	}
	else if (CastField<FIntProperty>(Type))
	{
		Success = GConfig->GetInt(*Section, *Key, *(static_cast<int32*>(Value)), Filename);
	}
	else if (CastField<FStrProperty>(Type))
	{
		Success = GConfig->GetString(*Section, *Key, *(static_cast<FString*>(Value)), Filename);
	}
	else if (CastField<FFloatProperty>(Type))
	{
		Success = GConfig->GetFloat(*Section, *Key, *(static_cast<float*>(Value)), Filename);
	}
	else if (CastField<FDoubleProperty>(Type))
	{
		Success = GConfig->GetDouble(*Section, *Key, *(static_cast<double*>(Value)), Filename);
	}
	else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Type))
	{
		if (CastField<FStrProperty>(ArrayProperty->Inner))
		{
			TArray<FString>* Arr = (static_cast<TArray<FString>*>(Value));
			if (SingleLineArray)
			{
				Success = (GConfig->GetSingleLineArray(*Section, *Key, *Arr, Filename) != 0);
			}
			else
			{
				Success = (GConfig->GetArray(*Section, *Key, *Arr, Filename) != 0);
			}
		}
	}
	/* else if (FTextProperty* TextProperty = CastField<FTextProperty>(Type))
	{
		Success = GConfig->GetText(*Section, *Key, *(static_cast<FText*>(Value)), Filename);
	}*/
	else if (FStructProperty* StructProperty = CastField<FStructProperty>(Type))
	{
		if (StructProperty->Struct->IsNative())
		{
			FName TypeName = StructProperty->Struct->GetFName();
			if (TypeName == "Rotator")
			{
				Success = GConfig->GetRotator(*Section, *Key, *(static_cast<FRotator*>(Value)), *Filename);
			}
			else if (TypeName == "Vector")
			{
				Success = GConfig->GetVector(*Section, *Key, *(static_cast<FVector*>(Value)), *Filename);
			}
			else if (TypeName == "LinearColor")
			{
				Success = GConfig->GetColor(*Section, *Key, *(static_cast<FColor*>(Value)), *Filename);
			}
			else if (TypeName == "Vector4")
			{
				Success = GConfig->GetVector4(*Section, *Key, *(static_cast<FVector4*>(Value)), *Filename);
			}
			else if (TypeName == "Vector2D")
			{
				Success = GConfig->GetVector2D(*Section, *Key, *(static_cast<FVector2D*>(Value)), *Filename);
			}
		}
	}
	return Success;
}

bool UEFFileLibrary::RemoveConfig(FString FilePath, FString Section, FString Key)
{
	// Removing a key marks the file dirty, and GConfig writes it back on its next flush.
	if (!EFFileLibraryAccess::CheckConfigFile(TEXT("UEFFileLibrary::RemoveConfig"), FilePath, true))
	{
		return false;
	}
	if (!GConfig)
	{
		return false;
	}
	return GConfig->RemoveKey(*Section, *Key, *FilePath);
}
