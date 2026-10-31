// Copyright Moon Punch Games. All Rights Reserved.

#include "UnrealExtendedFramework.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "EFFoundationVersion.h"

EF_REQUIRE_FOUNDATION_VERSION(1, 0, "UnrealExtendedFramework");

DEFINE_LOG_CATEGORY(LogExtendedFramework);

#define LOCTEXT_NAMESPACE "FUnrealExtendedFrameworkModule"

void FUnrealExtendedFrameworkModule::StartupModule()
{
#if WITH_EDITOR
	InitializeGameplayTags();
#endif
}

void FUnrealExtendedFrameworkModule::ShutdownModule()
{
}

#if WITH_EDITOR
void FUnrealExtendedFrameworkModule::InitializeGameplayTags()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UnrealExtendedFramework"));
	if (!Plugin.IsValid())
	{
		return;
	}
	const FString PluginDir = Plugin->GetBaseDir();
	FString CSVPath = FPaths::Combine(PluginDir, TEXT("Source/UnrealExtendedFramework/ModularSettings/Tags/ExtendedSettingTags.csv"));

	// Read the CSV file
	FString CSVContent;
	if (!FFileHelper::LoadFileToString(CSVContent, *CSVPath))
	{
		UE_LOG(LogExtendedFramework, Warning, TEXT("Failed to read ExtendedSettingTags.csv"));
		return;
	}

	// Parse tags from CSV (skip header row)
	TArray<FString> CSVLines;
	CSVContent.ParseIntoArrayLines(CSVLines);

	TArray<FString> GameplayTags;
	for (int32 i = 1; i < CSVLines.Num(); i++) // Skip header row
	{
		TArray<FString> CSVColumns;
		CSVLines[i].ParseIntoArray(CSVColumns, TEXT(","), true);

		if (CSVColumns.Num() >= 2)
		{
			FString Tag = CSVColumns[1].TrimStartAndEnd();
			if (!Tag.IsEmpty())
			{
				GameplayTags.Add(Tag);
			}
		}
	}

	// Ensure Config/Tags directory exists
	FString TagsConfigDir = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Tags"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*TagsConfigDir))
	{
		PlatformFile.CreateDirectory(*TagsConfigDir);
	}

	// Path to ExtendedSettingsTags.ini
	FString ConfigPath = FPaths::Combine(TagsConfigDir, TEXT("ExtendedSettingsTags.ini"));

	// Check if file exists
	FString IniContent;
	bool bFileExists = PlatformFile.FileExists(*ConfigPath);

	if (bFileExists)
	{
		// Read existing content
		FFileHelper::LoadFileToString(IniContent, *ConfigPath);
	}
	else
	{
		// Create new content with metadata and section header
		IniContent = TEXT(";METADATA=(Diff=true, UseCommands=true)\n\n[/Script/GameplayTags.GameplayTagsList]\n");
	}

	// Check if our tags are already present to avoid duplicates
	TArray<FString> NewTagsToAdd;
	for (const FString& Tag : GameplayTags)
	{
		FString TagEntry = FString::Printf(TEXT("GameplayTagList=(Tag=\"%s\",DevComment=\"\")"), *Tag);
		if (!IniContent.Contains(Tag)) // Check for tag name instead of full entry
		{
			NewTagsToAdd.Add(TagEntry);
		}
	}

	// Add new tags to the content
	if (NewTagsToAdd.Num() > 0)
	{
		// Ensure we have the correct section
		if (!IniContent.Contains(TEXT("[/Script/GameplayTags.GameplayTagsList]")))
		{
			IniContent += TEXT("\n[/Script/GameplayTags.GameplayTagsList]\n");
		}

		// Add the new tags
		for (const FString& TagEntry : NewTagsToAdd)
		{
			IniContent += TagEntry + TEXT("\n");
		}

		// Write the updated content back to file
		if (FFileHelper::SaveStringToFile(IniContent, *ConfigPath))
		{
			UE_LOG(LogExtendedFramework, Log, TEXT("Successfully created/updated ExtendedSettingsTags.ini with %d new tags"), NewTagsToAdd.Num());
		}
		else
		{
			UE_LOG(LogExtendedFramework, Error, TEXT("Failed to write ExtendedSettingsTags.ini"));
		}
	}
	else
	{
		UE_LOG(LogExtendedFramework, Log, TEXT("All Extended Settings tags are already present in ExtendedSettingsTags.ini"));
	}
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealExtendedFrameworkModule, UnrealExtendedFramework)