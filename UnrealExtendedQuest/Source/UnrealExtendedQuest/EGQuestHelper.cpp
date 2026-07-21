// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestHelper.h"

#include "HAL/FileManager.h"
#include "Engine/Blueprint.h"
#include "Logging/EGQuestLogger.h"
#include "EGQuestPluginSettings.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "Framework/Docking/TabManager.h"

bool FEGQuestHelper::DeleteFile(const FString& PathName, bool bVerbose)
{
	IFileManager& FileManager = IFileManager::Get();

	// Text file does not exist, ignore
	if (!FileManager.FileExists(*PathName))
	{
		if (bVerbose)
		{
			FEGQuestLogger::Get().Debugf(TEXT("File does not exist at path = `%s`. Can't delete."), *PathName);
		}
		return false;
	}

	// Delete the text file
	if (!FileManager.Delete(*PathName))
	{
		if (bVerbose)
		{
			FEGQuestLogger::Get().Errorf(TEXT("Can't delete file at path = `%s`"), *PathName);
		}
		return false;
	}

	if (bVerbose)
	{
		FEGQuestLogger::Get().Infof(TEXT("Deleted file %s"), *PathName);
	}
	return true;
}

bool FEGQuestHelper::RenameFile(const FString& OldPathName, const FString& NewPathName, bool bOverWrite, bool bVerbose)
{
	IFileManager& FileManager = IFileManager::Get();

	//  File we want to rename does not exist anymore
	if (!FileManager.FileExists(*OldPathName))
	{
		if (bVerbose)
		{
			FEGQuestLogger::Get().Debugf(TEXT("File before rename at path = `%s` does not exist. Can't Rename."), *OldPathName);
		}
		return false;
	}

	// File at destination already exists, conflict :/
	if (!bOverWrite && FileManager.FileExists(*NewPathName))
	{
		if (bVerbose)
		{
			FEGQuestLogger::Get().Errorf(
				TEXT("File at destination (after rename) at path = `%s` already exists. Current text file at path = `%s` won't be moved/renamed."),
				*NewPathName, *OldPathName
			);
		}
		return false;
	}

	// Finally Move/Rename
	if (!FileManager.Move(/*Dest=*/ *NewPathName, /*Src=*/ *OldPathName, /*bReplace=*/ bOverWrite))
	{
		if (bVerbose)
		{
			FEGQuestLogger::Get().Errorf(TEXT("Failure to move/rename file from `%s` to `%s`"), *OldPathName, *NewPathName);
		}
		return false;
	}

	if (bVerbose)
	{
		FEGQuestLogger::Get().Infof(TEXT("Text file moved/renamed from `%s` to `%s`"), *OldPathName, *NewPathName);
	}
	return true;
}


TSharedPtr<SDockTab> FEGQuestHelper::InvokeTab(TSharedPtr<FTabManager> TabManager, const FTabId& TabID)
{
	if (!TabManager.IsValid())
	{
		return nullptr;
	}

#if NY_ENGINE_VERSION >= 426
	return TabManager->TryInvokeTab(TabID);
#else
	return TabManager->InvokeTab(TabID);
#endif
}

FString FEGQuestHelper::CleanObjectName(FString Name)
{
	Name.RemoveFromEnd(TEXT("_C"));

	// Get rid of the extension from `filename.extension` from the end of the path
	static constexpr bool bRemovePath = false;
	Name = FPaths::GetBaseFilename(Name, bRemovePath);

	return Name;
}

bool FEGQuestHelper::IsABlueprintClass(const UClass* Class)
{
	return Cast<UBlueprintGeneratedClass>(Class) != nullptr;
}

bool FEGQuestHelper::IsObjectAChildOf(const UObject* Object, const UClass* ParentClass)
{
	if (!Object || !ParentClass)
	{
		return false;
	}

	// Blueprint
	if (const UBlueprint* Blueprint = Cast<UBlueprint>(Object))
	{
		if (const UClass* GeneratedClass = Cast<UClass>(Blueprint->GeneratedClass))
		{
			return GeneratedClass->IsChildOf(ParentClass);
		}
	}

	// A class object, does this ever happen?
	if (const UClass* ClassObject = Cast<UClass>(Object))
	{
		return ClassObject->IsChildOf(ParentClass);
	}

	// All other object types
	return Object->GetClass()->IsChildOf(ParentClass);
}

bool FEGQuestHelper::GetAllChildClassesOf(const UClass* ParentClass, TArray<UClass*>& OutNativeClasses, TArray<UClass*>& OutBlueprintClasses)
{
	// Iterate over UClass, this might be heavy on performance
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* ChildClass = *It;
		if (!ChildClass->IsChildOf(ParentClass))
		{
			continue;
		}

		// It is a child of the Parent Class
		// make sure we don't include our parent class in the array
		if (ChildClass == ParentClass)
		{
			continue;
		}

		if (IsABlueprintClass(ChildClass))
		{
			// Blueprint
			OutBlueprintClasses.Add(ChildClass);
		}
		else
		{
			// Native
			OutNativeClasses.Add(ChildClass);
		}
	}

	return OutNativeClasses.Num() > 0 || OutBlueprintClasses.Num() > 0;
}

