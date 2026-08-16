// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedEOSEditorModule.h"

#include "DevAuthTool/EEOSDevAuthTool.h"
#include "DevAuthTool/EEOSPIELoginSettings.h"
#include "DevAuthTool/EEOSPIEPlan.h"
#include "Shared/EEOSLog.h"

#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/ScopedSlowTask.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "ExtendedEOSEditor"

namespace
{
	FSimpleMulticastDelegate& GetPostEngineInitDelegate()
	{
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 8, 0)
		return FCoreDelegates::GetOnPostEngineInit();
#else
		return FCoreDelegates::OnPostEngineInit;
#endif
	}
}

void FExtendedEOSEditorModule::StartupModule()
{
	PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddRaw(this, &FExtendedEOSEditorModule::HandlePreBeginPIE);

	// Populate the Project Settings status block once everything is registered. Doing it in
	// StartupModule would be too early: the reflection lookup for UOnlinePIESettings can fail
	// before OnlineSubsystemUtils' class data exists, and the panel would open showing a
	// spurious "could not read Play Credentials".
	PostEngineInitHandle = GetPostEngineInitDelegate().AddStatic(&UEEOSPIELoginSettings::RefreshStatusOnDefault);

	// ToolMenus may not be up yet during module load; this defers to its own ready callback.
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FExtendedEOSEditorModule::RegisterMenus));

	UE_LOG(LogExtendedEOS, Log, TEXT("ExtendedEOSEditor module started"));
}

void FExtendedEOSEditorModule::ShutdownModule()
{
	if (PreBeginPIEHandle.IsValid())
	{
		FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
		PreBeginPIEHandle.Reset();
	}

	if (PostEngineInitHandle.IsValid())
	{
		GetPostEngineInitDelegate().Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	UE_LOG(LogExtendedEOS, Log, TEXT("ExtendedEOSEditor module shutdown"));
}

void FExtendedEOSEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
	if (!ToolsMenu)
	{
		return;
	}

	FToolMenuSection& Section = ToolsMenu->FindOrAddSection(
		TEXT("ExtendedEOS"), LOCTEXT("ExtendedEOSSection", "Extended EOS"));

	Section.AddMenuEntry(
		TEXT("EEOSOpenDevAuthTool"),
		LOCTEXT("OpenDevAuthTool", "Open Dev Auth Tool"),
		LOCTEXT("OpenDevAuthToolTooltip",
			"Launch the EOS Developer Authentication Tool bundled with the plugin (no-op if it is already "
			"running). Add one account per PIE client there, then run \"Sync PIE Logins\"."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FExtendedEOSEditorModule::ExecuteOpenDevAuthTool)));

	Section.AddMenuEntry(
		TEXT("EEOSSyncPIELogins"),
		LOCTEXT("SyncPIELogins", "Sync PIE Logins from Dev Auth Tool"),
		LOCTEXT("SyncPIELoginsTooltip",
			"Replace the engine's Play Credentials with one Developer login per account in the tool, in "
			"order. PIE client N then signs in as account N."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FExtendedEOSEditorModule::ExecuteSyncPIELogins)));

	Section.AddMenuEntry(
		TEXT("EEOSShowPIEPlan"),
		LOCTEXT("ShowPIEPlan", "Show PIE Sign-In Plan"),
		LOCTEXT("ShowPIEPlanTooltip",
			"Print which account each PIE client will sign in as on the next Play, and anything that will "
			"stop that from happening."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FExtendedEOSEditorModule::ExecuteShowPIEPlan)));
}

void FExtendedEOSEditorModule::Notify(const FText& Message, bool bSuccess)
{
	if (bSuccess)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("%s"), *Message.ToString());
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("%s"), *Message.ToString());
	}

	FNotificationInfo Info(Message);
	Info.ExpireDuration = bSuccess ? 5.f : 12.f;
	Info.bFireAndForget = true;

	if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
	{
		Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}
}

/** One toast + one log block describing the plan. */
void FExtendedEOSEditorModule::ReportPlan(const FEEOSPIEPlan& Plan)
{
	// The full breakdown always goes to the log — that is the artifact you go back to when a
	// client came up as the wrong account.
	UE_LOG(LogExtendedEOS, Log, TEXT("%s%s"), LINE_TERMINATOR, *Plan.ToLogString());

	if (Plan.Problems.Num() > 0)
	{
		Notify(FText::Format(
			LOCTEXT("PlanProblems", "Extended EOS — PIE sign-in problem: {0}{1}"),
			FText::FromString(Plan.Problems[0]),
			Plan.Problems.Num() > 1
				? FText::Format(LOCTEXT("PlanMoreProblems", " (+{0} more — see the Output Log)"), FText::AsNumber(Plan.Problems.Num() - 1))
				: FText::GetEmpty()), false);
		return;
	}

	Notify(FText::Format(
		LOCTEXT("PlanOk", "Extended EOS — signing in {0} client(s): {1}"),
		FText::AsNumber(Plan.SignInOrder.Num()),
		FText::FromString(FString::Join(Plan.SignInOrder, TEXT("  |  ")))), true);
}

void FExtendedEOSEditorModule::ExecuteShowPIEPlan()
{
	UEEOSPIELoginSettings::RefreshStatusOnDefault();
	ReportPlan(FEEOSPIEPlan::Build());
}

void FExtendedEOSEditorModule::ExecuteSyncPIELogins()
{
	// The settings object owns the action so the Project Settings button and this menu entry
	// cannot drift apart; it also refreshes the status block for us.
	GetMutableDefault<UEEOSPIELoginSettings>()->SyncPIELoginsFromDevAuthTool();
	ReportPlan(FEEOSPIEPlan::Build());
}

void FExtendedEOSEditorModule::ExecuteOpenDevAuthTool()
{
	const int32 Port = UEEOSPIELoginSettings::GetDevAuthToolPort();
	const bool bWasRunning = FEEOSDevAuthTool::IsToolListening(Port);

	GetMutableDefault<UEEOSPIELoginSettings>()->OpenDevAuthTool();

	if (bWasRunning)
	{
		Notify(LOCTEXT("ToolAlreadyUp", "Extended EOS: the Dev Auth Tool is already running."), true);
	}
	else if (FEEOSDevAuthTool::FindToolExecutable().IsEmpty())
	{
		Notify(LOCTEXT("ToolMissing",
			"Extended EOS: EOS_DevAuthTool.exe was not found under "
			"Plugins/ExtendedFramework/UnrealExtendedEOS/Thirdparty/EOS/SDK/Tools."), false);
	}
	else
	{
		Notify(LOCTEXT("ToolLaunched",
			"Extended EOS: Dev Auth Tool launched. Add one account per PIE client, then run "
			"\"Sync PIE Logins from Dev Auth Tool\"."), true);
	}
}

void FExtendedEOSEditorModule::HandlePreBeginPIE(const bool /*bIsSimulating*/)
{
	FEEOSPIEPlan Plan = FEEOSPIEPlan::Build();

	// No credential entries at all means this project is not using per-instance PIE logins.
	// Stay completely silent — launching a tool or nagging would be wrong.
	if (Plan.NumConfiguredLogins == 0)
	{
		return;
	}

	// ── Make sure the tool is up before the clients try to use it ────────────
	const UEEOSPIELoginSettings* Settings = GetDefault<UEEOSPIELoginSettings>();
	if (!Plan.bToolRunning && Settings->bAutoLaunchOnPlay)
	{
		const FEEOSDevAuthTool::EEnvironment Environment = Settings->bUseGameDevEnvironment
			? FEEOSDevAuthTool::EEnvironment::GameDev
			: FEEOSDevAuthTool::EEnvironment::Prod;

		if (FEEOSDevAuthTool::LaunchTool(Environment))
		{
			// PIE cannot be deferred asynchronously, so this waits inline. It only happens on
			// the first Play after the tool is closed; the slow task keeps the editor honest
			// about why it is unresponsive.
			FScopedSlowTask SlowTask(1.f, LOCTEXT("WaitingForTool", "Waiting for the EOS Dev Auth Tool to start..."));
			SlowTask.MakeDialog();
			FEEOSDevAuthTool::WaitForTool(UEEOSPIELoginSettings::GetDevAuthToolPort(), Settings->LaunchTimeoutSeconds);
			SlowTask.EnterProgressFrame(1.f);

			// Re-resolve: the tool being up (and its account store now readable) can clear
			// several of the problems collected a moment ago.
			Plan = FEEOSPIEPlan::Build();
		}
	}

	// Keep the Project Settings view in step with what just happened.
	UEEOSPIELoginSettings::RefreshStatusOnDefault();

	// Always report — the sign-in order is the thing worth seeing on every Play, not just when
	// something is broken.
	ReportPlan(Plan);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FExtendedEOSEditorModule, ExtendedEOSEditor)
