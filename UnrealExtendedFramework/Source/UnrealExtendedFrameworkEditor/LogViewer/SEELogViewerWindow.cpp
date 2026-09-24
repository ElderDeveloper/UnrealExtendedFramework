// Copyright Moon Punch Games. All Rights Reserved.

#include "SEELogViewerWindow.h"

#if WITH_EDITOR

#include "DirectoryWatcherModule.h"
#include "EFLog.h"
#include "EFLogViewerSettings.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "IDirectoryWatcher.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SEELogFileTab.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "EELogViewerWindow"

namespace EELogViewerWindowPrivate
{
	/** The layout's only stack: every log tab is inserted next to this closed placeholder. */
	static const FName DocumentTabId(TEXT("EELogViewerDocument"));

	static const TCHAR* ArchivePrefix = TEXT("Archive/");

	/** The All tab's key among the file keys. ':' cannot appear in a file name, so it never clashes with one. */
	static const FString AllTabKey(TEXT(":All"));
}

void SEELogViewerWindow::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& InMajorTab, const TSharedPtr<SWindow>& InOwnerWindow)
{
	using namespace EELogViewerWindowPrivate;

	RootDirectory = EFLog::GetRootDirectory();
	FPaths::NormalizeDirectoryName(RootDirectory);

	// Document tabs only, as asset editors host theirs: the placeholder is a closed tab, and each
	// log tab is inserted beside it. Nothing here is worth persisting, so no layout is saved.
	TabManager = FGlobalTabmanager::Get()->NewTabManager(InMajorTab);
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("EELogViewer_Layout_v1"))
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->AddTab(DocumentTabId, ETabState::ClosedTab)
			)
		);
	const TSharedPtr<SWidget> TabArea = TabManager->RestoreFrom(Layout, InOwnerWindow);

	InMajorTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateSP(this, &SEELogViewerWindow::HandleMajorTabClosed));

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(4.0f))
		[
			BuildToolbar()
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			[
				TabArea.IsValid() ? TabArea.ToSharedRef() : SNullWidget::NullWidget
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.MaxDesiredWidth(520.0f)
				.Visibility_Lambda([this]() { return OpenLogs.Num() == 0 && QueuedOpens.Num() == 0 ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Justification(ETextJustify::Center)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Text_Lambda([this]()
					{
						if (RootDirectory.IsEmpty())
						{
							return LOCTEXT("OutputOff", "EF_LOG file output is off for this session (no log folder could be created, or logging is compiled out).");
						}
						return FText::Format(
							LOCTEXT("EmptyHint", "No log tabs open.\n\nEvery EF_LOG(Category, ...) category writes its own file, and each file shows up here as a tab the moment it is written. Closed tabs can be reopened from Open log.\n\n{0}"),
							FText::FromString(RootDirectory));
					})
				]
			]
		]
	];

	if (RootDirectory.IsEmpty())
	{
		return;
	}

	RestoreTabs();

	// Files written by other processes (separate-process PIE clients, a standalone game, a commandlet).
	if (FDirectoryWatcherModule* Watcher = FModuleManager::LoadModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher")))
	{
		if (IDirectoryWatcher* DirectoryWatcher = Watcher->Get())
		{
			DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
				RootDirectory,
				IDirectoryWatcher::FDirectoryChanged::CreateSP(this, &SEELogViewerWindow::HandleDirectoryChanged),
				DirectoryWatcherHandle,
				0);
		}
	}

	// This process's own files: EF_LOG says which ones it just flushed, without waiting on the OS.
	FilesFlushedHandle = EFLog::AddFilesFlushedListener(FEFLogFilesFlushed::FDelegate::CreateSP(this, &SEELogViewerWindow::HandleFilesFlushed));
}

SEELogViewerWindow::~SEELogViewerWindow()
{
	if (FilesFlushedHandle.IsValid())
	{
		EFLog::RemoveFilesFlushedListener(FilesFlushedHandle);
	}

	if (DirectoryWatcherHandle.IsValid())
	{
		if (FDirectoryWatcherModule* Watcher = FModuleManager::GetModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher")))
		{
			if (IDirectoryWatcher* DirectoryWatcher = Watcher->Get())
			{
				DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(RootDirectory, DirectoryWatcherHandle);
			}
		}
	}
}

TSharedRef<SWidget> SEELogViewerWindow::BuildToolbar()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SComboButton)
			.ToolTipText(LOCTEXT("OpenLogTooltip", "Open a log file of this session, or of an archived one."))
			.OnGetMenuContent(this, &SEELogViewerWindow::BuildOpenMenu)
			.ButtonContent()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Plus"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("OpenLog", "Open log"))
				]
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
		[
			SNew(SButton)
			.Text(LOCTEXT("ShowFolder", "Show folder"))
			.IsEnabled_Lambda([this]() { return !RootDirectory.IsEmpty(); })
			.OnClicked_Lambda([this]()
			{
				FPlatformProcess::ExploreFolder(*RootDirectory);
				return FReply::Handled();
			})
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		.Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text(FText::FromString(RootDirectory))
		];
}

TSharedRef<SWidget> SEELogViewerWindow::BuildOpenMenu()
{
	using namespace EELogViewerWindowPrivate;

	FMenuBuilder Menu(true, nullptr);

	Menu.BeginSection(TEXT("Combined"), LOCTEXT("Combined", "Combined"));
	Menu.AddMenuEntry(
		LOCTEXT("AllCategories", "All categories"),
		LOCTEXT("AllCategoriesTooltip", "One tab with every log file of this session, merged by the time each line was written. Its eye menu picks the categories."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SEELogViewerWindow::OpenLogFromMenu, AllTabKey),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SEELogViewerWindow::IsLogOpen, AllTabKey)),
		NAME_None,
		EUserInterfaceActionType::Check);
	Menu.EndSection();

	Menu.BeginSection(TEXT("ThisSession"), LOCTEXT("ThisSession", "This session"));
	{
		const TArray<FString> Keys = ScanCurrentKeys();
		if (Keys.Num() == 0)
		{
			Menu.AddMenuEntry(
				LOCTEXT("NoFiles", "No log files yet"),
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() { return false; })));
		}

		for (const FString& Key : Keys)
		{
			Menu.AddMenuEntry(
				FText::FromString(GetTitleForKey(Key)),
				FText::FromString(ToFullPath(Key)),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SEELogViewerWindow::OpenLogFromMenu, Key),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SEELogViewerWindow::IsLogOpen, Key)),
				NAME_None,
				EUserInterfaceActionType::Check);
		}
	}
	Menu.EndSection();

	const TArray<FString> Sessions = ScanArchiveSessions();
	if (Sessions.Num() > 0)
	{
		Menu.BeginSection(TEXT("Archive"), LOCTEXT("ArchivedSessions", "Archived sessions"));
		for (const FString& Session : Sessions)
		{
			Menu.AddSubMenu(
				FText::FromString(Session),
				FText::GetEmpty(),
				FNewMenuDelegate::CreateSP(this, &SEELogViewerWindow::BuildArchiveSessionMenu, Session));
		}
		Menu.EndSection();
	}

	return Menu.MakeWidget();
}

void SEELogViewerWindow::BuildArchiveSessionMenu(FMenuBuilder& Menu, FString SessionFolder)
{
	using namespace EELogViewerWindowPrivate;

	TArray<FString> FileNames;
	IFileManager::Get().FindFiles(FileNames, *(RootDirectory / ArchivePrefix / SessionFolder), TEXT(".log"));
	FileNames.Sort();

	for (const FString& FileName : FileNames)
	{
		const FString Key = FString(ArchivePrefix) + SessionFolder + TEXT("/") + FileName;
		Menu.AddMenuEntry(
			FText::FromString(FPaths::GetBaseFilename(FileName)),
			FText::FromString(ToFullPath(Key)),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SEELogViewerWindow::OpenLogFromMenu, Key),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SEELogViewerWindow::IsLogOpen, Key)),
			NAME_None,
			EUserInterfaceActionType::Check);
	}
}

void SEELogViewerWindow::ExtendTabContextMenu(FMenuBuilder& Menu, FString Key)
{
	using namespace EELogViewerWindowPrivate;

	Menu.BeginSection(TEXT("EELogFile"), LOCTEXT("LogFileSection", "Log file"));

	// The All tab is no one file: only its view can be cleared.
	if (Key != AllTabKey)
	{
		const FString FullPath = ToFullPath(Key);

		Menu.AddMenuEntry(
			LOCTEXT("OpenExternal", "Open in external editor"),
			FText::FromString(FullPath),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.OpenInExternalEditor"),
			FUIAction(FExecuteAction::CreateLambda([FullPath]() { FPlatformProcess::LaunchFileInDefaultExternalApplication(*FullPath); })));

		Menu.AddMenuEntry(
			LOCTEXT("ShowInExplorer", "Show in Explorer"),
			FText::GetEmpty(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderOpen"),
			FUIAction(FExecuteAction::CreateLambda([FullPath]() { FPlatformProcess::ExploreFolder(*FullPath); })));

		Menu.AddMenuEntry(
			LOCTEXT("CopyPath", "Copy path"),
			FText::GetEmpty(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
			FUIAction(FExecuteAction::CreateLambda([FullPath]() { FPlatformApplicationMisc::ClipboardCopy(*FullPath); })));
	}

	if (const FOpenLog* Open = OpenLogs.Find(Key))
	{
		const TSharedPtr<SEELogFileTab> Content = Open->Content;
		Menu.AddMenuEntry(
			LOCTEXT("ClearView", "Clear view"),
			LOCTEXT("ClearViewTooltip", "Hide the rows read so far. The file is not touched."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([Content]()
			{
				if (Content.IsValid())
				{
					Content->ClearView();
				}
			})));
	}

	// Only this process's live files belong to categories it can reconfigure.
	const FString CategoryName = GetLiveCategoryName(Key);
	if (!CategoryName.IsEmpty())
	{
		Menu.AddSubMenu(
			LOCTEXT("Verbosity", "Verbosity"),
			LOCTEXT("VerbosityTooltip", "The level this category writes down to, for this session. Lines above it are never written, so they cannot be shown."),
			FNewMenuDelegate::CreateSP(this, &SEELogViewerWindow::BuildVerbosityMenu, CategoryName));
	}

	Menu.EndSection();
}

void SEELogViewerWindow::BuildVerbosityMenu(FMenuBuilder& Menu, FString CategoryName)
{
	for (uint8 Value = static_cast<uint8>(EEFLogVerbosity::Error); Value <= static_cast<uint8>(EEFLogVerbosity::VeryVerbose); ++Value)
	{
		const EEFLogVerbosity Verbosity = static_cast<EEFLogVerbosity>(Value);
		Menu.AddMenuEntry(
			FText::FromString(LexToString(Verbosity)),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([CategoryName, Verbosity]() { EFLog::SetCategoryVerbosity(FName(*CategoryName), Verbosity); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([CategoryName, Verbosity]()
				{
					EEFLogVerbosity Current = EEFLogVerbosity::Log;
					return EFLog::GetCategoryVerbosity(FName(*CategoryName), Current) && Current == Verbosity;
				})),
			NAME_None,
			EUserInterfaceActionType::RadioButton);
	}
}

void SEELogViewerWindow::RestoreTabs()
{
	using namespace EELogViewerWindowPrivate;

	UEFLogViewerSettings* Settings = GetMutableDefault<UEFLogViewerSettings>();

	// A file in another process's folder ("Game_20412/Araf.log") names a pid that will not come back,
	// so remembering it past its session only grows the lists.
	Settings->ClosedTabs.RemoveAll([](const FString& Key) { return Key.Contains(TEXT("/")); });
	Settings->AllTabHiddenFiles.RemoveAll([](const FString& Key) { return Key.Contains(TEXT("/")); });

	const TArray<FString> Keys = ScanCurrentKeys();
	KnownFiles.Append(Keys);

	// The saved order first, then (when auto-open is on) every other file not closed on purpose.
	TArray<FString> Order;
	for (const FString& Saved : Settings->OpenTabs)
	{
		if (Keys.Contains(Saved) || Saved == AllTabKey)
		{
			Order.AddUnique(Saved);
		}
	}
	if (Settings->bAutoOpenNewFiles)
	{
		for (const FString& Key : Keys)
		{
			if (!Settings->ClosedTabs.Contains(Key))
			{
				Order.AddUnique(Key);
			}
		}
	}

	// Opened once the window is on screen: an editor restoring its layout with this tab in a
	// background slot must not have it jump forward.
	for (const FString& Key : Order)
	{
		QueueAutomaticOpen(Key);
	}
}

void SEELogViewerWindow::QueueAutomaticOpen(const FString& Key)
{
	QueuedOpens.AddUnique(Key);

	if (!QueuedOpenTimer.IsValid())
	{
		QueuedOpenTimer = RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SEELogViewerWindow::OpenQueuedTabs));
	}
}

EActiveTimerReturnType SEELogViewerWindow::OpenQueuedTabs(double CurrentTime, float DeltaTime)
{
	QueuedOpenTimer.Reset();

	const TArray<FString> Keys = MoveTemp(QueuedOpens);
	QueuedOpens.Reset();

	if (bClosing)
	{
		return EActiveTimerReturnType::Stop;
	}

	// Whatever the user is reading stays in front; the new tabs announce themselves with their
	// unread counts instead.
	TArray<TSharedPtr<SDockTab>> InFront;
	for (const TPair<FString, FOpenLog>& Pair : OpenLogs)
	{
		const TSharedPtr<SDockTab> Tab = Pair.Value.DockTab.Pin();
		if (Tab.IsValid() && Tab->IsForeground())
		{
			InFront.Add(Tab);
		}
	}

	const bool bWasEmpty = InFront.Num() == 0;

	bBatchingSaves = true;
	for (const FString& Key : Keys)
	{
		OpenLog(Key, false);
	}
	bBatchingSaves = false;
	SaveOpenTabs();

	if (bWasEmpty && Keys.Num() > 0)
	{
		// Nothing was open: the first of the batch is the one to show (the saved order puts it first).
		if (const FOpenLog* First = OpenLogs.Find(Keys[0]))
		{
			InFront.Add(First->DockTab.Pin());
		}
	}

	for (const TSharedPtr<SDockTab>& Tab : InFront)
	{
		if (Tab.IsValid())
		{
			Tab->ActivateInParent(ETabActivationCause::SetDirectly);
		}
	}

	return EActiveTimerReturnType::Stop;
}

void SEELogViewerWindow::OpenLog(const FString& Key, bool bUserRequested)
{
	using namespace EELogViewerWindowPrivate;

	if (bUserRequested)
	{
		// Reopened on purpose: it is no longer a tab the user closed.
		GetMutableDefault<UEFLogViewerSettings>()->ClosedTabs.Remove(Key);
		QueuedOpens.Remove(Key);
	}

	if (const FOpenLog* Existing = OpenLogs.Find(Key))
	{
		if (const TSharedPtr<SDockTab> ExistingTab = Existing->DockTab.Pin())
		{
			if (bUserRequested)
			{
				ExistingTab->ActivateInParent(ETabActivationCause::SetDirectly);
			}
			return;
		}
		OpenLogs.Remove(Key);
		OpenOrder.Remove(Key);
	}

	const UEFLogViewerSettings* Settings = GetDefault<UEFLogViewerSettings>();
	const bool bAllTab = Key == AllTabKey;
	const FString FullPath = bAllTab ? RootDirectory : ToFullPath(Key);

	const TSharedRef<SEELogFileTab> Content = SNew(SEELogFileTab)
		.FilePath(FullPath)
		.Title(bAllTab ? LOCTEXT("AllTabTitle", "All").ToString() : GetTitleForKey(Key))
		.Merged(bAllTab)
		.MaxRows(Settings->MaxRowsPerTab)
		.LoadChunkBytes(static_cast<int64>(Settings->LoadChunkMB) * 1024 * 1024);

	const TSharedRef<SDockTab> Tab = SNew(SDockTab)
		.TabRole(ETabRole::DocumentTab)
		.Label(TAttribute<FText>::CreateSP(&Content.Get(), &SEELogFileTab::GetTabLabel))
		.ToolTipText(bAllTab
			? LOCTEXT("AllTabTooltip", "Every log file of this session, merged by the time each line was written.")
			: FText::FromString(FullPath))
		.OnTabClosed(SDockTab::FOnTabClosedCallback::CreateSP(this, &SEELogViewerWindow::HandleLogTabClosed, Key))
		.OnExtendContextMenu(SDockTab::FExtendContextMenu::CreateSP(this, &SEELogViewerWindow::ExtendTabContextMenu, Key))
		[
			Content
		];

	Tab->SetTabIcon(TAttribute<const FSlateBrush*>::CreateSP(&Content.Get(), &SEELogFileTab::GetTabIcon));
	Tab->SetOnTabActivated(SDockTab::FOnTabActivatedCallback::CreateSP(Content, &SEELogFileTab::HandleTabActivated));
	Content->SetOwnerTab(Tab);

	OpenLogs.Add(Key, FOpenLog { Tab, Content });
	OpenOrder.Add(Key);

	TabManager->InsertNewDocumentTab(DocumentTabId, FTabManager::ESearchPreference::RequireClosedTab, Tab);
	if (bUserRequested)
	{
		Tab->ActivateInParent(ETabActivationCause::SetDirectly);
	}

	if (bAllTab)
	{
		// Every live file, including other processes' folders: a separate-process client's lines
		// interleave with the server's by time. Files that appear later join through HandleFileTouched.
		for (const FString& FileKey : ScanCurrentKeys())
		{
			Content->AddSource(FileKey, ToFullPath(FileKey), GetTitleForKey(FileKey));
		}
	}
	else
	{
		Content->Refresh();
	}
	SaveOpenTabs();
}

void SEELogViewerWindow::OpenLogFromMenu(FString Key)
{
	OpenLog(Key, true);
}

bool SEELogViewerWindow::IsLogOpen(FString Key) const
{
	const FOpenLog* Open = OpenLogs.Find(Key);
	return Open && Open->DockTab.IsValid();
}

void SEELogViewerWindow::HandleLogTabClosed(TSharedRef<SDockTab> Tab, FString Key)
{
	if (bClosing)
	{
		return;
	}

	OpenLogs.Remove(Key);
	OpenOrder.Remove(Key);

	// Remembered, so the file does not pop back open the next time it is written or the window opens.
	// (The All tab never opens by itself, so there is nothing to remember for it.)
	if (!IsArchiveKey(Key) && Key != EELogViewerWindowPrivate::AllTabKey)
	{
		GetMutableDefault<UEFLogViewerSettings>()->ClosedTabs.AddUnique(Key);
	}
	SaveOpenTabs();
}

void SEELogViewerWindow::HandleMajorTabClosed(TSharedRef<SDockTab> Tab)
{
	SaveOpenTabs();
	bClosing = true;

	// Also closes any log tab torn off into its own window, which would otherwise outlive this one.
	if (TabManager.IsValid())
	{
		TabManager->CloseAllAreas();
	}
}

void SEELogViewerWindow::SaveOpenTabs() const
{
	if (bClosing || bBatchingSaves)
	{
		return;
	}

	UEFLogViewerSettings* Settings = GetMutableDefault<UEFLogViewerSettings>();
	Settings->OpenTabs.Reset();
	for (const FString& Key : OpenOrder)
	{
		// Archived files belong to one past session; there is nothing to restore them into.
		if (!IsArchiveKey(Key))
		{
			Settings->OpenTabs.Add(Key);
		}
	}
	Settings->SaveConfig();
}

void SEELogViewerWindow::HandleDirectoryChanged(const TArray<FFileChangeData>& Changes)
{
	for (const FFileChangeData& Change : Changes)
	{
		if (Change.Action == FFileChangeData::FCA_RescanRequired)
		{
			RescanAll();
			continue;
		}

		if (!Change.Filename.EndsWith(TEXT(".log"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		FString FullPath = Change.Filename;
		FPaths::NormalizeFilename(FullPath);

		// A roll at the size cap moves the file away and creates it again at once, and the watcher may
		// deliver the removal last: only a file that is really gone is marked so.
		if (Change.Action == FFileChangeData::FCA_Removed && !FPaths::FileExists(FullPath))
		{
			if (const FOpenLog* Open = OpenLogs.Find(ToKey(FullPath)))
			{
				if (Open->Content.IsValid())
				{
					Open->Content->MarkMissing();
				}
			}
			continue;
		}

		HandleFileTouched(FullPath);
	}

	RefreshAllTab();
}

void SEELogViewerWindow::HandleFilesFlushed(const TArray<FString>& FilePaths)
{
	for (const FString& FilePath : FilePaths)
	{
		FString FullPath = FilePath;
		FPaths::NormalizeFilename(FullPath);
		HandleFileTouched(FullPath);
	}

	RefreshAllTab();
}

void SEELogViewerWindow::HandleFileTouched(const FString& FullPath)
{
	const FString Key = ToKey(FullPath);
	if (Key.IsEmpty())
	{
		return;
	}

	// The All tab follows every live file, whether or not it has a tab of its own. A file it has not
	// seen joins it (and is read) here; one it follows is read once the batch is done.
	if (!IsArchiveKey(Key))
	{
		if (const TSharedPtr<SEELogFileTab> AllTab = GetAllTab())
		{
			AllTab->AddSource(Key, FullPath, GetTitleForKey(Key));
			PendingAllTabKeys.AddUnique(Key);
		}
	}

	if (const FOpenLog* Open = OpenLogs.Find(Key))
	{
		if (Open->Content.IsValid())
		{
			Open->Content->Refresh();
		}
		return;
	}

	// The archive is browsed from the Open log menu, never auto-opened.
	if (IsArchiveKey(Key) || KnownFiles.Contains(Key))
	{
		return;
	}

	// A file this window has not seen before: a new category, or another process starting to log.
	KnownFiles.Add(Key);

	const UEFLogViewerSettings* Settings = GetDefault<UEFLogViewerSettings>();
	if (Settings->bAutoOpenNewFiles && !Settings->ClosedTabs.Contains(Key))
	{
		QueueAutomaticOpen(Key);
	}
}

void SEELogViewerWindow::RescanAll()
{
	for (const FString& Key : ScanCurrentKeys())
	{
		HandleFileTouched(ToFullPath(Key));
	}

	RefreshAllTab();
}

TSharedPtr<SEELogFileTab> SEELogViewerWindow::GetAllTab() const
{
	const FOpenLog* Open = OpenLogs.Find(EELogViewerWindowPrivate::AllTabKey);
	return Open && Open->DockTab.IsValid() ? Open->Content : nullptr;
}

void SEELogViewerWindow::RefreshAllTab()
{
	if (PendingAllTabKeys.Num() == 0)
	{
		return;
	}

	const TArray<FString> Keys = MoveTemp(PendingAllTabKeys);
	PendingAllTabKeys.Reset();

	if (const TSharedPtr<SEELogFileTab> AllTab = GetAllTab())
	{
		AllTab->RefreshSources(Keys);
	}
}

TArray<FString> SEELogViewerWindow::ScanCurrentKeys() const
{
	TArray<FString> Keys;
	if (RootDirectory.IsEmpty())
	{
		return Keys;
	}

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *RootDirectory, TEXT("*.log"), true, false);
	for (FString& File : Files)
	{
		FPaths::NormalizeFilename(File);
		const FString Key = ToKey(File);
		if (!Key.IsEmpty() && !IsArchiveKey(Key))
		{
			Keys.Add(Key);
		}
	}

	Keys.Sort();
	return Keys;
}

TArray<FString> SEELogViewerWindow::ScanArchiveSessions() const
{
	using namespace EELogViewerWindowPrivate;

	TArray<FString> Sessions;
	if (!RootDirectory.IsEmpty())
	{
		IFileManager::Get().FindFiles(Sessions, *(RootDirectory / ArchivePrefix / TEXT("*")), false, true);
	}

	// Newest first: folder names start with the session stamp.
	Sessions.Sort([](const FString& A, const FString& B) { return A > B; });
	return Sessions;
}

FString SEELogViewerWindow::ToKey(const FString& FullPath) const
{
	const FString Prefix = RootDirectory + TEXT("/");
	if (RootDirectory.IsEmpty() || !FullPath.StartsWith(Prefix, ESearchCase::IgnoreCase))
	{
		return FString();
	}
	return FullPath.RightChop(Prefix.Len());
}

FString SEELogViewerWindow::ToFullPath(const FString& Key) const
{
	return RootDirectory / Key;
}

bool SEELogViewerWindow::IsArchiveKey(const FString& Key)
{
	return Key.StartsWith(EELogViewerWindowPrivate::ArchivePrefix, ESearchCase::IgnoreCase);
}

FString SEELogViewerWindow::GetTitleForKey(const FString& Key)
{
	using namespace EELogViewerWindowPrivate;

	// "Araf", "Araf - Game_20412", "Araf - 2026.09.24-11.20.05_UnrealEditor".
	const FString Name = FPaths::GetBaseFilename(Key);
	FString Folder = FPaths::GetPath(Key);
	if (Folder.IsEmpty())
	{
		return Name;
	}
	if (Folder.StartsWith(ArchivePrefix, ESearchCase::IgnoreCase))
	{
		Folder.RightChopInline(FCString::Strlen(ArchivePrefix));
	}
	return FString::Printf(TEXT("%s - %s"), *Name, *Folder);
}

FString SEELogViewerWindow::GetLiveCategoryName(const FString& Key) const
{
	if (IsArchiveKey(Key) || Key == EELogViewerWindowPrivate::AllTabKey)
	{
		return FString();
	}

	FString OwnDirectory = EFLog::GetLogDirectory();
	FPaths::NormalizeDirectoryName(OwnDirectory);

	FString FileDirectory = FPaths::GetPath(ToFullPath(Key));
	FPaths::NormalizeDirectoryName(FileDirectory);

	return FileDirectory.Equals(OwnDirectory, ESearchCase::IgnoreCase) ? FPaths::GetBaseFilename(Key) : FString();
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
