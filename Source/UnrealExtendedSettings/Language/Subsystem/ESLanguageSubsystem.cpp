// Fill out your copyright notice in the Description page of Project Settings.


#include "ESLanguageSubsystem.h"

#include "UnrealExtendedFramework/Libraries/File/EFFileLibrary.h"

FString UESLanguageSubsystem::GetCurrentLanguageFileDirection()
{
	const auto Tag = LanguageFiles.FindChecked(CurrentLanguageTag);

	if (Tag.FilePath.Len() != 0)
	{
		return UEFFileLibrary::GetProjectDirectory(Tag.LanguageAssetProjectDirectory) + Tag.FilePath;
	}
	return "-1";
}
