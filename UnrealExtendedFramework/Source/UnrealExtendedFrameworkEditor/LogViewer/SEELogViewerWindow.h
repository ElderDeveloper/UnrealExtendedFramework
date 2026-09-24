// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Widgets/SCompoundWidget.h"

class FMenuBuilder;
class FTabManager;
class SDockTab;
class SEELogFileTab;
class SWindow;
struct FFileChangeData;

/**
 * The Extended Log window: one Chrome-style tab per EF_LOG file, and an optional All tab that merges
 * every file of the session by time (Open log > All categories).
 *
 * The tabs are document tabs of a nested tab manager, the way asset editors host theirs, so they can
 * be reordered, closed and torn off into their own windows. Files are found three ways, all
 * event-driven: a scan when the window opens, the directory watcher for files other processes write,
 * and EF_LOG's own flush notification for this process's files (faster than the operating system's).
 */
class SEELogViewerWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEELogViewerWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& InMajorTab, const TSharedPtr<SWindow>& InOwnerWindow);
	virtual ~SEELogViewerWindow() override;

private:
	struct FOpenLog
	{
		TWeakPtr<SDockTab> DockTab;
		TSharedPtr<SEELogFileTab> Content;
	};

	TSharedRef<SWidget> BuildToolbar();
	TSharedRef<SWidget> BuildOpenMenu();
	void BuildArchiveSessionMenu(FMenuBuilder& Menu, FString SessionFolder);
	void ExtendTabContextMenu(FMenuBuilder& Menu, FString Key);
	void BuildVerbosityMenu(FMenuBuilder& Menu, FString CategoryName);

	void RestoreTabs();

	/**
	 * Opens a tab. A user-requested one opens now and comes to the front. An automatic one (restore,
	 * or a newly detected file) waits until this window is actually on screen, and then opens behind
	 * whatever tab is in front, because inserting a tab brings its window's tab to the front of its
	 * dock stack, and a category first written mid-PIE must not pull the window forward.
	 */
	void OpenLog(const FString& Key, bool bUserRequested);
	void QueueAutomaticOpen(const FString& Key);
	EActiveTimerReturnType OpenQueuedTabs(double CurrentTime, float DeltaTime);
	void OpenLogFromMenu(FString Key);
	bool IsLogOpen(FString Key) const;
	void HandleLogTabClosed(TSharedRef<SDockTab> Tab, FString Key);
	void HandleMajorTabClosed(TSharedRef<SDockTab> Tab);
	void SaveOpenTabs() const;

	void HandleDirectoryChanged(const TArray<FFileChangeData>& Changes);
	void HandleFilesFlushed(const TArray<FString>& FilePaths);
	void HandleFileTouched(const FString& FullPath);
	void RescanAll();

	/** The All tab, when open. */
	TSharedPtr<SEELogFileTab> GetAllTab() const;

	/** Reads the files the last batch of changes touched into the All tab, in one pass, so its rows are merged once per batch rather than once per file. */
	void RefreshAllTab();

	/** Every live .log under the root: the owner's and other processes' folders, never the archive. */
	TArray<FString> ScanCurrentKeys() const;
	TArray<FString> ScanArchiveSessions() const;

	/** Path relative to the root with '/' separators ("Araf.log", "Game_20412/Araf.log", "Archive/<session>/Araf.log"). Empty when outside the root. */
	FString ToKey(const FString& FullPath) const;
	FString ToFullPath(const FString& Key) const;
	static bool IsArchiveKey(const FString& Key);
	static FString GetTitleForKey(const FString& Key);

	/** The category name when the file is one this process is writing right now (its verbosity can be changed from here). */
	FString GetLiveCategoryName(const FString& Key) const;

	TSharedPtr<FTabManager> TabManager;
	TMap<FString, FOpenLog> OpenLogs;
	TArray<FString> OpenOrder;
	TSet<FString> KnownFiles;
	FString RootDirectory;
	FDelegateHandle DirectoryWatcherHandle;
	FDelegateHandle FilesFlushedHandle;

	/** Files changed since the All tab last read them. */
	TArray<FString> PendingAllTabKeys;

	/** Automatic opens waiting for this window to be painted (an active timer only runs while it is). */
	TArray<FString> QueuedOpens;
	TSharedPtr<FActiveTimerHandle> QueuedOpenTimer;

	/** Set while the whole window closes, so the nested tabs closing with it do not count as the user closing them. */
	bool bClosing = false;

	/** Set while several tabs open at once, so the open-tab list is saved once rather than per tab. */
	bool bBatchingSaves = false;
};

#endif // WITH_EDITOR
