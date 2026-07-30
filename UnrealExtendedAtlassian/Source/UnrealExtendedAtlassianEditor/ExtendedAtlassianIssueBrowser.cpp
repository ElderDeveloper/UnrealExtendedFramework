// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianIssueBrowser.h"

#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianConnectPrompt.h"
#include "ExtendedAtlassianJira.h"
#include "ExtendedAtlassianNewIssueDialog.h"
#include "ExtendedAtlassianSettings.h"
#include "ExtendedAtlassianStyle.h"
#include "UnrealExtendedAtlassian.h"

#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformProcess.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "ExtendedAtlassianIssueBrowser"

namespace ExtendedAtlassianIssueBrowserPrivate
{
	const FName ColumnKey(TEXT("Key"));
	const FName ColumnType(TEXT("Type"));
	const FName ColumnSummary(TEXT("Summary"));
	const FName ColumnStatus(TEXT("Status"));
	const FName ColumnAssignee(TEXT("Assignee"));
	const FName ColumnPriority(TEXT("Priority"));
	const FName ColumnUpdated(TEXT("Updated"));

	const TCHAR* CustomPresetLabel = TEXT("Custom");

	/** Issues are capped so a careless JQL cannot page forever. */
	constexpr int32 MaxIssuesPerQuery = 200;

	/** How often the active timer wakes to consider polling; the real interval comes from settings. */
	constexpr float PollCheckIntervalSeconds = 15.0f;

	FLinearColor GetStatusColor(const FString& StatusCategoryKey)
	{
		if (StatusCategoryKey == TEXT("done"))
		{
			return FLinearColor(0.30f, 0.78f, 0.40f);
		}
		if (StatusCategoryKey == TEXT("indeterminate"))
		{
			return FLinearColor(0.35f, 0.62f, 0.92f);
		}
		return FLinearColor(0.70f, 0.70f, 0.70f);
	}

	void ShowNotification(const FText& Text, bool bSuccess)
	{
		FNotificationInfo Info(Text);
		Info.bFireAndForget = true;
		Info.ExpireDuration = bSuccess ? 3.0f : 6.0f;

		if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	FText FormatTimestamp(const FDateTime& In)
	{
		return In == FDateTime::MinValue()
			? FText::GetEmpty()
			: FText::FromString(In.ToString(TEXT("%Y-%m-%d %H:%M")));
	}
}

/** One row of the issue list. */
class SExtendedAtlassianIssueRow : public SMultiColumnTableRow<TSharedPtr<FExtendedAtlassianIssue>>
{
public:
	SLATE_BEGIN_ARGS(SExtendedAtlassianIssueRow) {}
		SLATE_ARGUMENT(TSharedPtr<FExtendedAtlassianIssue>, Issue)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
	{
		Issue = InArgs._Issue;
		SMultiColumnTableRow<TSharedPtr<FExtendedAtlassianIssue>>::Construct(FSuperRowType::FArguments(), InOwnerTable);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		using namespace ExtendedAtlassianIssueBrowserPrivate;

		if (!Issue.IsValid())
		{
			return SNullWidget::NullWidget;
		}

		auto MakeText = [](const FString& Text)
		{
			return SNew(STextBlock)
				.Text(FText::FromString(Text))
				.Margin(FMargin(4.0f, 2.0f));
		};

		if (ColumnName == ColumnKey)
		{
			return SNew(STextBlock)
				.Text(FText::FromString(Issue->Key))
				.Margin(FMargin(4.0f, 2.0f))
				.Font(FAppStyle::GetFontStyle(TEXT("BoldFont")));
		}
		if (ColumnName == ColumnType)
		{
			return MakeText(Issue->IssueTypeName);
		}
		if (ColumnName == ColumnSummary)
		{
			return SNew(STextBlock)
				.Text(FText::FromString(Issue->Summary))
				.Margin(FMargin(4.0f, 2.0f))
				.ToolTipText(FText::FromString(Issue->Summary));
		}
		if (ColumnName == ColumnStatus)
		{
			// Colour by status category rather than status name, so custom workflows still read correctly.
			return SNew(STextBlock)
				.Text(FText::FromString(Issue->StatusName))
				.Margin(FMargin(4.0f, 2.0f))
				.ColorAndOpacity(FSlateColor(GetStatusColor(Issue->StatusCategoryKey)));
		}
		if (ColumnName == ColumnAssignee)
		{
			return MakeText(Issue->AssigneeDisplayName.IsEmpty() ? TEXT("Unassigned") : Issue->AssigneeDisplayName);
		}
		if (ColumnName == ColumnPriority)
		{
			return MakeText(Issue->PriorityName);
		}
		if (ColumnName == ColumnUpdated)
		{
			return SNew(STextBlock)
				.Text(FormatTimestamp(Issue->Updated))
				.Margin(FMargin(4.0f, 2.0f));
		}

		return SNullWidget::NullWidget;
	}

private:
	TSharedPtr<FExtendedAtlassianIssue> Issue;
};

void SExtendedAtlassianIssueBrowser::Construct(const FArguments& InArgs)
{
	using namespace ExtendedAtlassianIssueBrowserPrivate;

	SortColumn = ColumnUpdated;

	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	if (Settings)
	{
		for (const FExtendedAtlassianJqlPreset& Preset : Settings->JqlPresets)
		{
			if (!Preset.Name.IsEmpty())
			{
				PresetNames.Add(MakeShared<FString>(Preset.Name));
			}
		}
	}
	PresetNames.Add(MakeShared<FString>(CustomPresetLabel));

	if (Settings && Settings->JqlPresets.Num() > 0)
	{
		CurrentJql = Settings->JqlPresets[0].Jql;
	}

	ChildSlot
	[
		// An unconfigured plugin is a first run, not an error: show setup instructions instead of
		// an empty list with a red message under it.
		SNew(SWidgetSwitcher)
		.WidgetIndex_Lambda([]() { return SExtendedAtlassianConnectPrompt::IsConnected() ? 1 : 0; })

		+ SWidgetSwitcher::Slot()
		[
			SNew(SExtendedAtlassianConnectPrompt)
		]

		+ SWidgetSwitcher::Slot()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f, 6.0f, 6.0f, 2.0f)
			[
				BuildQueryBar()
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(6.0f, 2.0f, 6.0f, 6.0f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)

				+ SSplitter::Slot()
				.Value(0.6f)
				[
					BuildIssueList()
				]

				+ SSplitter::Slot()
				.Value(0.4f)
				[
					BuildDetailPane()
				]
			]
		]
	];

	if (const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient())
	{
		AuthChangedHandle = Client->OnAuthStateChanged().AddSP(this, &SExtendedAtlassianIssueBrowser::HandleAuthStateChanged);
	}

	if (PresetCombo.IsValid() && PresetNames.Num() > 0)
	{
		PresetCombo->SetSelectedItem(PresetNames[0]);
	}

	// One timer that wakes regularly and decides for itself whether a poll is due, so toggling the
	// setting takes effect without reopening the tab.
	PollTimerHandle = RegisterActiveTimer(
		PollCheckIntervalSeconds,
		FWidgetActiveTimerDelegate::CreateSP(this, &SExtendedAtlassianIssueBrowser::HandlePollTimer));

	Refresh();
}

TSharedRef<SWidget> SExtendedAtlassianIssueBrowser::BuildQueryBar()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SAssignNew(PresetCombo, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&PresetNames)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
				{
					return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
				})
				.OnSelectionChanged(this, &SExtendedAtlassianIssueBrowser::OnPresetChanged)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const TSharedPtr<FString> Selected = PresetCombo.IsValid() ? PresetCombo->GetSelectedItem() : nullptr;
						return FText::FromString(Selected.IsValid() ? *Selected : FString(TEXT("Preset")));
					})
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(JqlBox, SEditableTextBox)
					.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
						TEXT("Backlot.Field")))
				.Text_Lambda([this]() { return FText::FromString(CurrentJql); })
				.HintText(LOCTEXT("JqlHint", "JQL query"))
				.OnTextChanged_Lambda([this](const FText& NewText) { CurrentJql = NewText.ToString(); })
				.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type CommitType)
				{
					if (CommitType == ETextCommit::OnEnter)
					{
						Refresh();
					}
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RefreshButton", "Refresh"))
				.IsEnabled_Lambda([this]() { return !bLoading; })
				.OnClicked_Lambda([this]()
				{
					Refresh();
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("NewIssueButton", "New Issue"))
				.ToolTipText(LOCTEXT("NewIssueTooltip", "Create a task, story or other work item in the configured Jira project."))
				.OnClicked_Lambda([this]()
				{
					OpenNewIssueDialog();
					return FReply::Handled();
				})
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SThrobber)
				.Visibility_Lambda([this]() { return bLoading ? EVisibility::Visible : EVisibility::Collapsed; })
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return StatusMessage; })
				.ColorAndOpacity_Lambda([this]()
				{
					return FSlateColor(bStatusIsError
						? FLinearColor(0.90f, 0.35f, 0.30f)
						: FLinearColor(0.70f, 0.70f, 0.70f));
				})
				.AutoWrapText(true)
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianIssueBrowser::BuildIssueList()
{
	using namespace ExtendedAtlassianIssueBrowserPrivate;

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		[
			SAssignNew(IssueListView, SListView<FIssuePtr>)
			.ListItemsSource(&Issues)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SExtendedAtlassianIssueBrowser::OnGenerateIssueRow)
			.OnSelectionChanged(this, &SExtendedAtlassianIssueBrowser::OnIssueSelectionChanged)
			.HeaderRow
			(
				SNew(SHeaderRow)

				+ SHeaderRow::Column(ColumnKey)
				.DefaultLabel(LOCTEXT("KeyColumn", "Key"))
				.FixedWidth(100.0f)
				.SortMode(this, &SExtendedAtlassianIssueBrowser::GetSortModeForColumn, ColumnKey)
				.OnSort(this, &SExtendedAtlassianIssueBrowser::OnSortModeChanged)

				+ SHeaderRow::Column(ColumnType)
				.DefaultLabel(LOCTEXT("TypeColumn", "Type"))
				.FixedWidth(90.0f)
				.SortMode(this, &SExtendedAtlassianIssueBrowser::GetSortModeForColumn, ColumnType)
				.OnSort(this, &SExtendedAtlassianIssueBrowser::OnSortModeChanged)

				+ SHeaderRow::Column(ColumnSummary)
				.DefaultLabel(LOCTEXT("SummaryColumn", "Summary"))
				.FillWidth(1.0f)
				.SortMode(this, &SExtendedAtlassianIssueBrowser::GetSortModeForColumn, ColumnSummary)
				.OnSort(this, &SExtendedAtlassianIssueBrowser::OnSortModeChanged)

				+ SHeaderRow::Column(ColumnStatus)
				.DefaultLabel(LOCTEXT("StatusColumn", "Status"))
				.FixedWidth(130.0f)
				.SortMode(this, &SExtendedAtlassianIssueBrowser::GetSortModeForColumn, ColumnStatus)
				.OnSort(this, &SExtendedAtlassianIssueBrowser::OnSortModeChanged)

				+ SHeaderRow::Column(ColumnAssignee)
				.DefaultLabel(LOCTEXT("AssigneeColumn", "Assignee"))
				.FixedWidth(140.0f)
				.SortMode(this, &SExtendedAtlassianIssueBrowser::GetSortModeForColumn, ColumnAssignee)
				.OnSort(this, &SExtendedAtlassianIssueBrowser::OnSortModeChanged)

				+ SHeaderRow::Column(ColumnPriority)
				.DefaultLabel(LOCTEXT("PriorityColumn", "Priority"))
				.FixedWidth(90.0f)
				.SortMode(this, &SExtendedAtlassianIssueBrowser::GetSortModeForColumn, ColumnPriority)
				.OnSort(this, &SExtendedAtlassianIssueBrowser::OnSortModeChanged)

				+ SHeaderRow::Column(ColumnUpdated)
				.DefaultLabel(LOCTEXT("UpdatedColumn", "Updated"))
				.FixedWidth(120.0f)
				.SortMode(this, &SExtendedAtlassianIssueBrowser::GetSortModeForColumn, ColumnUpdated)
				.OnSort(this, &SExtendedAtlassianIssueBrowser::OnSortModeChanged)
			)
		];
}

TSharedRef<SWidget> SExtendedAtlassianIssueBrowser::BuildDetailPane()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			// Header
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
				.AutoWrapText(true)
				.Text_Lambda([this]()
				{
					return SelectedIssue.IsValid()
						? FText::FromString(FString::Printf(TEXT("%s  %s"), *SelectedIssue->Key, *SelectedIssue->Summary))
						: LOCTEXT("NoSelection", "Select an issue");
				})
			]

			// Actions
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([this]() { return SelectedIssue.IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("OpenInBrowser", "Open in Browser"))
					.OnClicked_Lambda([this]()
					{
						if (SelectedIssue.IsValid())
						{
							const FString Url = FExtendedAtlassianJira::GetIssueBrowseUrl(SelectedIssue->Key);
							if (!Url.IsEmpty())
							{
								FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
							}
						}
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(TransitionCombo, SComboBox<FTransitionPtr>)
					.OptionsSource(&Transitions)
					.IsEnabled_Lambda([this]() { return !bDetailLoading && Transitions.Num() > 0; })
					.OnGenerateWidget_Lambda([](FTransitionPtr Item)
					{
						return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->Name : FString()));
					})
					.OnSelectionChanged_Lambda([this](FTransitionPtr Item, ESelectInfo::Type SelectInfo)
					{
						if (SelectInfo != ESelectInfo::Direct && Item.IsValid())
						{
							ApplyTransition(Item);
						}
					})
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							if (bDetailLoading)
							{
								return LOCTEXT("LoadingTransitions", "Loading...");
							}
							return SelectedIssue.IsValid()
								? FText::FromString(SelectedIssue->StatusName)
								: FText::GetEmpty();
						})
					]
				]
			]

			// Description
			+ SVerticalBox::Slot()
			.FillHeight(0.5f)
			.Padding(0.0f, 4.0f)
			[
				SNew(SMultiLineEditableTextBox)
					.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
						TEXT("Backlot.Field")))
				.IsReadOnly(true)
				.AllowMultiLine(true)
				.AutoWrapText(true)
				.Text_Lambda([this]()
				{
					if (!SelectedIssue.IsValid())
					{
						return FText::GetEmpty();
					}
					return SelectedIssue->Description.IsEmpty()
						? LOCTEXT("NoDescription", "(no description)")
						: FText::FromString(SelectedIssue->Description);
				})
			]

			// Comments
			+ SVerticalBox::Slot()
			.FillHeight(0.5f)
			.Padding(0.0f, 4.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.DarkGroupBorder")))
				[
					SAssignNew(CommentListView, SListView<FCommentPtr>)
					.ListItemsSource(&Comments)
					.SelectionMode(ESelectionMode::None)
					.OnGenerateRow(this, &SExtendedAtlassianIssueBrowser::OnGenerateCommentRow)
				]
			]

			// New comment
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SVerticalBox)
				.Visibility_Lambda([this]() { return SelectedIssue.IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(60.0f)
					[
						SAssignNew(CommentBox, SMultiLineEditableTextBox)
							.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
								TEXT("Backlot.Field")))
						.HintText(LOCTEXT("CommentHint", "Add a comment"))
						.AllowMultiLine(true)
						.AutoWrapText(true)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("PostComment", "Post Comment"))
					.IsEnabled_Lambda([this]() { return !bPostingComment; })
					.OnClicked_Lambda([this]()
					{
						PostComment();
						return FReply::Handled();
					})
				]
			]
		];
}

TSharedRef<ITableRow> SExtendedAtlassianIssueBrowser::OnGenerateIssueRow(FIssuePtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SExtendedAtlassianIssueRow, OwnerTable).Issue(Item);
}

TSharedRef<ITableRow> SExtendedAtlassianIssueBrowser::OnGenerateCommentRow(FCommentPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	using namespace ExtendedAtlassianIssueBrowserPrivate;

	const FString Header = Item.IsValid()
		? FString::Printf(TEXT("%s  -  %s"), *Item->AuthorDisplayName, *FormatTimestamp(Item->Created).ToString())
		: FString();

	return SNew(STableRow<FCommentPtr>, OwnerTable)
		.Padding(FMargin(4.0f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Header))
				.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item.IsValid() ? Item->Body : FString()))
				.AutoWrapText(true)
			]
		];
}

void SExtendedAtlassianIssueBrowser::OnPresetChanged(TSharedPtr<FString> NewPreset, ESelectInfo::Type SelectInfo)
{
	using namespace ExtendedAtlassianIssueBrowserPrivate;

	if (!NewPreset.IsValid() || SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	if (*NewPreset == CustomPresetLabel)
	{
		// Leave whatever is in the box; the user is about to type their own.
		return;
	}

	if (const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get())
	{
		for (const FExtendedAtlassianJqlPreset& Preset : Settings->JqlPresets)
		{
			if (Preset.Name == *NewPreset)
			{
				CurrentJql = Preset.Jql;
				Refresh();
				return;
			}
		}
	}
}

void SExtendedAtlassianIssueBrowser::OnIssueSelectionChanged(FIssuePtr Item, ESelectInfo::Type SelectInfo)
{
	SelectedIssue = Item;

	Transitions.Reset();
	Comments.Reset();

	if (CommentListView.IsValid())
	{
		CommentListView->RequestListRefresh();
	}

	if (Item.IsValid())
	{
		LoadDetailsFor(Item);
	}
}

void SExtendedAtlassianIssueBrowser::OpenNewIssueDialog()
{
	TWeakPtr<SExtendedAtlassianIssueBrowser> WeakBrowser = SharedThis(this);

	SExtendedAtlassianNewIssueDialog::Open(
		SExtendedAtlassianNewIssueDialog::FOnIssueCreated::CreateLambda(
			[WeakBrowser](const FString& IssueKey)
			{
				const TSharedPtr<SExtendedAtlassianIssueBrowser> Browser = WeakBrowser.Pin();
				if (!Browser.IsValid())
				{
					return;
				}

				Browser->PendingSelectKey = IssueKey;
				Browser->Refresh();
			}));
}

void SExtendedAtlassianIssueBrowser::Refresh()
{
	using namespace ExtendedAtlassianIssueBrowserPrivate;

	if (bLoading)
	{
		return;
	}

	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid() || !Client->IsReady())
	{
		Issues.Reset();
		if (IssueListView.IsValid())
		{
			IssueListView->RequestListRefresh();
		}

		SetStatus(LOCTEXT("NotConnected",
			"Not connected. Set the site URL and credentials in Project Settings > Plugins > Extended Atlassian."), true);
		return;
	}

	if (CurrentJql.TrimStartAndEnd().IsEmpty())
	{
		SetStatus(LOCTEXT("EmptyJql", "Enter a JQL query or pick a preset."), true);
		return;
	}

	bLoading = true;
	SetStatus(LOCTEXT("Searching", "Searching..."), false);

	TWeakPtr<SExtendedAtlassianIssueBrowser> WeakBrowser = SharedThis(this);

	FExtendedAtlassianJira::SearchIssues(CurrentJql, MaxIssuesPerQuery,
		FExtendedAtlassianIssuesDelegate::CreateLambda(
			[WeakBrowser](const FExtendedAtlassianIssueQueryResult& Result)
			{
				const TSharedPtr<SExtendedAtlassianIssueBrowser> Browser = WeakBrowser.Pin();
				if (!Browser.IsValid())
				{
					return;
				}

				Browser->bLoading = false;

				if (!Result.bSuccess)
				{
					Browser->Issues.Reset();
					if (Browser->IssueListView.IsValid())
					{
						Browser->IssueListView->RequestListRefresh();
					}

					// The query failing says nothing about the created issue, and the dialog already
					// confirmed it. Drop the pending selection rather than letting it surface against
					// some later, unrelated refresh.
					Browser->PendingSelectKey.Reset();

					Browser->SetStatus(FText::FromString(Result.Error.Message), true);
					return;
				}

				Browser->Issues.Reset();
				for (const FExtendedAtlassianIssue& Issue : Result.Issues)
				{
					Browser->Issues.Add(MakeShared<FExtendedAtlassianIssue>(Issue));
				}

				Browser->ApplySort();

				if (Browser->IssueListView.IsValid())
				{
					Browser->IssueListView->RequestListRefresh();
				}

				if (Browser->Issues.Num() == 0)
				{
					Browser->SetStatus(LOCTEXT("NoResults", "No issues matched."), false);
				}
				else if (Result.bTruncated)
				{
					Browser->SetStatus(FText::Format(
						LOCTEXT("ResultsTruncated", "Showing the first {0} issues; more matched. Narrow the query to see the rest."),
						FText::AsNumber(Browser->Issues.Num())), false);
				}
				else
				{
					Browser->SetStatus(FText::Format(
						LOCTEXT("ResultsCount", "{0} issue(s)."),
						FText::AsNumber(Browser->Issues.Num())), false);
				}

				Browser->SelectPendingIssue();
			}));
}

void SExtendedAtlassianIssueBrowser::SelectPendingIssue()
{
	if (PendingSelectKey.IsEmpty())
	{
		return;
	}

	const FString Key = MoveTemp(PendingSelectKey);
	PendingSelectKey.Reset();

	const FIssuePtr* Found = Issues.FindByPredicate(
		[&Key](const FIssuePtr& Issue) { return Issue.IsValid() && Issue->Key == Key; });

	if (!Found)
	{
		// Creating an unassigned task while "My open issues" is selected lands here, which is the
		// ordinary case rather than a failure. Say where it went instead of leaving the list looking
		// like nothing happened.
		SetStatus(FText::Format(
			LOCTEXT("CreatedOutsideQuery", "{0} created. It does not match the current query, so it is not listed."),
			FText::FromString(Key)), false);
		return;
	}

	if (IssueListView.IsValid())
	{
		IssueListView->SetSelection(*Found);
		IssueListView->RequestScrollIntoView(*Found);
	}
}

void SExtendedAtlassianIssueBrowser::LoadDetailsFor(FIssuePtr Issue)
{
	if (!Issue.IsValid())
	{
		return;
	}

	bDetailLoading = true;

	const FString IssueKey = Issue->Key;
	TWeakPtr<SExtendedAtlassianIssueBrowser> WeakBrowser = SharedThis(this);

	FExtendedAtlassianJira::GetTransitions(IssueKey,
		FExtendedAtlassianTransitionsDelegate::CreateLambda(
			[WeakBrowser, IssueKey](bool bSuccess, const TArray<FExtendedAtlassianTransition>& InTransitions, const FExtendedAtlassianError& Error)
			{
				const TSharedPtr<SExtendedAtlassianIssueBrowser> Browser = WeakBrowser.Pin();
				if (!Browser.IsValid())
				{
					return;
				}

				Browser->bDetailLoading = false;

				// The selection may have moved on while this was in flight.
				if (!Browser->SelectedIssue.IsValid() || Browser->SelectedIssue->Key != IssueKey)
				{
					return;
				}

				Browser->Transitions.Reset();
				if (bSuccess)
				{
					for (const FExtendedAtlassianTransition& Transition : InTransitions)
					{
						Browser->Transitions.Add(MakeShared<FExtendedAtlassianTransition>(Transition));
					}
				}

				if (Browser->TransitionCombo.IsValid())
				{
					Browser->TransitionCombo->RefreshOptions();
				}
			}));

	FExtendedAtlassianJira::GetComments(IssueKey,
		FExtendedAtlassianCommentsDelegate::CreateLambda(
			[WeakBrowser, IssueKey](bool bSuccess, const TArray<FExtendedAtlassianComment>& InComments, const FExtendedAtlassianError& Error)
			{
				const TSharedPtr<SExtendedAtlassianIssueBrowser> Browser = WeakBrowser.Pin();
				if (!Browser.IsValid())
				{
					return;
				}

				if (!Browser->SelectedIssue.IsValid() || Browser->SelectedIssue->Key != IssueKey)
				{
					return;
				}

				Browser->Comments.Reset();
				if (bSuccess)
				{
					for (const FExtendedAtlassianComment& Comment : InComments)
					{
						Browser->Comments.Add(MakeShared<FExtendedAtlassianComment>(Comment));
					}
				}

				if (Browser->CommentListView.IsValid())
				{
					Browser->CommentListView->RequestListRefresh();
				}
			}));
}

void SExtendedAtlassianIssueBrowser::ApplyTransition(FTransitionPtr Transition)
{
	if (!Transition.IsValid() || !SelectedIssue.IsValid())
	{
		return;
	}

	const FString IssueKey = SelectedIssue->Key;

	// Apply optimistically so the UI responds immediately, and remember enough to undo it.
	const FString PreviousStatus = SelectedIssue->StatusName;
	const FString PreviousCategory = SelectedIssue->StatusCategoryKey;

	SelectedIssue->StatusName = Transition->ToStatusName;
	SelectedIssue->StatusCategoryKey = Transition->ToStatusCategoryKey;

	if (IssueListView.IsValid())
	{
		IssueListView->RequestListRefresh();
	}

	TWeakPtr<SExtendedAtlassianIssueBrowser> WeakBrowser = SharedThis(this);
	const FString TransitionName = Transition->Name;

	FExtendedAtlassianJira::TransitionIssue(IssueKey, Transition->Id,
		FExtendedAtlassianActionDelegate::CreateLambda(
			[WeakBrowser, IssueKey, PreviousStatus, PreviousCategory, TransitionName](bool bSuccess, const FExtendedAtlassianError& Error)
			{
				using namespace ExtendedAtlassianIssueBrowserPrivate;

				const TSharedPtr<SExtendedAtlassianIssueBrowser> Browser = WeakBrowser.Pin();
				if (!Browser.IsValid())
				{
					return;
				}

				if (bSuccess)
				{
					ShowNotification(FText::Format(
						LOCTEXT("TransitionApplied", "{0} moved to {1}."),
						FText::FromString(IssueKey),
						FText::FromString(TransitionName)), true);

					// Re-read transitions: the available set changes with the new status.
					if (Browser->SelectedIssue.IsValid() && Browser->SelectedIssue->Key == IssueKey)
					{
						Browser->LoadDetailsFor(Browser->SelectedIssue);
					}
					return;
				}

				// Roll the optimistic change back on whichever row still holds this issue.
				for (const FIssuePtr& Issue : Browser->Issues)
				{
					if (Issue.IsValid() && Issue->Key == IssueKey)
					{
						Issue->StatusName = PreviousStatus;
						Issue->StatusCategoryKey = PreviousCategory;
						break;
					}
				}

				if (Browser->IssueListView.IsValid())
				{
					Browser->IssueListView->RequestListRefresh();
				}

				ShowNotification(FText::Format(
					LOCTEXT("TransitionFailed", "Could not move {0}: {1}"),
					FText::FromString(IssueKey),
					FText::FromString(Error.Message)), false);
			}));
}

void SExtendedAtlassianIssueBrowser::PostComment()
{
	if (!SelectedIssue.IsValid() || !CommentBox.IsValid() || bPostingComment)
	{
		return;
	}

	const FString Text = CommentBox->GetText().ToString().TrimStartAndEnd();
	if (Text.IsEmpty())
	{
		return;
	}

	bPostingComment = true;

	const FString IssueKey = SelectedIssue->Key;
	TWeakPtr<SExtendedAtlassianIssueBrowser> WeakBrowser = SharedThis(this);

	FExtendedAtlassianJira::AddComment(IssueKey, Text,
		FExtendedAtlassianActionDelegate::CreateLambda(
			[WeakBrowser, IssueKey](bool bSuccess, const FExtendedAtlassianError& Error)
			{
				using namespace ExtendedAtlassianIssueBrowserPrivate;

				const TSharedPtr<SExtendedAtlassianIssueBrowser> Browser = WeakBrowser.Pin();
				if (!Browser.IsValid())
				{
					return;
				}

				Browser->bPostingComment = false;

				if (!bSuccess)
				{
					ShowNotification(FText::Format(
						LOCTEXT("CommentFailed", "Could not post the comment: {0}"),
						FText::FromString(Error.Message)), false);
					return;
				}

				// Only clear the box once Jira has accepted it, so a failure never loses typing.
				if (Browser->CommentBox.IsValid())
				{
					Browser->CommentBox->SetText(FText::GetEmpty());
				}

				if (Browser->SelectedIssue.IsValid() && Browser->SelectedIssue->Key == IssueKey)
				{
					Browser->LoadDetailsFor(Browser->SelectedIssue);
				}
			}));
}

EColumnSortMode::Type SExtendedAtlassianIssueBrowser::GetSortModeForColumn(FName ColumnId) const
{
	return SortColumn == ColumnId ? SortMode : EColumnSortMode::None;
}

void SExtendedAtlassianIssueBrowser::OnSortModeChanged(EColumnSortPriority::Type Priority, const FName& ColumnId, EColumnSortMode::Type NewMode)
{
	SortColumn = ColumnId;
	SortMode = NewMode;

	ApplySort();

	if (IssueListView.IsValid())
	{
		IssueListView->RequestListRefresh();
	}
}

void SExtendedAtlassianIssueBrowser::ApplySort()
{
	using namespace ExtendedAtlassianIssueBrowserPrivate;

	const bool bAscending = SortMode != EColumnSortMode::Descending;
	const FName Column = SortColumn;

	Issues.Sort([bAscending, Column](const FIssuePtr& A, const FIssuePtr& B)
	{
		if (!A.IsValid() || !B.IsValid())
		{
			return false;
		}

		int32 Comparison = 0;

		if (Column == ColumnKey)
		{
			Comparison = A->Key.Compare(B->Key);
		}
		else if (Column == ColumnType)
		{
			Comparison = A->IssueTypeName.Compare(B->IssueTypeName);
		}
		else if (Column == ColumnSummary)
		{
			Comparison = A->Summary.Compare(B->Summary);
		}
		else if (Column == ColumnStatus)
		{
			Comparison = A->StatusName.Compare(B->StatusName);
		}
		else if (Column == ColumnAssignee)
		{
			Comparison = A->AssigneeDisplayName.Compare(B->AssigneeDisplayName);
		}
		else if (Column == ColumnPriority)
		{
			Comparison = A->PriorityName.Compare(B->PriorityName);
		}
		else if (Column == ColumnUpdated)
		{
			if (A->Updated == B->Updated)
			{
				Comparison = 0;
			}
			else
			{
				Comparison = A->Updated < B->Updated ? -1 : 1;
			}
		}

		return bAscending ? Comparison < 0 : Comparison > 0;
	});
}

EActiveTimerReturnType SExtendedAtlassianIssueBrowser::HandlePollTimer(double InCurrentTime, float InDeltaTime)
{
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();

	if (Settings && Settings->bEnablePolling && !bLoading)
	{
		const double Interval = static_cast<double>(FMath::Max(60, Settings->PollIntervalSeconds));
		if (InCurrentTime - LastPollTime >= Interval)
		{
			LastPollTime = InCurrentTime;
			Refresh();
		}
	}
	else if (!Settings || !Settings->bEnablePolling)
	{
		// Keep the clock current so enabling polling does not fire an immediate extra request.
		LastPollTime = InCurrentTime;
	}

	return EActiveTimerReturnType::Continue;
}

void SExtendedAtlassianIssueBrowser::HandleAuthStateChanged()
{
	// Credentials just became usable and nothing has been fetched yet: fill the tab.
	if (Issues.Num() == 0 && !bLoading)
	{
		Refresh();
	}
}

SExtendedAtlassianIssueBrowser::~SExtendedAtlassianIssueBrowser()
{
	if (const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient())
	{
		Client->OnAuthStateChanged().Remove(AuthChangedHandle);
	}
}

void SExtendedAtlassianIssueBrowser::SetStatus(const FText& Message, bool bIsError)
{
	StatusMessage = Message;
	bStatusIsError = bIsError;
}

#undef LOCTEXT_NAMESPACE
