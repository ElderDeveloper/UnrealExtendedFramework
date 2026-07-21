// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PerfSentinelEditorMenu.h"

#include "UnrealExtendedPerfSentinel.h"
#include "PerfSentinelAnalysisManager.h"
#include "PerfSentinelEditorCommands.h"
#include "PerfSentinelPythonRunner.h"
#include "PerfSentinelReportView.h"
#include "PerfSentinelSettings.h"
#include "PerfSentinelTraceController.h"

#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "IDesktopPlatform.h"
#include "ISettingsModule.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "PerfSentinelEditorMenu"

void FPerfSentinelEditorMenu::Register()
{
	BindCommands();
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPerfSentinelEditorMenu::RegisterMenus));
}

void FPerfSentinelEditorMenu::Unregister()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	CommandList.Reset();
}

void FPerfSentinelEditorMenu::BindCommands()
{
	CommandList = MakeShared<FUICommandList>();
	const FPerfSentinelEditorCommands& Commands = FPerfSentinelEditorCommands::Get();

	CommandList->MapAction(
		Commands.StartTraceCapture,
		FExecuteAction::CreateRaw(this, &FPerfSentinelEditorMenu::StartTraceCapture),
		FCanExecuteAction::CreateRaw(this, &FPerfSentinelEditorMenu::CanStartTraceCapture));

	CommandList->MapAction(
		Commands.StopTraceCapture,
		FExecuteAction::CreateRaw(this, &FPerfSentinelEditorMenu::StopTraceCapture),
		FCanExecuteAction::CreateRaw(this, &FPerfSentinelEditorMenu::CanStopTraceCapture));

	CommandList->MapAction(
		Commands.AnalyzeLastTrace,
		FExecuteAction::CreateRaw(this, &FPerfSentinelEditorMenu::AnalyzeLastTrace),
		FCanExecuteAction::CreateRaw(this, &FPerfSentinelEditorMenu::CanAnalyzeLastTrace));

	CommandList->MapAction(
		Commands.AnalyzeExistingTrace,
		FExecuteAction::CreateRaw(this, &FPerfSentinelEditorMenu::AnalyzeExistingTrace));

	CommandList->MapAction(
		Commands.CancelAnalysis,
		FExecuteAction::CreateRaw(this, &FPerfSentinelEditorMenu::CancelAnalysis),
		FCanExecuteAction::CreateRaw(this, &FPerfSentinelEditorMenu::CanCancelAnalysis));

	CommandList->MapAction(
		Commands.OpenLastReport,
		FExecuteAction::CreateRaw(this, &FPerfSentinelEditorMenu::OpenLastReport));

	CommandList->MapAction(
		Commands.LaunchProfileSession,
		FExecuteAction::CreateRaw(this, &FPerfSentinelEditorMenu::LaunchProfileSession));

	CommandList->MapAction(
		Commands.CopyProfileLaunchArguments,
		FExecuteAction::CreateRaw(this, &FPerfSentinelEditorMenu::CopyProfileLaunchArguments));

	CommandList->MapAction(
		Commands.OpenReportsFolder,
		FExecuteAction::CreateRaw(this, &FPerfSentinelEditorMenu::OpenReportsFolder));

	CommandList->MapAction(
		Commands.OpenPerfSentinelSettings,
		FExecuteAction::CreateRaw(this, &FPerfSentinelEditorMenu::OpenPerfSentinelSettings));
}

void FPerfSentinelEditorMenu::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* MainMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu"));
	FToolMenuSection& TopLevelSection = MainMenu->FindOrAddSection(TEXT("Window"));
	TopLevelSection.AddSubMenu(
		TEXT("PerfSentinel"),
		LOCTEXT("PerfSentinelMenuLabel", "PerfSentinel"),
		LOCTEXT("PerfSentinelMenuTooltip", "PerfSentinel trace capture and analysis tools."),
		FNewToolMenuDelegate::CreateLambda([this](UToolMenu* InMenu)
		{
			const FPerfSentinelEditorCommands& Commands = FPerfSentinelEditorCommands::Get();

			FToolMenuSection& CaptureSection = InMenu->AddSection(TEXT("Capture"), LOCTEXT("CaptureSection", "Capture"));
			CaptureSection.AddSubMenu(
				TEXT("CaptureProfile"),
				LOCTEXT("CaptureProfileLabel", "Capture Profile"),
				LOCTEXT("CaptureProfileTooltip", "Choose a provider set tailored to the investigation."),
				FNewToolMenuDelegate::CreateLambda([this](UToolMenu* ProfileMenu)
				{
					FToolMenuSection& ProfileSection = ProfileMenu->AddSection(TEXT("Profiles"));
					auto AddProfile = [this, &ProfileSection](FName Name, const FText& Label, const FText& Tooltip, EPerfSentinelCaptureProfile Profile)
					{
						const FUIAction Action(
							FExecuteAction::CreateRaw(this, &FPerfSentinelEditorMenu::SetCaptureProfile, Profile),
							FCanExecuteAction(),
							FIsActionChecked::CreateLambda([Profile]()
							{
								const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
								return Settings && Settings->CaptureProfile == Profile;
							}));
						ProfileSection.AddEntry(FToolMenuEntry::InitMenuEntry(Name, Label, Tooltip, FSlateIcon(), Action, EUserInterfaceActionType::RadioButton));
					};

					AddProfile(TEXT("Standard"), LOCTEXT("ProfileStandard", "Standard"), LOCTEXT("ProfileStandardTip", "Low-overhead CPU, GPU, frame, counter, stats, region, and bookmark capture."), EPerfSentinelCaptureProfile::Standard);
					AddProfile(TEXT("HitchDiagnosis"), LOCTEXT("ProfileHitch", "Hitch Diagnosis"), LOCTEXT("ProfileHitchTip", "Adds task scheduling, context-switch, and stack-sampling evidence."), EPerfSentinelCaptureProfile::HitchDiagnosis);
					AddProfile(TEXT("LoadingStreaming"), LOCTEXT("ProfileLoading", "Loading / Streaming"), LOCTEXT("ProfileLoadingTip", "Adds load-time, file, and asset metadata providers."), EPerfSentinelCaptureProfile::LoadingStreaming);
					AddProfile(TEXT("MemoryLeak"), LOCTEXT("ProfileMemory", "Memory Leak"), LOCTEXT("ProfileMemoryTip", "Adds memory allocations, tags, callstacks, modules, and asset metadata."), EPerfSentinelCaptureProfile::MemoryLeak);
					AddProfile(TEXT("Multiplayer"), LOCTEXT("ProfileMultiplayer", "Multiplayer"), LOCTEXT("ProfileMultiplayerTip", "Adds packet, connection, and network event evidence."), EPerfSentinelCaptureProfile::Multiplayer);
					AddProfile(TEXT("UIAnimation"), LOCTEXT("ProfileUI", "UI / Animation"), LOCTEXT("ProfileUITip", "Adds Slate and animation trace providers."), EPerfSentinelCaptureProfile::UIAnimation);
				}));
			CaptureSection.AddMenuEntryWithCommandList(Commands.LaunchProfileSession, CommandList);
			CaptureSection.AddMenuEntryWithCommandList(Commands.CopyProfileLaunchArguments, CommandList);
			CaptureSection.AddMenuEntryWithCommandList(Commands.StartTraceCapture, CommandList);
			CaptureSection.AddMenuEntryWithCommandList(Commands.StopTraceCapture, CommandList);

			FToolMenuSection& AnalysisSection = InMenu->AddSection(TEXT("Analysis"), LOCTEXT("AnalysisSection", "Analysis"));
			AnalysisSection.AddMenuEntryWithCommandList(Commands.AnalyzeLastTrace, CommandList);
			AnalysisSection.AddMenuEntryWithCommandList(Commands.AnalyzeExistingTrace, CommandList);
			AnalysisSection.AddMenuEntryWithCommandList(Commands.CancelAnalysis, CommandList);
			AnalysisSection.AddMenuEntryWithCommandList(Commands.OpenLastReport, CommandList);

			FToolMenuSection& UtilitiesSection = InMenu->AddSection(TEXT("Utilities"), LOCTEXT("UtilitiesSection", "Utilities"));
			UtilitiesSection.AddMenuEntryWithCommandList(Commands.OpenReportsFolder, CommandList);
			UtilitiesSection.AddMenuEntryWithCommandList(Commands.OpenPerfSentinelSettings, CommandList);
		}));
}

void FPerfSentinelEditorMenu::StartTraceCapture()
{
	if (FPerfSentinelTraceController* Controller = GetController())
	{
		Controller->StartCapture(TEXT("EditorMenu"));
	}
}

void FPerfSentinelEditorMenu::StopTraceCapture()
{
	if (FPerfSentinelTraceController* Controller = GetController())
	{
		Controller->StopCapture();
	}
}

void FPerfSentinelEditorMenu::AnalyzeLastTrace()
{
	RunAnalysisForSession();
}

void FPerfSentinelEditorMenu::AnalyzeExistingTrace()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("AnalyzeExistingTrace: DesktopPlatform is unavailable."));
		return;
	}

	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	const FString DefaultPath = Settings ? Settings->GetResolvedTraceOutputDirectory() : FPaths::ProjectSavedDir();

	const void* ParentWindowHandle = nullptr;
	if (FSlateApplication::IsInitialized())
	{
		ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	}

	TArray<FString> OutFiles;
	const bool bPickedFile = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("Choose Unreal Trace"),
		DefaultPath,
		TEXT(""),
		TEXT("Unreal Trace (*.utrace)|*.utrace|All Files (*.*)|*.*"),
		EFileDialogFlags::None,
		OutFiles);

	if (bPickedFile && OutFiles.Num() > 0)
	{
		RunAnalysisForTrace(OutFiles[0]);
	}
}

void FPerfSentinelEditorMenu::CancelAnalysis()
{
	if (const TSharedPtr<FPerfSentinelAnalysisManager> Manager = FUnrealExtendedPerfSentinelModule::GetAnalysisManager())
	{
		Manager->CancelAnalysis();
	}
}

void FPerfSentinelEditorMenu::OpenLastReport()
{
	FGlobalTabmanager::Get()->TryInvokeTab(PerfSentinelReportTabName);
}

void FPerfSentinelEditorMenu::LaunchProfileSession()
{
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (!Settings)
	{
		return;
	}

	TArray<FString> Arguments;
	Arguments.Add(FString::Printf(TEXT("\"%s\""), *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath())));
	Arguments.Append(Settings->GetRequiredLaunchArguments());
	Arguments.Append({ TEXT("-game"), TEXT("-log"), TEXT("-NoSplash"), TEXT("-ExecCmds=\"PerfSentinel.StartCapture ProfileSession\"") });
	const FString CommandLine = FString::Join(Arguments, TEXT(" "));
	FProcHandle Process = FPlatformProcess::CreateProc(FPlatformProcess::ExecutablePath(), *CommandLine, true, false, false, nullptr, 0, *FPaths::ProjectDir(), nullptr, nullptr);
	if (!Process.IsValid())
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("LaunchProfileSession: Failed to launch %s %s"), FPlatformProcess::ExecutablePath(), *CommandLine);
	}
	else
	{
		FPlatformProcess::CloseProc(Process);
		UE_LOG(LogPerfSentinel, Log, TEXT("LaunchProfileSession: Started profile process with: %s"), *CommandLine);
	}
}

void FPerfSentinelEditorMenu::CopyProfileLaunchArguments()
{
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (!Settings)
	{
		return;
	}
	const FString Arguments = FString::Join(Settings->GetRequiredLaunchArguments(), TEXT(" "));
	FPlatformApplicationMisc::ClipboardCopy(*Arguments);
	UE_LOG(LogPerfSentinel, Log, TEXT("CopyProfileLaunchArguments: Copied '%s'."), *Arguments);
}

void FPerfSentinelEditorMenu::SetCaptureProfile(EPerfSentinelCaptureProfile Profile)
{
	UPerfSentinelSettings* Settings = GetMutableDefault<UPerfSentinelSettings>();
	if (!Settings)
	{
		return;
	}
	Settings->CaptureProfile = Profile;
	Settings->ApplyCaptureProfile();
	Settings->TryUpdateDefaultConfigFile();
	UE_LOG(LogPerfSentinel, Log, TEXT("Capture profile changed to %d. Channels: %s"), static_cast<int32>(Profile), *FString::Join(Settings->TraceChannels, TEXT(",")));
}

void FPerfSentinelEditorMenu::OpenReportsFolder()
{
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("OpenReportsFolder: PerfSentinel settings are unavailable."));
		return;
	}

	const FString ReportsDir = Settings->GetResolvedReportOutputDirectory();
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*ReportsDir);
	FPlatformProcess::ExploreFolder(*ReportsDir);
}

void FPerfSentinelEditorMenu::OpenPerfSentinelSettings()
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings"));
	if (!SettingsModule)
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("OpenPerfSentinelSettings: Settings module is unavailable."));
		return;
	}

	SettingsModule->ShowViewer(TEXT("Project"), TEXT("Plugins"), TEXT("PerfSentinel"));
}

bool FPerfSentinelEditorMenu::CanStartTraceCapture() const
{
	const FPerfSentinelTraceController* Controller = GetController();
	return Controller && Controller->IsIdle();
}

bool FPerfSentinelEditorMenu::CanStopTraceCapture() const
{
	const FPerfSentinelTraceController* Controller = GetController();
	return Controller && Controller->IsCapturing();
}

bool FPerfSentinelEditorMenu::CanAnalyzeLastTrace() const
{
	const FPerfSentinelTraceController* Controller = GetController();
	const TSharedPtr<FPerfSentinelAnalysisManager> Manager = FUnrealExtendedPerfSentinelModule::GetAnalysisManager();
	return Controller && Controller->HasCompletedTrace() && (!Manager || !Manager->IsRunning());
}

bool FPerfSentinelEditorMenu::CanCancelAnalysis() const
{
	const TSharedPtr<FPerfSentinelAnalysisManager> Manager = FUnrealExtendedPerfSentinelModule::GetAnalysisManager();
	return Manager && Manager->IsRunning();
}

FPerfSentinelTraceController* FPerfSentinelEditorMenu::GetController() const
{
	return FUnrealExtendedPerfSentinelModule::GetTraceController();
}

void FPerfSentinelEditorMenu::RunAnalysisForSession() const
{
	const FPerfSentinelTraceController* Controller = GetController();
	if (!Controller || !Controller->HasCompletedTrace())
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("AnalyzeLastTrace: No completed trace is available."));
		return;
	}

	FPerfSentinelPythonRunner Runner;
	FPerfSentinelAnalysisRequest Request;
	FString Error;
	if (!Runner.BuildRequestForSession(Controller->GetLastCompletedSession(), Request, Error))
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("AnalyzeLastTrace: %s"), *Error);
		return;
	}
	const TSharedPtr<FPerfSentinelAnalysisManager> Manager = FUnrealExtendedPerfSentinelModule::GetAnalysisManager();
	if (!Manager || !Manager->StartAnalysis(Request, Error))
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("AnalyzeLastTrace: %s"), *Error);
	}
}

void FPerfSentinelEditorMenu::RunAnalysisForTrace(const FString& TracePath) const
{
	FPerfSentinelPythonRunner Runner;
	FPerfSentinelAnalysisRequest Request;
	FString Error;
	if (!Runner.BuildRequestForTrace(TracePath, Request, Error))
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("AnalyzeExistingTrace: %s"), *Error);
		return;
	}
	const TSharedPtr<FPerfSentinelAnalysisManager> Manager = FUnrealExtendedPerfSentinelModule::GetAnalysisManager();
	if (!Manager || !Manager->StartAnalysis(Request, Error))
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("AnalyzeExistingTrace: %s"), *Error);
	}
}

#undef LOCTEXT_NAMESPACE
