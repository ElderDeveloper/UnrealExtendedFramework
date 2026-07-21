// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Styling/SlateStyle.h"

// how everything looks, fancy stuff
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestStyle
{
public:
	static void Initialize();

	static void Shutdown();

	static TSharedPtr<ISlateStyle> Get() { return StyleSet; }

	/** Gets the style name. */
	static FName GetStyleSetName() { return TEXT("QuestPluginStyle"); }

	/**
	 * A toolbar style identical to the one given, but with a green button - for Compile, the action
	 * the editor exists for. Derived from the caller's own style rather than a hardcoded engine one,
	 * so the button keeps its neighbours' shape and metrics and only the colour differs.
	 *
	 * Returns SourceStyleName unchanged if the style could not be derived, so the caller degrades to
	 * an ordinary button instead of an unstyled one.
	 */
	static FName GetGreenToolBarStyle(const ISlateStyle* SourceStyleSet, FName SourceStyleName);

	/** Gets the small property name variant */
	static FName GetSmallProperty(FName PropertyName)
	{
		return FName(*(PropertyName.ToString() + TEXT(".Small")));
	}

	/** Get the RelativePath to the QuestPlugin Content Dir */
	static FString GetPluginContentPath(const FString& RelativePath)
	{
		return PluginContentRoot / RelativePath;
	}

	/** Get the RelativePath to the Engine Content Dir */
	static FString GetEngineContentPath(const FString& RelativePath)
	{
		return EngineContentRoot / RelativePath;
	}

public:
	static const FName PROPERTY_QuestGraphClassIcon;
	static const FName PROPERTY_QuestGraphClassThumbnail;

	static const FName PROPERTY_QuestEventCustomClassIcon;
	static const FName PROPERTY_QuestEventCustomClassThumbnail;

	static const FName PROPERTY_QuestObjectiveClassIcon;
	static const FName PROPERTY_QuestObjectiveClassThumbnail;

	static const FName PROPERTY_QuestTextArgumentCustomClassIcon;
	static const FName PROPERTY_QuestTextArgumentCustomClassThumbnail;

	static const FName PROPERTY_GraphNodeCircleBox;
	static const FName PROPERTY_EventIcon;

	static const FName PROPERTY_ReloadAssetIcon;
	static const FName PROPERTY_OpenAssetIcon;
	static const FName PROPERTY_FindAssetIcon;

	static const FName PROPERTY_SaveAllQuestsIcon;
	static const FName PROPERTY_DeleteAllQuestsTextFilesIcon;
	static const FName PROPERTY_DeleteCurrentQuestTextFilesIcon;
	static const FName PROPERTY_QuestSearch_TabIcon;
	static const FName PROPERTY_QuestBrowser_TabIcon;
	static const FName PROPERTY_QuestDataDisplay_TabIcon;

	static const FName PROPERTY_FindInQuestEditorIcon;
	static const FName PROPERTY_FindInAllQuestEditorIcon;

	static const FName PROPERTY_CommentBubbleOn;

private:
	/** Singleton instance. */
	static TSharedPtr<FSlateStyleSet> StyleSet;

	/** Engine content root. */
	static FString EngineContentRoot;

	/** QuestPlugin Content Root */
	static FString PluginContentRoot;
};
