// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestGraphAssetTypeActions.h"

#include "UnrealExtendedQuestEditor/Editor/EGQuestEditor.h"
#include "UnrealExtendedQuestEditor/EGQuestPluginEditorModule.h"

void FEGQuestGraphAssetTypeActions::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	// Implement the menu actions here
}

void FEGQuestGraphAssetTypeActions::OpenAssetEditor(const TArray<UObject*>& InObjects,
												TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (UObject* Object : InObjects)
	{
		// Only handle quests
		if (UEGQuestGraph* Quest = Cast<UEGQuestGraph>(Object))
		{
			UE_LOG(LogEGQuestPluginEditor, Log, TEXT("Clicked a Quest = `%s`"), *Quest->GetPathName());

			TSharedRef<FEGQuestEditor> NewQuestEditor(new FEGQuestEditor());
 			NewQuestEditor->InitQuestEditor(Mode, EditWithinLevelEditor, Quest);

			// Default Editor
//			FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, Quest);
		}
	}
}
