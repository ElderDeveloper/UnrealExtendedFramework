// Copyright Moon Punch Games. All Rights Reserved.

#include "EELogViewerFeature.h"

#if WITH_EDITOR

#include "EFLog.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/World.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Modules/ModuleManager.h"
#include "PlayInEditorDataTypes.h"
#include "SEELogViewerWindow.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "EELogViewerFeature"

const FName FEELogViewerFeature::TabId(TEXT("ExtendedLog"));

void FEELogViewerFeature::Register(const TSharedRef<FWorkspaceItem>& InGroup)
{
	// Window > Log, beside Output Log and Message Log, where a log window is looked for; not the
	// Extended Framework tools group the other features use (InGroup).
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabId, FOnSpawnTab::CreateRaw(this, &FEELogViewerFeature::SpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Extended Log"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Every EF_LOG category file as a live tab: filter, follow, and jump to the line that wrote it."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsLogCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "MessageLog.TabIcon"));

	BeginPIEHandle = FEditorDelegates::BeginPIE.AddRaw(this, &FEELogViewerFeature::HandleBeginPIE);
	EndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &FEELogViewerFeature::HandleEndPIE);
	ShutdownPIEHandle = FEditorDelegates::ShutdownPIE.AddRaw(this, &FEELogViewerFeature::HandleShutdownPIE);

#if EF_LOG_ENABLED
	// The writer reports a missing folder through the debugger output only; here it also becomes
	// something a person sees.
	if (EFLog::GetRootDirectory().IsEmpty())
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		if (MainFrame.IsWindowInitialized())
		{
			ShowOutputOffNotification();
		}
		else
		{
			MainFrameHandle = MainFrame.OnMainFrameCreationFinished().AddRaw(this, &FEELogViewerFeature::HandleMainFrameCreated);
		}
	}
#endif

	bRegistered = true;
}

void FEELogViewerFeature::Unregister()
{
	if (!bRegistered)
	{
		return;
	}

	FEditorDelegates::BeginPIE.Remove(BeginPIEHandle);
	FEditorDelegates::EndPIE.Remove(EndPIEHandle);
	FEditorDelegates::ShutdownPIE.Remove(ShutdownPIEHandle);

	if (MainFrameHandle.IsValid())
	{
		if (IMainFrameModule* MainFrame = FModuleManager::GetModulePtr<IMainFrameModule>(TEXT("MainFrame")))
		{
			MainFrame->OnMainFrameCreationFinished().Remove(MainFrameHandle);
		}
		MainFrameHandle.Reset();
	}

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);
	bRegistered = false;
}

void FEELogViewerFeature::HandleMainFrameCreated(TSharedPtr<SWindow> MainWindow, bool bIsRunningStartupDialog)
{
	if (IMainFrameModule* MainFrame = FModuleManager::GetModulePtr<IMainFrameModule>(TEXT("MainFrame")))
	{
		MainFrame->OnMainFrameCreationFinished().Remove(MainFrameHandle);
	}
	MainFrameHandle.Reset();

	ShowOutputOffNotification();
}

void FEELogViewerFeature::ShowOutputOffNotification()
{
	FNotificationInfo Info(LOCTEXT("OutputOff", "Extended Log: EF_LOG could not create its log folder under Saved/Logs, so nothing it logs is written this session."));
	Info.ExpireDuration = 10.0f;
	Info.bUseSuccessFailIcons = true;

	if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
	{
		Item->SetCompletionState(SNotificationItem::CS_Fail);
	}
}

TSharedRef<SDockTab> FEELogViewerFeature::SpawnTab(const FSpawnTabArgs& Args)
{
	const TSharedRef<SDockTab> MajorTab = SNew(SDockTab).TabRole(ETabRole::NomadTab);
	MajorTab->SetContent(SNew(SEELogViewerWindow, MajorTab, Args.GetOwnerWindow()));
	return MajorTab;
}

void FEELogViewerFeature::HandleBeginPIE(const bool bIsSimulating)
{
	// A start the engine abandoned without an EndPIE (a failed pre-create) is closed here, so two runs
	// never share one marker pair.
	CloseRun();

	++RunCount;
	bRunOpen = true;
	bRunIsSimulation = bIsSimulating;

	// "==== PIE 3 started | L_CastleTest | listen server + 1 client ====" in every category file,
	// including ones first written later in this run.
	EFLog::WriteSeparator(
		FString::Printf(TEXT("%s %d started | %s"), bIsSimulating ? TEXT("Simulate") : TEXT("PIE"), RunCount, *DescribePlaySession(bIsSimulating)),
		EEFLogSeparatorKind::RunStarted);
}

void FEELogViewerFeature::HandleEndPIE(const bool bIsSimulating)
{
	// A normal end still has its teardown ahead (the end marker waits for ShutdownPIE). Without a
	// play world the start was cancelled (a Blueprint error dialog, a failed instance), and no
	// ShutdownPIE will follow.
	if (bRunOpen && (!GEditor || !GEditor->PlayWorld))
	{
		CloseRun();
	}
}

void FEELogViewerFeature::HandleShutdownPIE(const bool bIsSimulating)
{
	CloseRun();
}

void FEELogViewerFeature::CloseRun()
{
	if (!bRunOpen)
	{
		return;
	}

	bRunOpen = false;
	EFLog::WriteSeparator(
		FString::Printf(TEXT("%s %d ended"), bRunIsSimulation ? TEXT("Simulate") : TEXT("PIE"), RunCount),
		EEFLogSeparatorKind::RunEnded);
}

FString FEELogViewerFeature::DescribePlaySession(bool bIsSimulating)
{
	// At BeginPIE there is no play world yet: the map being played is the editor's.
	FString MapName = TEXT("unknown map");
	const UWorld* PlayedWorld = GEditor ? (GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World()) : nullptr;
	if (PlayedWorld)
	{
		MapName = UWorld::RemovePIEPrefix(PlayedWorld->GetMapName());
	}

	if (bIsSimulating)
	{
		return FString::Printf(TEXT("%s | simulate"), *MapName);
	}

	// The settings the session was requested with, which can differ from the saved defaults.
	const ULevelEditorPlaySettings* PlaySettings = nullptr;
	if (GEditor)
	{
		const TOptional<FPlayInEditorSessionInfo> Session = GEditor->GetPlayInEditorSessionInfo();
		if (Session.IsSet())
		{
			PlaySettings = Session->OriginalRequestParams.EditorPlaySettings;
		}
	}
	if (!PlaySettings)
	{
		PlaySettings = GetDefault<ULevelEditorPlaySettings>();
	}

	EPlayNetMode NetMode = PIE_Standalone;
	int32 Clients = 1;
	bool bRunUnderOneProcess = true;
	PlaySettings->GetPlayNetMode(NetMode);
	PlaySettings->GetPlayNumberOfClients(Clients);
	PlaySettings->GetRunUnderOneProcess(bRunUnderOneProcess);
	Clients = FMath::Max(1, Clients);

	auto Plural = [](int32 Count) { return Count == 1 ? TEXT("") : TEXT("s"); };

	FString Topology;
	switch (NetMode)
	{
	case PIE_ListenServer:
		Topology = Clients > 1
			? FString::Printf(TEXT("listen server + %d client%s"), Clients - 1, Plural(Clients - 1))
			: FString(TEXT("listen server"));
		break;

	case PIE_Client:
		Topology = FString::Printf(TEXT("dedicated server + %d client%s"), Clients, Plural(Clients));
		break;

	default:
		Topology = Clients > 1 ? FString::Printf(TEXT("%d standalone instances"), Clients) : FString(TEXT("standalone"));
		break;
	}

	if (!bRunUnderOneProcess && Clients > 1)
	{
		Topology += TEXT(", separate processes");
	}

	return FString::Printf(TEXT("%s | %s"), *MapName, *Topology);
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
