// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "EELogFileParser.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/SlateColor.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SDockTab;
class SHeaderRow;
template <typename OptionType> class SComboBox;

/**
 * One tab of the Extended Log window: follows its file, parses it into rows, and shows them in a
 * filterable, virtualized list.
 *
 * A merged tab (the "All" tab) follows several files instead and interleaves their rows by the time
 * they were written, with a Category column saying which file each row came from. The markers every
 * file carries (run separators, session headers) show once.
 */
class SEELogFileTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEELogFileTab)
		: _MaxRows(100000)
		, _LoadChunkBytes(32 * 1024 * 1024)
		, _Merged(false)
	{}
		/** The file; for a merged tab, the folder its files are in ("Show in folder"). */
		SLATE_ARGUMENT(FString, FilePath)
		SLATE_ARGUMENT(FString, Title)
		SLATE_ARGUMENT(int32, MaxRows)
		SLATE_ARGUMENT(int64, LoadChunkBytes)
		/** Starts with no files; AddSource adds them. */
		SLATE_ARGUMENT(bool, Merged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SEELogFileTab() override;

	/** The dock tab this sits in: used to tell whether new rows were seen, and for its label and icon. */
	void SetOwnerTab(const TSharedRef<SDockTab>& InOwnerTab);

	/** Reads whatever the file (every file, for a merged tab) gained since the last call. */
	void Refresh();

	/** Merged tab: follows one more file (nothing happens if it already does) and reads it at once. Key is its path relative to the log folder. */
	void AddSource(const FString& Key, const FString& Path, const FString& SourceTitle);

	/** Merged tab: reads the files among Keys, which just changed. */
	void RefreshSources(const TArray<FString>& Keys);

	bool IsMerged() const { return bMerged; }

	/** The file was deleted or moved away; the rows already read stay. */
	void MarkMissing();

	/** Hides every row read so far. The file is not touched. */
	void ClearView();

	FText GetTabLabel() const;
	const FSlateBrush* GetTabIcon() const;
	void HandleTabActivated(TSharedRef<SDockTab> Tab, ETabActivationCause Cause);

	const FString& GetFilePath() const { return FilePath; }

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	using FRowPtr = TSharedPtr<FEELogRow>;

	/** One followed file. A file tab has exactly one; a merged tab one per file, never removed, so a row's SourceIndex stays valid. */
	struct FSource
	{
		FString Key;
		FString Path;
		FString Title;
		FSlateColor Color;
		TUniquePtr<FEELogFileTail> Tail;
		FEELogLineParser Parser;

		/** Line and Raw rows currently held from this file. */
		int32 RowCount = 0;

		/** Merged tab: unticked in the eye menu. */
		bool bShown = true;
	};

	/** Error, Warning, Display, Log, and Verbose (with VeryVerbose). */
	static constexpr int32 BucketCount = 5;
	static int32 GetBucket(EEFLogVerbosity Verbosity);

	TSharedRef<SWidget> BuildToolbar();

	/**
	 * The eye menu beside the search box, staying open while you toggle, like the viewport's Show
	 * menu. Levels (with live counts) filter this tab; columns (Message always shows) are shared by
	 * every tab and remembered per user. A merged tab also lists its files, remembered per user.
	 */
	TSharedRef<SWidget> BuildViewMenu();
	void ToggleLevel(int32 Bucket);
	bool IsLevelShown(int32 Bucket) const;
	void ToggleColumn(FName InColumnId);
	bool IsColumnShown(FName InColumnId) const;
	void ToggleSource(int32 SourceIndex);
	bool IsSourceShownAt(int32 SourceIndex) const;

	/** Every level, column (and, merged, file) shown again. */
	void UseDefaults();

	/** Remembers which files a merged tab hides, keeping entries for files not written this session. */
	void SaveHiddenSources() const;

	/** Structure rows (markers) belong to every file, so they never hide with one. */
	bool IsRowSourceShown(const FEELogRow& Row) const;

	/** The level counts follow the files shown. */
	void RecountBuckets();

	/** Appends one file's new rows to OutRows, stamped with its index. */
	void ReadSource(int32 SourceIndex, TArray<FRowPtr>& OutRows);

	/** The header changed (eye menu or header right-click): remember it and tell the other tabs. */
	void HandleHiddenColumnsChanged();

	/** Brings this tab's header in line with the shared setting. */
	void ApplyHiddenColumnsSetting();
	TSharedRef<ITableRow> GenerateRow(FRowPtr Row, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedPtr<SWidget> BuildRowContextMenu();

	/** Takes new rows in: stamps, counts and places them (by time, in a merged tab), then updates the view. */
	void AddRows(TArray<FRowPtr>&& NewRows);

	/** Merged tab: drops rows older than MergedWindowStartTicks (a long file was read from a later point than the others). */
	void PruneBeforeMergedWindow();

	/** Drops the oldest rows past the cap. True when anything went. */
	bool TrimToCap();
	void RebuildFilter();
	bool PassesFilter(const FEELogRow& Row) const;
	void AddInstanceOption(const FString& Instance);
	void ScrollToEndIfFollowing();
	void HandleListScrolled(double Offset);
	void LoadEarlier();
	bool IsViewed() const;
	void MarkViewed();

	void OpenSource(const FEELogRow& Row) const;
	void CopySelection(bool bMessagesOnly) const;

	FString FilePath;
	FString Title;
	int32 MaxRows = 100000;
	int64 LoadChunkBytes = 0;
	bool bMerged = false;

	TArray<TUniquePtr<FSource>> Sources;
	TArray<FRowPtr> Rows;
	TArray<FRowPtr> FilteredRows;
	TSharedPtr<SListView<FRowPtr>> ListView;
	TSharedPtr<SHeaderRow> HeaderRow;
	TWeakPtr<SDockTab> OwnerTab;

	FDelegateHandle HiddenColumnsChangedHandle;

	/** Set while this tab applies the shared setting, so that change is not taken for a new user choice. */
	bool bApplyingColumnSetting = false;

	// Filters.
	FString FilterText;
	bool bShowBucket[BucketCount] = { true, true, true, true, true };
	FString SelectedInstance;
	TArray<TSharedPtr<FString>> InstanceOptions;
	TSet<FString> KnownInstances;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> InstanceCombo;
	bool bCurrentRunOnly = false;
	uint64 ClearedBeforeSequence = 0;

	/** Arrival order in this tab, which is what "Clear view" cuts at (a merged tab's rows come from several parsers). */
	uint64 NextArrivalSequence = 1;

	/** File tab: the file's latest run. */
	int32 LatestRunIndex = 0;

	/** Merged tab: when the latest run started; its files number their runs differently, so time decides. */
	int64 CurrentRunStartTicks = 0;

	/** Merged tab: rows before this are not shown, because some file's history is not read that far back. */
	int64 MergedWindowStartTicks = 0;

	/** Merged tab: markers already shown, so each shows once rather than once per file. */
	TSet<FString> SeenStructureKeys;

	// Counts and unread state.
	int32 BucketCounts[BucketCount] = { 0, 0, 0, 0, 0 };
	int32 UnreadCount = 0;
	int32 UnreadWarnings = 0;
	int32 UnreadErrors = 0;
	int32 LoadedEarlierRows = 0;
	bool bTrimmed = false;
	bool bMissing = false;

	// Follow tail.
	bool bFollowTail = true;
	double LastScrollOffset = 0.0;
	double PendingTrimCompensation = 0.0;
};

#endif // WITH_EDITOR
