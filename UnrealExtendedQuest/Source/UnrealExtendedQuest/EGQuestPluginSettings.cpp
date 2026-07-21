// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestPluginSettings.h"

#include "GameFramework/Character.h"
#include "Runtime/Launch/Resources/Version.h"

#include "EGQuestManager.h"
#include "Logging/EGQuestLogger.h"

#define LOCTEXT_NAMESPACE "QuestPlugin"

//////////////////////////////////////////////////////////////////////////
// UEGQuestPluginSettings
UEGQuestPluginSettings::UEGQuestPluginSettings()
{
	BlacklistedReflectionClasses = {AActor::StaticClass(), APawn::StaticClass(),  ACharacter::StaticClass()};
	// AdditionalTextFormatFileExtensionsToLookFor = {""};
}

#if WITH_EDITOR
FText UEGQuestPluginSettings::GetSectionText() const
{
	return LOCTEXT("SectionText", "Quest");
}

FText UEGQuestPluginSettings::GetSectionDescription() const
{
	return LOCTEXT("SectionDescription", "Configure how the Quest Editor behaves + Runtime behaviour");
}


bool UEGQuestPluginSettings::CanEditChange(const FProperty* InProperty) const
{
	const bool bIsEditable = Super::CanEditChange(InProperty);
	if (bIsEditable && InProperty)
	{
		const FName PropertyName = InProperty->GetFName();

		// Only useful for GlobalNamespace
		if (QuestTextNamespaceLocalization != EEGQuestTextNamespaceLocalization::Global &&
			PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, QuestTextGlobalNamespaceName))
		{
			return false;
		}

		// Only useful for WithPrefixPerQuest
		if (QuestTextNamespaceLocalization != EEGQuestTextNamespaceLocalization::WithPrefixPerQuest &&
			PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, QuestTextPrefixNamespaceName))
		{
			return false;
		}

		if (QuestTextNamespaceLocalization == EEGQuestTextNamespaceLocalization::Ignore &&
			(PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, LocalizationIgnoredStrings)))
		{
			return false;
		}

	}

	return bIsEditable;
}

void UEGQuestPluginSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property != nullptr ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, bEnableMessageLog))
	{
		// Prevent no logging at all
		bEnableOutputLog = !bEnableMessageLog;
	}

	// Check category
	if (PropertyChangedEvent.Property != nullptr && PropertyChangedEvent.Property->HasMetaData(TEXT("Category")))
	{
		const FString& Category = PropertyChangedEvent.Property->GetMetaData(TEXT("Category"));

		// Sync logger settings
		if (Category.Equals(TEXT("Logger"), ESearchCase::IgnoreCase))
		{
			FEGQuestLogger::Get().SyncWithSettings();
		}
	}
}
#endif // WITH_EDITOR

bool UEGQuestPluginSettings::IsIgnoredTextForLocalization(const FText& Text) const
{
	// Ignored texts
	// for (const FText& Ignored : LocalizationIgnoredTexts)
	// {
	// 	if (Text.EqualTo(Ignored))
	// 	{
	// 		return false;
	// 	}
	// }

	// Ignore strings
	const FString& TextSourceString = *FTextInspector::GetSourceString(Text);
	if (LocalizationIgnoredStrings.Contains(TextSourceString))
	{
		return false;
	}
	// for (const FString& Ignored : LocalizationIgnoredStrings)
	// {
	// 	if (TextSourceString.Equals(Ignored))
	// 	{
	// 		return false;
	// 	}
	// }

	return true;
}

FString UEGQuestPluginSettings::GetTextFileExtension(EEGQuestGraphTextFormat TextFormat)
{
	switch (TextFormat)
	{
		// JSON has the .json added at the end
		case EEGQuestGraphTextFormat::JSON:
			return TEXT(".quest.json");

		// Empty
		case EEGQuestGraphTextFormat::None:
		default:
			return FString();
	}
}

const TSet<FString>& UEGQuestPluginSettings::GetAllCurrentTextFileExtensions()
{
	static TSet<FString> Extensions;
	if (Extensions.Num() == 0)
	{
		// Iterate over all possible text formats
		const int32 TextFormatsNum = static_cast<int32>(EEGQuestGraphTextFormat::NumTextFormats);
		for (int32 TextFormatIndex = static_cast<int32>(EEGQuestGraphTextFormat::StartTextFormats);
				   TextFormatIndex < TextFormatsNum; TextFormatIndex++)
		{
			const EEGQuestGraphTextFormat CurrentTextFormat = static_cast<EEGQuestGraphTextFormat>(TextFormatIndex);
			Extensions.Add(GetTextFileExtension(CurrentTextFormat));
		}
	}

	return Extensions;
}

TSet<FString> UEGQuestPluginSettings::GetAllTextFileExtensions() const
{
	TSet<FString> CurrentFileExtensions = GetAllCurrentTextFileExtensions();

	// Look for additional file extensions
	for (const FString& Ext : AdditionalTextFormatFileExtensionsToLookFor)
	{
		// Only allow file extension that start with dot, also ignore uasset
		if (Ext.StartsWith(".") && Ext != ".uasset")
		{
			CurrentFileExtensions.Add(Ext);
		}
	}

	return CurrentFileExtensions;
}

#undef LOCTEXT_NAMESPACE
