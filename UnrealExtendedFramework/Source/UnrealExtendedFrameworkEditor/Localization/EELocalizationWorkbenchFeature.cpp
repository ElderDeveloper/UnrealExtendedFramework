// Copyright Moon Punch Games. All Rights Reserved.

#include "EELocalizationWorkbenchFeature.h"

#if WITH_EDITOR

#include "Framework/Docking/TabManager.h"
#include "Glossary/EEGlossaryValidation.h"
#include "Styling/AppStyle.h"
#include "UI/SEELocalizationWorkbenchPanel.h"
#include "Validation/EELocalizationValidator.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "EELocalizationWorkbenchFeature"

const FName FEELocalizationWorkbenchFeature::TabId(TEXT("ExtendedLocalizationWorkbench"));

void FEELocalizationWorkbenchFeature::Register(const TSharedRef<FWorkspaceItem>& InGroup)
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabId, FOnSpawnTab::CreateRaw(this, &FEELocalizationWorkbenchFeature::SpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Localization Workbench"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Discover, edit, review, and validate localized text on top of the standard Unreal localization pipeline."))
		.SetGroup(InGroup)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LocalizationDashboard.MenuIcon"));

	// LW-5: the glossary rule joins the built-in validation rules.
	FEELocValidator::Get().RegisterRule(MakeShared<FEEGlossaryValidationRule>());

	bRegistered = true;
}

void FEELocalizationWorkbenchFeature::Unregister()
{
	if (bRegistered)
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);
		bRegistered = false;
	}
}

TSharedRef<SDockTab> FEELocalizationWorkbenchFeature::SpawnTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(NomadTab)
		[
			SNew(SEELocalizationWorkbenchPanel)
		];
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
