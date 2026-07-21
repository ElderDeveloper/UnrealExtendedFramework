// Copyright Moon Punch Games. All Rights Reserved.

#include "EEUILabMenuExtensions.h"

#if WITH_EDITOR

#include "ContentBrowserMenuContexts.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "UILab/EEUILabFeature.h"
#include "UILab/Fixture/EFUIFixture.h"
#include "UILab/Scripting/EFUIInteractionScript.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditor.h"
#include "WidgetBlueprintToolMenuContext.h"

#define LOCTEXT_NAMESPACE "EEUILabMenuExtensions"

namespace
{
	const FName MenuOwnerName(TEXT("EEUILab"));

	/** Compiles when dirty/never-compiled so the lab hosts what the designer shows. */
	void OpenBlueprintInLab(UWidgetBlueprint* Blueprint)
	{
		if (!Blueprint)
		{
			return;
		}

		if (!Blueprint->GeneratedClass || Blueprint->Status != BS_UpToDate)
		{
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
		}

		if (Blueprint->GeneratedClass)
		{
			FEEUILabFeature::OpenWidgetInLab(Blueprint->GeneratedClass, FString());
		}
	}
}

void EEUILabMenuExtensions::Register()
{
	FToolMenuOwnerScoped OwnerScoped(MenuOwnerName);
	UToolMenus* ToolMenus = UToolMenus::Get();

	// --- UX-1: UMG designer toolbar, same section as the Widget Reflector button. ---
	{
		UToolMenu* Toolbar = ToolMenus->ExtendMenu("AssetEditor.WidgetBlueprintEditor.ToolBar.DesignerName");
		FToolMenuSection& Section = Toolbar->FindOrAddSection("WidgetTools");

		FToolUIAction OpenAction;
		OpenAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& Context)
		{
			if (const UWidgetBlueprintToolMenuContext* WidgetContext = Context.FindContext<UWidgetBlueprintToolMenuContext>())
			{
				if (const TSharedPtr<FWidgetBlueprintEditor> Editor = WidgetContext->WidgetBlueprintEditor.Pin())
				{
					OpenBlueprintInLab(Editor->GetWidgetBlueprintObj());
				}
			}
		});

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			"OpenInUILab",
			FToolUIActionChoice(OpenAction),
			LOCTEXT("OpenInUILab", "UI Lab"),
			LOCTEXT("OpenInUILabTooltip", "Hosts this widget in the Extended UI Lab: emulated resolutions/cultures, gamepad input, focus visualization, and interaction tests without PIE."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "WidgetReflector.TabIcon")));
	}

	// --- UX-2: Content Browser context actions. ---
	{
		UToolMenu* Menu = ToolMenus->ExtendMenu("ContentBrowser.AssetContextMenu.WidgetBlueprint");
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		Section.AddMenuEntry(
			"OpenInUILab",
			LOCTEXT("OpenInUILabAsset", "Open in Extended UI Lab"),
			LOCTEXT("OpenInUILabAssetTooltip", "Host this widget in the Extended UI Lab without PIE."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "WidgetReflector.TabIcon"),
			FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& Context)
			{
				if (const UContentBrowserAssetContextMenuContext* CBContext = Context.FindContext<UContentBrowserAssetContextMenuContext>())
				{
					for (UWidgetBlueprint* Blueprint : CBContext->LoadSelectedObjects<UWidgetBlueprint>())
					{
						OpenBlueprintInLab(Blueprint);
						break;
					}
				}
			}));
	}

	{
		UToolMenu* Menu = ToolMenus->ExtendMenu("ContentBrowser.AssetContextMenu.EFUIFixture");
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		Section.AddMenuEntry(
			"OpenFixtureInUILab",
			LOCTEXT("OpenFixtureInUILab", "Open Setup in UI Lab"),
			LOCTEXT("OpenFixtureInUILabTooltip", "Apply this saved setup (widget, display, culture, providers) in the Extended UI Lab."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "WidgetReflector.TabIcon"),
			FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& Context)
			{
				if (const UContentBrowserAssetContextMenuContext* CBContext = Context.FindContext<UContentBrowserAssetContextMenuContext>())
				{
					for (UEFUIFixture* Fixture : CBContext->LoadSelectedObjects<UEFUIFixture>())
					{
						FEEUILabFeature::OpenFixtureInLab(Fixture);
						break;
					}
				}
			}));
	}

	{
		UToolMenu* Menu = ToolMenus->ExtendMenu("ContentBrowser.AssetContextMenu.EFUIInteractionScript");
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		Section.AddMenuEntry(
			"OpenScriptInUILab",
			LOCTEXT("OpenScriptInUILab", "Open in UI Lab Scripts"),
			LOCTEXT("OpenScriptInUILabTooltip", "Open the Extended UI Lab with this interaction script loaded in the Scripts panel."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "WidgetReflector.TabIcon"),
			FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& Context)
			{
				if (const UContentBrowserAssetContextMenuContext* CBContext = Context.FindContext<UContentBrowserAssetContextMenuContext>())
				{
					for (UEFUIInteractionScript* Script : CBContext->LoadSelectedObjects<UEFUIInteractionScript>())
					{
						FEEUILabFeature::OpenScriptInLab(Script);
						break;
					}
				}
			}));
	}
}

void EEUILabMenuExtensions::Unregister()
{
	if (UToolMenus* ToolMenus = UToolMenus::TryGet())
	{
		ToolMenus->UnregisterOwnerByName(MenuOwnerName);
	}
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
