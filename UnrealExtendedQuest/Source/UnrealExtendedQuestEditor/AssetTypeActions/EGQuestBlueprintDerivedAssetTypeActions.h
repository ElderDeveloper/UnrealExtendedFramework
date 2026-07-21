// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "AssetTypeActions_Base.h"
#include "UnrealExtendedQuest/EGQuestConstants.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Objective.h"
#include "UnrealExtendedQuest/EGQuestEventCustom.h"

#include "UnrealExtendedQuest/EGQuestTextArgumentCustom.h"
#include "UnrealExtendedQuestEditor/EGQuestEditorUtilities.h"
#include "UnrealExtendedQuest/EGQuestHelper.h"

class IToolkitHost;

/**
 * See FEGQuestPluginEditorModule::StartupModule for usage.
 * NOTE: all of these are Blueprints but we derrive it here so that it appears nicer in the content browser
 */
class FEGQuestBlueprintDerivedAssetTypeActions : public FAssetTypeActions_Base
{
public:
	//
	// IAssetTypeActions interface
	//
	FEGQuestBlueprintDerivedAssetTypeActions(EAssetTypeCategories::Type InAssetCategory) : AssetCategory(InAssetCategory) {}

	// Same Color as the Blueprints
	FColor GetTypeColor() const override { return FColor(63, 126, 255); }
	bool HasActions(const TArray<UObject*>& InObjects) const override { return false; }
	uint32 GetCategories() override { return AssetCategory; }
	const TArray<FText>& GetSubMenus() const override
	{
		static const TArray<FText> SubMenus{QUEST_SYSTEM_MENU_SUBCATEGORY_TEXT};
		return SubMenus;
	}
	bool CanFilter() override { return true; }
	void BuildBackendFilter(FARFilter& InFilter) override
	{
		FilterAddNativeParentClassPath(InFilter, GetSupportedClass());

		// Add to filter all native children classes of our supported class
		TArray<UClass*> NativeChildClasses;
		TArray<UClass*> BlueprintChildClasses;
		FEGQuestHelper::GetAllChildClassesOf(GetSupportedClass(), NativeChildClasses, BlueprintChildClasses);
		for (const UClass* ChildNativeClass : NativeChildClasses)
		{
			FilterAddNativeParentClassPath(InFilter, ChildNativeClass);
		}

#if NY_ENGINE_VERSION >= 501
		InFilter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
#else
		InFilter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());
#endif

		InFilter.bRecursiveClasses = true;
	}

	static void FilterAddNativeParentClassPath(FARFilter& InFilter, const UClass* Class)
	{
		if (Class == nullptr)
		{
			return;
		}

		const FString Value = FString::Printf(
			TEXT("%s'%s'"),
			*UClass::StaticClass()->GetName(),
			*Class->GetPathName()
		);
		InFilter.TagsAndValues.Add(FBlueprintTags::NativeParentClassPath, Value);
	}

protected:
	EAssetTypeCategories::Type AssetCategory;
};


class FAssetTypeActions_QuestEventCustom : public FEGQuestBlueprintDerivedAssetTypeActions
{
public:
	//
	// IAssetTypeActions interface
	//
	FAssetTypeActions_QuestEventCustom(EAssetTypeCategories::Type InAssetCategory) : FEGQuestBlueprintDerivedAssetTypeActions(InAssetCategory) {}

	FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "QuestEventCustomAssetTypeActions", "Quest Custom Event Blueprint"); }
	UClass* GetSupportedClass() const override { return UEGQuestEventCustom::StaticClass(); }
};


class FAssetTypeActions_QuestObjective : public FEGQuestBlueprintDerivedAssetTypeActions
{
public:
	//
	// IAssetTypeActions interface
	//
	FAssetTypeActions_QuestObjective(EAssetTypeCategories::Type InAssetCategory) : FEGQuestBlueprintDerivedAssetTypeActions(InAssetCategory) {}

	FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "QuestObjectiveAssetTypeActions", "Quest Objective Blueprint"); }
	UClass* GetSupportedClass() const override { return UEGQuestNode_Objective::StaticClass(); }
};


class FAssetTypeActions_QuestTextArgumentCustom : public FEGQuestBlueprintDerivedAssetTypeActions
{
public:
	//
	// IAssetTypeActions interface
	//
	FAssetTypeActions_QuestTextArgumentCustom(EAssetTypeCategories::Type InAssetCategory) : FEGQuestBlueprintDerivedAssetTypeActions(InAssetCategory) {}

	FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "QuestTextArgumentCustomAssetTypeActions", "Quest Custom Text Argument Blueprint"); }
	UClass* GetSupportedClass() const override { return UEGQuestTextArgumentCustom::StaticClass(); }
};
