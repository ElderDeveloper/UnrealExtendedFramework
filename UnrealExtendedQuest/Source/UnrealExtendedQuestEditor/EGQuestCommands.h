// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Framework/Commands/Commands.h"

#include "EGQuestStyle.h"

// Add menu commands and stuff, if you want to that is
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestCommands : public TCommands<FEGQuestCommands>
{
public:
	FEGQuestCommands()
		: TCommands<FEGQuestCommands>(
			TEXT("UnrealExtendedQuestEditor"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "QuestPluginEditor", "QuestPlugin Editor"), // Localized context name for displaying
			NAME_None, // Parent
			FEGQuestStyle::Get()->GetStyleSetName() // Icon Style Set
			)
	{
	}

	//
	// TCommand<> interface
	//
	void RegisterCommands() override;

public:
	// Compiles the editor graph into the runtime quest data. Saving does this too.
	TSharedPtr<FUICommandInfo> QuestCompile;

	// Opens the Blueprint of the quest's run actor, when it has one.
	TSharedPtr<FUICommandInfo> OpenQuestRunActor;

	// Saves all the quests
	TSharedPtr<FUICommandInfo> SaveAllQuests;

	// Delete all the quests text files
	TSharedPtr<FUICommandInfo> DeleteAllQuestsTextFiles;

	// Delete all the text files for the CURRENT Quest
	TSharedPtr<FUICommandInfo> DeleteCurrentQuestTextFiles;

	// Open find in ALL Quests search window
	TSharedPtr<FUICommandInfo> FindInAllQuests;

	// Open find in current Quest tab
	TSharedPtr<FUICommandInfo> FindInQuest;
};
