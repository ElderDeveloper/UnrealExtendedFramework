// Copyright Moon Punch Games. All Rights Reserved.

#include "EEUILabFeature.h"

#if WITH_EDITOR

#include "Framework/Docking/TabManager.h"
#include "Host/SEEUILabHostPanel.h"
#include "Integration/EEUILabLocValidationHook.h"
#include "Integration/EEUILabMenuExtensions.h"
#include "Localization/Validation/EELocalizationValidator.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "EEUILabFeature"

const FName FEEUILabFeature::TabId(TEXT("ExtendedUILab"));
TWeakPtr<SEEUILabHostPanel> FEEUILabFeature::LastSpawnedPanel;

void FEEUILabFeature::Register(const TSharedRef<FWorkspaceItem>& InGroup)
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabId, FOnSpawnTab::CreateRaw(this, &FEEUILabFeature::SpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle", "UI Lab"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Host UMG widgets in controlled fixtures, route input, and inspect focus/navigation without PIE."))
		.SetGroup(InGroup)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "WidgetReflector.TabIcon"));

	// Integration hook (plan section 7): UI Lab fulfils the workbench's UI-aware validation slot.
	FEELocValidator::Get().SetUIValidationHook(MakeShared<FEEUILabLocValidationHook>());

	// UX-1/UX-2 entry points once ToolMenus is ready.
	ToolMenusStartupHandle = UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateStatic(&EEUILabMenuExtensions::Register));

	bRegistered = true;
}

void FEEUILabFeature::Unregister()
{
	if (bRegistered)
	{
		UToolMenus::UnRegisterStartupCallback(ToolMenusStartupHandle);
		EEUILabMenuExtensions::Unregister();
		FEELocValidator::Get().SetUIValidationHook(nullptr);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);
		bRegistered = false;
	}
}

TSharedPtr<SEEUILabHostPanel> FEEUILabFeature::InvokeLabPanel()
{
	// Spawning/fronting the tab refreshes LastSpawnedPanel.
	FGlobalTabmanager::Get()->TryInvokeTab(TabId);
	return LastSpawnedPanel.Pin();
}

bool FEEUILabFeature::OpenWidgetInLab(UClass* WidgetClass, const FString& PreviewCulture)
{
	if (!WidgetClass)
	{
		return false;
	}

	const TSharedPtr<SEEUILabHostPanel> Panel = InvokeLabPanel();
	if (!Panel.IsValid())
	{
		return false;
	}

	Panel->HostWidgetWithCulture(WidgetClass, PreviewCulture);
	return true;
}

bool FEEUILabFeature::OpenFixtureInLab(UEFUIFixture* Fixture)
{
	if (!Fixture)
	{
		return false;
	}

	const TSharedPtr<SEEUILabHostPanel> Panel = InvokeLabPanel();
	if (!Panel.IsValid())
	{
		return false;
	}

	Panel->ApplyFixture(Fixture);
	return true;
}

bool FEEUILabFeature::OpenScriptInLab(UEFUIInteractionScript* Script)
{
	if (!Script)
	{
		return false;
	}

	const TSharedPtr<SEEUILabHostPanel> Panel = InvokeLabPanel();
	if (!Panel.IsValid())
	{
		return false;
	}

	Panel->OpenScript(Script);
	return true;
}

TSharedRef<SDockTab> FEEUILabFeature::SpawnTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SEEUILabHostPanel> Panel = SNew(SEEUILabHostPanel);
	LastSpawnedPanel = Panel;

	return SNew(SDockTab)
		.TabRole(NomadTab)
		[
			Panel
		];
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
