// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "FileHelpers.h"
#include "UnrealExtendedQuest/EGQuestGraph.h"
#include "UnrealExtendedQuest/EGQuestManager.h"


class FEGQuestCommandletHelper
{
public:
	static bool SaveAllDirtyQuests()
	{
		// Save all dirty packages
		constexpr bool bPromptUserToSave = false;
		constexpr bool bFastSave = true;
		constexpr bool bNotifyNoPackagesSaved = false;
		constexpr bool bCanBeDeclined = false;
		static TArray<UClass*> SaveContentClasses = { UEGQuestGraph::StaticClass() };
		return FEditorFileUtils::SaveDirtyContentPackages(SaveContentClasses, bPromptUserToSave, bFastSave, bNotifyNoPackagesSaved, bCanBeDeclined);
	}

	static bool SaveAllQuests()
	{
		TArray<UEGQuestGraph*> Quests = UEGQuestManager::GetAllQuestsFromMemory();
		TArray<UPackage*> PackagesToSave;
		for (UEGQuestGraph* Quest : Quests)
		{
			Quest->OnPreAssetSaved();
			Quest->MarkPackageDirty();
			PackagesToSave.Add(Quest->GetOutermost());
		}

		static constexpr bool bCheckDirty = false;
		return UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, bCheckDirty);
	}
};
