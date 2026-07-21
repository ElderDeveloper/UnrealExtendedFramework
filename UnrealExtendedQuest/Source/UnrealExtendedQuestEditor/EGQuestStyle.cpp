// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestStyle.h"

#include "Styling/SlateStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/ToolBarStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"

#include "EGQuestPluginEditorModule.h"
#include "UnrealExtendedQuest/EGQuestPluginModule.h"

// Const icon sizes
static const FVector2D Icon16x16(16.0f, 16.0f);
static const FVector2D Icon20x20(20.0f, 20.0f);
static const FVector2D Icon24x24(24.0f, 24.0f);
static const FVector2D Icon40x40(40.0f, 40.0f);
static const FVector2D Icon64x64(64.0f, 64.0f);
static const FVector2D Icon96x96(96.0f, 96.0f);


// What is displayed in the Content Browser
const FName FEGQuestStyle::PROPERTY_QuestGraphClassIcon(TEXT("ClassIcon.QuestGraph"));
const FName FEGQuestStyle::PROPERTY_QuestGraphClassThumbnail(TEXT("ClassThumbnail.QuestGraph"));
const FName FEGQuestStyle::PROPERTY_QuestEventCustomClassIcon(TEXT("ClassIcon.QuestEventCustom"));
const FName FEGQuestStyle::PROPERTY_QuestEventCustomClassThumbnail(TEXT("ClassThumbnail.QuestEventCustom"));
const FName FEGQuestStyle::PROPERTY_QuestObjectiveClassIcon(TEXT("ClassIcon.EGQuestNode_Objective"));
const FName FEGQuestStyle::PROPERTY_QuestObjectiveClassThumbnail(TEXT("ClassThumbnail.EGQuestNode_Objective"));
const FName FEGQuestStyle::PROPERTY_QuestTextArgumentCustomClassIcon(TEXT("ClassIcon.QuestTextArgumentCustom"));
const FName FEGQuestStyle::PROPERTY_QuestTextArgumentCustomClassThumbnail(TEXT("ClassThumbnail.QuestTextArgumentCustom"));


// Inside the Quest Editor Window
const FName FEGQuestStyle::PROPERTY_GraphNodeCircleBox(TEXT("QuestPluginEditor.Graph.Node.Circle"));
const FName FEGQuestStyle::PROPERTY_EventIcon(TEXT("QuestPluginEditor.Event"));

// Tied with FEGQuestCommands::QuestReloadData
const FName FEGQuestStyle::PROPERTY_ReloadAssetIcon(TEXT("QuestPluginEditor.QuestReloadData"));

// For the Quest Browser
const FName FEGQuestStyle::PROPERTY_OpenAssetIcon(TEXT("QuestPluginEditor.OpenAsset"));
const FName FEGQuestStyle::PROPERTY_FindAssetIcon(TEXT("QuestPluginEditor.FindAsset"));
const FName FEGQuestStyle::PROPERTY_QuestBrowser_TabIcon(TEXT("QuestPluginEditor.QuestBrowser.TabIcon"));

// Tied with FEGQuestCommands::SaveAllQuests
const FName FEGQuestStyle::PROPERTY_SaveAllQuestsIcon(TEXT("QuestPluginEditor.SaveAllQuests"));

// Tied with FEGQuestCommands::DeleteAllQuestsTextFiles
const FName FEGQuestStyle::PROPERTY_DeleteAllQuestsTextFilesIcon(TEXT("QuestPluginEditor.DeleteAllQuestsTextFiles"));

// Tied with FEGQuestCommands::DeleteCurrentQuestTextFiles
const FName FEGQuestStyle::PROPERTY_DeleteCurrentQuestTextFilesIcon(TEXT("QuestPluginEditor.DeleteCurrentQuestTextFiles"));

// For the Quest Search Browser
const FName FEGQuestStyle::PROPERTY_QuestSearch_TabIcon(TEXT("QuestPluginEditor.QuestSearch.TabIcon"));

// For the Quest Data Display Window
const FName FEGQuestStyle::PROPERTY_QuestDataDisplay_TabIcon(TEXT("QuestPluginEditor.QuestDataDisplay.TabIcon"));

// Tied with FEGQuestCommands::FindInQuest
const FName FEGQuestStyle::PROPERTY_FindInQuestEditorIcon(TEXT("QuestPluginEditor.FindInQuest"));

// Tied with FEGQuestCommands::FindInAllQuests
const FName FEGQuestStyle::PROPERTY_FindInAllQuestEditorIcon(TEXT("QuestPluginEditor.FindInAllQuests"));

// For FEGQuestSearchResult_CommentNode
const FName FEGQuestStyle::PROPERTY_CommentBubbleOn(TEXT("QuestPluginEditor.CommentBubbleOn"));

// The private ones
TSharedPtr<FSlateStyleSet> FEGQuestStyle::StyleSet = nullptr;
FString FEGQuestStyle::EngineContentRoot = FString();
FString FEGQuestStyle::PluginContentRoot = FString();

void FEGQuestStyle::Initialize()
{
	// Only register once
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShared<FSlateStyleSet>(GetStyleSetName());
	EngineContentRoot = FPaths::EngineContentDir() / TEXT("Editor/Slate");
	TSharedPtr<IPlugin> CurrentPlugin = IPluginManager::Get().FindPlugin(QUEST_SYSTEM_HOST_PLUGIN_NAME.ToString());
	if (CurrentPlugin.IsValid())
	{
		// Replaces the Engine Content Root (Engine/Editor/Slate) with the plugin content root
		StyleSet->SetContentRoot(CurrentPlugin->GetContentDir());
		PluginContentRoot = CurrentPlugin->GetContentDir();
	}
	else
	{
		UE_LOG(LogEGQuestPluginEditor, Fatal, TEXT("Could not find the Quest System Plugin :("));
		return;
	}

	// Content Browser icons for asset types
	StyleSet->Set(
		PROPERTY_QuestGraphClassIcon,
		new FSlateImageBrush(GetPluginContentPath("Icons/QuestGraph_16x.png"), Icon16x16)
	);
	StyleSet->Set(
		PROPERTY_QuestGraphClassThumbnail,
		new FSlateImageBrush(GetPluginContentPath("Icons/QuestGraph_64x.png"), Icon64x64)
	);
	StyleSet->Set(
		PROPERTY_QuestEventCustomClassIcon,
		new FSlateImageBrush(GetPluginContentPath("Icons/QuestEventCustom_16x.png"), Icon16x16)
	);
	StyleSet->Set(
		PROPERTY_QuestEventCustomClassThumbnail,
		new FSlateImageBrush(GetPluginContentPath("Icons/QuestEventCustom_64x.png"), Icon64x64)
	);
	StyleSet->Set(
		PROPERTY_QuestObjectiveClassIcon,
		new FSlateImageBrush(GetPluginContentPath("Icons/QuestConditionCustom_16x.png"), Icon16x16)
	);
	StyleSet->Set(
		PROPERTY_QuestObjectiveClassThumbnail,
		new FSlateImageBrush(GetPluginContentPath("Icons/QuestConditionCustom_64x.png"), Icon64x64)
	);
	StyleSet->Set(
		PROPERTY_QuestTextArgumentCustomClassIcon,
		new FSlateImageBrush(GetPluginContentPath("Icons/QuestTextArgumentCustom_16x.png"), Icon16x16)
	);
	StyleSet->Set(
		PROPERTY_QuestTextArgumentCustomClassThumbnail,
		new FSlateImageBrush(GetPluginContentPath("Icons/QuestTextArgumentCustom_64x.png"), Icon64x64)
	);

	// Quest Search
	StyleSet->Set(
		PROPERTY_FindInQuestEditorIcon,
		new FSlateImageBrush(GetEngineContentPath("Icons/icon_Blueprint_Find_40px.png"), Icon40x40)
	);
	StyleSet->Set(
		GetSmallProperty(PROPERTY_FindInQuestEditorIcon),
		new FSlateImageBrush(GetEngineContentPath("Icons/icon_Blueprint_Find_40px.png"), Icon20x20)
	);

	StyleSet->Set(
		PROPERTY_FindInAllQuestEditorIcon,
		new FSlateImageBrush(GetEngineContentPath("Icons/icon_FindInAnyBlueprint_40px.png"), Icon40x40)
	);
	StyleSet->Set(
		GetSmallProperty(PROPERTY_FindInAllQuestEditorIcon),
		new FSlateImageBrush(GetEngineContentPath("Icons/icon_FindInAnyBlueprint_40px.png"), Icon20x20)
	);

	StyleSet->Set(
		PROPERTY_QuestSearch_TabIcon,
		new FSlateImageBrush(GetEngineContentPath("Icons/icon_Genericfinder_16x.png"), Icon16x16)
	);

	// Level Editor File
	StyleSet->Set(
		PROPERTY_SaveAllQuestsIcon,
		new FSlateImageBrush(GetEngineContentPath("Icons/icon_file_saveall_40x.png"), Icon40x40)
	);
	StyleSet->Set(
		PROPERTY_DeleteAllQuestsTextFilesIcon,
		new FSlateImageBrush(GetEngineContentPath("Icons/Edit/icon_Edit_Delete_40x.png"), Icon40x40)
	);
	StyleSet->Set(
		PROPERTY_DeleteCurrentQuestTextFilesIcon,
		new FSlateImageBrush(GetEngineContentPath("Icons/Edit/icon_Edit_Delete_40x.png"), Icon40x40)
	);

	// Quest Browser
	StyleSet->Set(
		PROPERTY_QuestBrowser_TabIcon,
		new FSlateImageBrush(GetEngineContentPath("Icons/icon_tab_ContentBrowser_16x.png"), Icon16x16)
	);

	// Quest Data Display
	StyleSet->Set(
		PROPERTY_QuestDataDisplay_TabIcon,
		new FSlateImageBrush(GetPluginContentPath("Icons/DebugTools_40x.png"), Icon16x16)
	);

	// Quest Editor Window
	StyleSet->Set(
		PROPERTY_ReloadAssetIcon,
		new FSlateImageBrush(GetEngineContentPath("Icons/icon_Cascade_RestartInLevel_40x.png"), Icon40x40)
	);
	StyleSet->Set(
		GetSmallProperty(PROPERTY_ReloadAssetIcon),
		new FSlateImageBrush(GetEngineContentPath("Icons/icon_Refresh_16x.png"), Icon16x16)
	);

	StyleSet->Set(
		PROPERTY_EventIcon,
		new FSlateImageBrush(GetPluginContentPath("Icons/Event_96x.png"), Icon96x96)
	);
	StyleSet->Set(
		PROPERTY_OpenAssetIcon,
		new FSlateImageBrush(GetEngineContentPath("Icons/icon_asset_open_16px.png"), Icon16x16)
	);
	StyleSet->Set(
		PROPERTY_FindAssetIcon,
		new FSlateImageBrush(GetEngineContentPath("Icons/icon_Genericfinder_16x.png"), Icon16x16)
	);
	StyleSet->Set(
		PROPERTY_GraphNodeCircleBox,
		new FSlateBoxBrush(
			GetEngineContentPath("BehaviorTree/IndexCircle.png"),
			Icon20x20,
			FMargin(8.0f / 20.0f)
		)
	);
	StyleSet->Set(
		PROPERTY_CommentBubbleOn,
		new FSlateImageBrush(
			GetEngineContentPath("Icons/icon_Blueprint_CommentBubbleOn_16x.png"),
			Icon16x16,
			FLinearColor(1.f, 1.f, 1.f, 1.f)
		)
	);

	// Register the current style
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}

FName FEGQuestStyle::GetGreenToolBarStyle(const ISlateStyle* SourceStyleSet, FName SourceStyleName)
{
	static const FName GreenStyleName(TEXT("QuestPluginEditor.ToolBar.Green"));
	if (!StyleSet.IsValid() || SourceStyleSet == nullptr)
	{
		return SourceStyleName;
	}
	if (StyleSet->HasWidgetStyle<FToolBarStyle>(GreenStyleName))
	{
		return GreenStyleName;
	}
	// The toolbar the caller is building decides the shape and metrics; we only repaint the button,
	// so a style we cannot read leaves the button ordinary rather than unstyled.
	if (!SourceStyleSet->HasWidgetStyle<FToolBarStyle>(SourceStyleName))
	{
		return SourceStyleName;
	}

	static constexpr float CornerRadius = 4.0f;
	const FLinearColor Normal{0.058824f, 0.345098f, 0.101961f, 1.0f};
	const FLinearColor Hovered{0.101961f, 0.505882f, 0.164706f, 1.0f};
	const FLinearColor Pressed{0.031373f, 0.239216f, 0.070588f, 1.0f};

	FToolBarStyle GreenStyle = SourceStyleSet->GetWidgetStyle<FToolBarStyle>(SourceStyleName);
	GreenStyle.ButtonStyle.SetNormal(FSlateRoundedBoxBrush(Normal, CornerRadius));
	GreenStyle.ButtonStyle.SetHovered(FSlateRoundedBoxBrush(Hovered, CornerRadius));
	GreenStyle.ButtonStyle.SetPressed(FSlateRoundedBoxBrush(Pressed, CornerRadius));
	StyleSet->Set(GreenStyleName, GreenStyle);
	return GreenStyleName;
}

void FEGQuestStyle::Shutdown()
{
	// unregister the style
	if (!StyleSet.IsValid())
	{
		return;
	}

	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
	ensure(StyleSet.IsUnique());
	StyleSet.Reset();
}
