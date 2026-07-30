// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianNewIssueDialog.h"

#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianJira.h"
#include "ExtendedAtlassianSettings.h"
#include "ExtendedAtlassianStyle.h"
#include "UnrealExtendedAtlassian.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformProcess.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ExtendedAtlassianNewIssueDialog"

namespace ExtendedAtlassianNewIssuePrivate
{
	/**
	 * Pre-selected issue type.
	 *
	 * Only a preference: the combo lists everything the project offers, and a project without a
	 * Task type falls back to its first type rather than sending one Jira would reject.
	 */
	const TCHAR* PreferredIssueTypeName = TEXT("Task");

	void NotifyCreated(const FString& IssueKey)
	{
		FNotificationInfo Info(FText::Format(
			LOCTEXT("IssueCreated", "Created {0}"),
			FText::FromString(IssueKey)));

		const FString Url = FExtendedAtlassianJira::GetIssueBrowseUrl(IssueKey);
		if (!Url.IsEmpty())
		{
			Info.HyperlinkText = LOCTEXT("OpenIssueLink", "Open in browser");
			Info.Hyperlink = FSimpleDelegate::CreateLambda([Url]()
			{
				FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
			});
		}

		Info.bFireAndForget = true;
		Info.ExpireDuration = 8.0f;

		if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(SNotificationItem::CS_Success);
		}
	}
}

void SExtendedAtlassianNewIssueDialog::Open(FOnIssueCreated OnCreated)
{
	const TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "New Jira Issue"))
		.ClientSize(FVector2D(640.0f, 480.0f))
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	Window->SetContent(
		SNew(SExtendedAtlassianNewIssueDialog)
		.ParentWindow(Window)
		.OnIssueCreated(OnCreated));

	FSlateApplication::Get().AddWindow(Window);
}

void SExtendedAtlassianNewIssueDialog::Construct(const FArguments& InArgs)
{
	using namespace ExtendedAtlassianNewIssuePrivate;

	ParentWindow = InArgs._ParentWindow;
	OnIssueCreated = InArgs._OnIssueCreated;

	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();

	// The browsing project, not the bug project: this is ordinary work, not a defect report.
	ProjectKey = Settings ? Settings->ProjectKey.TrimStartAndEnd() : FString();

	Fields = MakeShared<FExtendedAtlassianIssueFields>();

	auto MakeLabel = [](const FText& Text)
	{
		return SNew(STextBlock).Text(Text).Font(FAppStyle::GetFontStyle(TEXT("BoldFont")));
	};

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(10.0f)
		[
			SNew(SVerticalBox)

			// --- Target project ---
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(ProjectKey.IsEmpty()
					? LOCTEXT("NoProject", "No Jira project is configured.")
					: FText::Format(LOCTEXT("TargetProject", "Filing into project {0}"), FText::FromString(ProjectKey)))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.70f, 0.70f)))
			]

			// --- Summary ---
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeLabel(LOCTEXT("SummaryLabel", "Summary"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 8.0f)
			[
				SAssignNew(SummaryBox, SEditableTextBox)
					.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
						TEXT("Backlot.Field")))
				.HintText(LOCTEXT("SummaryHint", "One line describing the work"))
				.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type CommitType)
				{
					if (CommitType == ETextCommit::OnEnter)
					{
						Submit();
					}
				})
			]

			// --- Description ---
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeLabel(LOCTEXT("DescriptionLabel", "Description"))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 2.0f, 0.0f, 8.0f)
			[
				SNew(SBox)
				.MinDesiredHeight(140.0f)
				[
					SAssignNew(DescriptionBox, SMultiLineEditableTextBox)
						.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
							TEXT("Backlot.Field")))
					.HintText(LOCTEXT("DescriptionHint", "What needs doing, and what done looks like"))
					.AllowMultiLine(true)
					.AutoWrapText(true)
				]
			]

			// --- Type / priority ---
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(0.5f)
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ MakeLabel(LOCTEXT("TypeLabel", "Issue Type")) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						SAssignNew(IssueTypeCombo, SComboBox<FIssueTypePtr>)
						.OptionsSource(&Fields->IssueTypes)
						.OnGenerateWidget_Lambda([](FIssueTypePtr Item)
						{
							return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->Name : FString()));
						})
						.OnSelectionChanged_Lambda([this](FIssueTypePtr Item, ESelectInfo::Type)
						{
							Fields->SelectedIssueType = Item;
						})
						[
							SNew(STextBlock)
							.Text_Lambda([this]() { return Fields->GetIssueTypeLabel(); })
						]
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.5f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ MakeLabel(LOCTEXT("PriorityLabel", "Priority")) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						SAssignNew(PriorityCombo, SComboBox<FPriorityPtr>)
						.OptionsSource(&Fields->Priorities)
						.OnGenerateWidget_Lambda([](FPriorityPtr Item)
						{
							return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->Name : FString()));
						})
						.OnSelectionChanged_Lambda([this](FPriorityPtr Item, ESelectInfo::Type)
						{
							Fields->SelectedPriority = Item;
						})
						[
							SNew(STextBlock)
							.Text_Lambda([this]() { return Fields->GetPriorityLabel(); })
						]
					]
				]
			]

			// --- Labels ---
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeLabel(LOCTEXT("LabelsLabel", "Labels"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SAssignNew(LabelsBox, SEditableTextBox)
					.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
						TEXT("Backlot.Field")))
				.HintText(LOCTEXT("LabelsHint", "Comma separated; spaces become hyphens"))
			]
		]

		// --- Status + buttons ---
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f, 4.0f, 10.0f, 10.0f)
		[
			SNew(SHorizontalBox)

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

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Cancel", "Cancel"))
				.IsEnabled_Lambda([this]() { return !bSubmitting; })
				.OnClicked_Lambda([this]()
				{
					CloseWindow();
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Submit", "Create Issue"))
				.IsEnabled_Lambda([this]() { return !bSubmitting; })
				.OnClicked_Lambda([this]()
				{
					Submit();
					return FReply::Handled();
				})
			]
		]
	];

	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid() || !Client->IsReady())
	{
		SetStatus(LOCTEXT("NotConnectedStatus",
			"Not connected to Atlassian. Set the site URL and credentials in Project Settings > Plugins > Extended Atlassian."), true);
		return;
	}

	TWeakPtr<SExtendedAtlassianNewIssueDialog> WeakDialog = SharedThis(this);

	Fields->Load(ProjectKey, PreferredIssueTypeName, Settings ? Settings->DefaultPriorityName : FString(),
		[WeakDialog]()
		{
			const TSharedPtr<SExtendedAtlassianNewIssueDialog> Dialog = WeakDialog.Pin();
			if (!Dialog.IsValid())
			{
				return;
			}

			if (Dialog->IssueTypeCombo.IsValid())
			{
				Dialog->IssueTypeCombo->RefreshOptions();
			}
			if (Dialog->PriorityCombo.IsValid())
			{
				Dialog->PriorityCombo->RefreshOptions();
			}
		});
}

void SExtendedAtlassianNewIssueDialog::Submit()
{
	using namespace ExtendedAtlassianNewIssuePrivate;

	if (bSubmitting)
	{
		return;
	}

	const FString Summary = SummaryBox.IsValid() ? SummaryBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (Summary.IsEmpty())
	{
		SetStatus(LOCTEXT("NeedSummary", "Enter a summary."), true);
		return;
	}

	if (!Fields->HasIssueType())
	{
		SetStatus(LOCTEXT("NeedIssueType", "Pick an issue type."), true);
		return;
	}

	if (ProjectKey.IsEmpty())
	{
		SetStatus(LOCTEXT("NeedProject", "Set a Jira project key in Project Settings > Plugins > Extended Atlassian."), true);
		return;
	}

	FExtendedAtlassianNewIssue NewIssue;
	NewIssue.ProjectKey = ProjectKey;
	NewIssue.IssueTypeName = Fields->GetIssueTypeNameToSubmit();
	NewIssue.Summary = Summary;
	NewIssue.Description = DescriptionBox.IsValid() ? DescriptionBox->GetText().ToString() : FString();
	NewIssue.PriorityName = Fields->GetPriorityNameToSubmit();

	if (LabelsBox.IsValid())
	{
		NewIssue.Labels = FExtendedAtlassianJira::ParseLabels(LabelsBox->GetText().ToString());
	}

	bSubmitting = true;
	SetStatus(LOCTEXT("Creating", "Creating issue..."), false);

	TWeakPtr<SExtendedAtlassianNewIssueDialog> WeakDialog = SharedThis(this);

	FExtendedAtlassianJira::CreateIssue(NewIssue,
		FExtendedAtlassianCreateIssueDelegate::CreateLambda(
			[WeakDialog](bool bSuccess, const FString& IssueKey, const FExtendedAtlassianError& Error)
			{
				const TSharedPtr<SExtendedAtlassianNewIssueDialog> Dialog = WeakDialog.Pin();

				if (!bSuccess)
				{
					// Keep the dialog open so nothing the user typed is lost.
					if (Dialog.IsValid())
					{
						Dialog->bSubmitting = false;
						Dialog->SetStatus(FText::Format(
							LOCTEXT("CreateFailed", "Could not create the issue: {0}"),
							FText::FromString(Error.Message)), true);
					}
					return;
				}

				NotifyCreated(IssueKey);

				if (Dialog.IsValid())
				{
					Dialog->OnIssueCreated.ExecuteIfBound(IssueKey);
					Dialog->CloseWindow();
				}
			}));
}

void SExtendedAtlassianNewIssueDialog::CloseWindow()
{
	if (const TSharedPtr<SWindow> Window = ParentWindow.Pin())
	{
		Window->RequestDestroyWindow();
	}
}

void SExtendedAtlassianNewIssueDialog::SetStatus(const FText& Message, bool bIsError)
{
	StatusMessage = Message;
	bStatusIsError = bIsError;
}

#undef LOCTEXT_NAMESPACE
