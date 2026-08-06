// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestCommands.h"

//////////////////////////////////////////////////////////////////////////
// QuestEditorCommands
#define LOCTEXT_NAMESPACE "QuestCommands"

void FEGQuestCommands::RegisterCommands()
{
	UI_COMMAND(
		QuestCompile,
		"Compile",
		"Compiles the graph into the runtime quest data: settles node indices, bakes route order from the authored priorities and reports graph errors. Saving the asset compiles as well.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::F7)
	);

	UI_COMMAND(
		OpenQuestRunActor,
		"Open Run Actor",
		"Opens the Blueprint of this quest's run actor - the actor spawned once per quest instance, carrying its logic and per-run state.",
		EUserInterfaceActionType::Button, FInputChord()
	);

	UI_COMMAND(
		SaveAllQuests,
		"Save All Quests...",
		"Saves all quests to the disk",
		EUserInterfaceActionType::Button, FInputChord()
	);

	UI_COMMAND(
		DeleteAllQuestsTextFiles,
		"Delete All Quests Text Files...",
		"Delete all quests text files on the disk from all existing known text formats and from the Settings AdditionalTextFormatFileExtensionsToLookFor",
		EUserInterfaceActionType::Button, FInputChord()
	);

	UI_COMMAND(
		DeleteCurrentQuestTextFiles,
		"Delete Current Quest Text Files...",
		"Delete all text files of the CURRENT Quest on the disk from all existing known text formats and from the Settings AdditionalTextFormatFileExtensionsToLookFor",
		EUserInterfaceActionType::Button, FInputChord()
	);

	UI_COMMAND(
		FindInAllQuests,
		"Find in All Quests",
		"Find references to descriptions, events, condition and variables in ALL Quest",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::F)
	);

	UI_COMMAND(
		FindInQuest,
		"Find",
		"Find references to descriptions, events, condition and variables in the current Quest (use Ctrl+Shift+F to search in all Quests)",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::F)
	);
}

#undef LOCTEXT_NAMESPACE
