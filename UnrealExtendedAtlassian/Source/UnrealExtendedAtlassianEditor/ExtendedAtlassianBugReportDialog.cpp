// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianBugReportDialog.h"

#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianJira.h"
#include "ExtendedAtlassianScreenshot.h"
#include "ExtendedAtlassianSettings.h"
#include "UnrealExtendedAtlassian.h"

#include "Brushes/SlateDynamicImageBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Guid.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ExtendedAtlassianBugReportDialog"

namespace ExtendedAtlassianBugReportPrivate
{
	constexpr float ThumbnailMaxWidth = 320.0f;

	/**
	 * Survives the dialog.
	 *
	 * Once Jira has accepted the issue the dialog closes immediately, but the attachments are still
	 * uploading. Everything they need is snapshotted here so nothing depends on the widget.
	 */
	struct FSubmitState
	{
		FString IssueKey;
		TArray<uint8> ScreenshotPng;
		FString LogTail;
		TArray<FString> Failures;
	};

	void FinishSubmit(TSharedRef<FSubmitState> State)
	{
		const FString Url = FExtendedAtlassianJira::GetIssueBrowseUrl(State->IssueKey);
		const bool bHadFailures = State->Failures.Num() > 0;

		FNotificationInfo Info(FText::Format(
			LOCTEXT("IssueCreated", "Created {0}"),
			FText::FromString(State->IssueKey)));

		if (bHadFailures)
		{
			// The issue itself is fine; say exactly what did not make it rather than implying failure.
			Info.SubText = FText::Format(
				LOCTEXT("AttachmentFailed", "Issue created, but these attachments failed: {0}"),
				FText::FromString(FString::Join(State->Failures, TEXT("; "))));
		}

		if (!Url.IsEmpty())
		{
			Info.HyperlinkText = LOCTEXT("OpenIssueLink", "Open in browser");
			Info.Hyperlink = FSimpleDelegate::CreateLambda([Url]()
			{
				FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
			});
		}

		Info.bFireAndForget = true;
		Info.ExpireDuration = 10.0f;

		if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bHadFailures ? SNotificationItem::CS_Fail : SNotificationItem::CS_Success);
		}
	}

	void UploadLogThenFinish(TSharedRef<FSubmitState> State)
	{
		if (State->LogTail.IsEmpty())
		{
			FinishSubmit(State);
			return;
		}

		const FTCHARToUTF8 Utf8(*State->LogTail);

		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());

		FExtendedAtlassianJira::AddAttachment(State->IssueKey, TEXT("EditorLogTail.txt"), TEXT("text/plain"), Bytes,
			FExtendedAtlassianActionDelegate::CreateLambda(
				[State](bool bSuccess, const FExtendedAtlassianError& Error)
				{
					if (!bSuccess)
					{
						State->Failures.Add(FString::Printf(TEXT("log (%s)"), *Error.Message));
					}
					FinishSubmit(State);
				}));
	}

	void UploadAttachments(TSharedRef<FSubmitState> State)
	{
		if (State->ScreenshotPng.Num() == 0)
		{
			UploadLogThenFinish(State);
			return;
		}

		FExtendedAtlassianJira::AddAttachment(State->IssueKey, TEXT("Screenshot.png"), TEXT("image/png"), State->ScreenshotPng,
			FExtendedAtlassianActionDelegate::CreateLambda(
				[State](bool bSuccess, const FExtendedAtlassianError& Error)
				{
					if (!bSuccess)
					{
						State->Failures.Add(FString::Printf(TEXT("screenshot (%s)"), *Error.Message));
					}
					UploadLogThenFinish(State);
				}));
	}
}

void SExtendedAtlassianBugReportDialog::Open()
{
	using namespace ExtendedAtlassianBugReportPrivate;

	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();

	// Capture first: once the window is up it would be in the shot.
	TArray<uint8> Png;
	FIntPoint Size = FIntPoint::ZeroValue;
	if (!Settings || Settings->bCaptureScreenshot)
	{
		FExtendedAtlassianScreenshot::CaptureActiveViewport(Png, Size);
	}

	const FExtendedAtlassianCapturedContext Captured = FExtendedAtlassianContextCapture::Capture();

	const TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Report a Bug to Jira"))
		.ClientSize(FVector2D(720.0f, 820.0f))
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	Window->SetContent(
		SNew(SExtendedAtlassianBugReportDialog)
		.ParentWindow(Window)
		.CapturedContext(Captured)
		.ScreenshotPng(Png)
		.ScreenshotSize(Size));

	FSlateApplication::Get().AddWindow(Window);
}

void SExtendedAtlassianBugReportDialog::Construct(const FArguments& InArgs)
{
	using namespace ExtendedAtlassianBugReportPrivate;

	ParentWindow = InArgs._ParentWindow;
	CapturedContext = InArgs._CapturedContext;
	ScreenshotPng = InArgs._ScreenshotPng;
	ScreenshotSize = InArgs._ScreenshotSize;

	Fields = MakeShared<FExtendedAtlassianIssueFields>();

	const bool bHasScreenshot = ScreenshotPng.Num() > 0 && ScreenshotSize.X > 0 && ScreenshotSize.Y > 0;
	bIncludeScreenshot = bHasScreenshot;

	if (const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get())
	{
		bIncludeLogTail = Settings->bCaptureLogTail;
	}

	if (bHasScreenshot)
	{
		const float Scale = FMath::Min(1.0f, ThumbnailMaxWidth / static_cast<float>(ScreenshotSize.X));
		const FVector2D DisplaySize(ScreenshotSize.X * Scale, ScreenshotSize.Y * Scale);

		ThumbnailBrush = FSlateDynamicImageBrush::CreateWithImageData(
			FName(*FString::Printf(TEXT("ExtendedAtlassianShot_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))),
			DisplaySize,
			ScreenshotPng);
	}

	auto MakeLabel = [](const FText& Text)
	{
		return SNew(STextBlock).Text(Text).Font(FAppStyle::GetFontStyle(TEXT("BoldFont")));
	};

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBox)

			+ SScrollBox::Slot()
			.Padding(10.0f)
			[
				SNew(SVerticalBox)

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
					.HintText(LOCTEXT("SummaryHint", "One line describing what went wrong"))
				]

				// --- Description ---
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeLabel(LOCTEXT("DescriptionLabel", "Description"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f, 0.0f, 8.0f)
				[
					SNew(SBox)
					.HeightOverride(120.0f)
					[
						SAssignNew(DescriptionBox, SMultiLineEditableTextBox)
						.HintText(LOCTEXT("DescriptionHint", "Steps to reproduce, expected vs actual"))
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
				.Padding(0.0f, 2.0f, 0.0f, 8.0f)
				[
					SAssignNew(LabelsBox, SEditableTextBox)
					.HintText(LOCTEXT("LabelsHint", "Comma separated; spaces become hyphens"))
				]

				// --- What to attach ---
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeLabel(LOCTEXT("AttachLabel", "Attach"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f, 0.0f, 8.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SCheckBox)
						.IsEnabled(bHasScreenshot)
						.IsChecked_Lambda([this]() { return bIncludeScreenshot ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bIncludeScreenshot = State == ECheckBoxState::Checked; })
						[
							SNew(STextBlock)
							.Text(bHasScreenshot
								? FText::Format(
									LOCTEXT("AttachScreenshot", "Screenshot ({0} x {1}, {2} KB)"),
									FText::AsNumber(ScreenshotSize.X),
									FText::AsNumber(ScreenshotSize.Y),
									FText::AsNumber(ScreenshotPng.Num() / 1024))
								: LOCTEXT("NoScreenshot", "Screenshot (no viewport was available to capture)"))
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return bIncludeLogTail ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bIncludeLogTail = State == ECheckBoxState::Checked; })
						[
							SNew(STextBlock).Text(LOCTEXT("AttachLog", "Tail of the editor log"))
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return bIncludeContext ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bIncludeContext = State == ECheckBoxState::Checked; })
						[
							SNew(STextBlock).Text(LOCTEXT("AttachContext", "Editor context in the description"))
						]
					]
				]

				// --- Thumbnail ---
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 8.0f)
				[
					SNew(SImage)
					.Visibility(ThumbnailBrush.IsValid() ? EVisibility::Visible : EVisibility::Collapsed)
					.Image(ThumbnailBrush.IsValid() ? ThumbnailBrush.Get() : nullptr)
				]

				// --- Captured context preview ---
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeLabel(LOCTEXT("ContextLabel", "Captured Context"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.HeightOverride(140.0f)
					[
						SNew(SMultiLineEditableTextBox)
						.IsReadOnly(true)
						.AllowMultiLine(true)
						.Text(FText::FromString(CapturedContext.ToContextBlock()))
					]
				]
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

	LoadFieldOptions();
}

void SExtendedAtlassianBugReportDialog::LoadFieldOptions()
{
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	if (!Settings)
	{
		return;
	}

	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid() || !Client->IsReady())
	{
		// Still seed the fallbacks, so an offline editor shows a usable form rather than empty combos.
		Fields->Load(FString(), Settings->DefaultIssueTypeName, Settings->DefaultPriorityName, nullptr);

		SetStatus(LOCTEXT("NotConnectedStatus",
			"Not connected to Atlassian. Set the site URL and credentials in Project Settings > Plugins > Extended Atlassian."), true);
		return;
	}

	TWeakPtr<SExtendedAtlassianBugReportDialog> WeakDialog = SharedThis(this);

	Fields->Load(Settings->GetEffectiveBugProjectKey(), Settings->DefaultIssueTypeName, Settings->DefaultPriorityName,
		[WeakDialog]()
		{
			const TSharedPtr<SExtendedAtlassianBugReportDialog> Dialog = WeakDialog.Pin();
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

void SExtendedAtlassianBugReportDialog::Submit()
{
	using namespace ExtendedAtlassianBugReportPrivate;

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

	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	if (!Settings || Settings->GetEffectiveBugProjectKey().IsEmpty())
	{
		SetStatus(LOCTEXT("NeedProject", "Set a Jira project key in Project Settings > Plugins > Extended Atlassian."), true);
		return;
	}

	FExtendedAtlassianNewIssue NewIssue;
	NewIssue.ProjectKey = Settings->GetEffectiveBugProjectKey();
	NewIssue.IssueTypeName = Fields->GetIssueTypeNameToSubmit();
	NewIssue.Summary = Summary;
	NewIssue.Description = DescriptionBox.IsValid() ? DescriptionBox->GetText().ToString() : FString();

	if (bIncludeContext)
	{
		NewIssue.ContextBlock = CapturedContext.ToContextBlock();
	}

	NewIssue.PriorityName = Fields->GetPriorityNameToSubmit();

	if (LabelsBox.IsValid())
	{
		NewIssue.Labels = FExtendedAtlassianJira::ParseLabels(LabelsBox->GetText().ToString());
	}

	// Snapshot attachment payloads now; the dialog closes as soon as the issue exists.
	TSharedRef<FSubmitState> State = MakeShared<FSubmitState>();
	if (bIncludeScreenshot)
	{
		State->ScreenshotPng = ScreenshotPng;
	}
	if (bIncludeLogTail)
	{
		State->LogTail = FExtendedAtlassianContextCapture::ReadLogTail(Settings->LogTailKilobytes);
	}

	bSubmitting = true;
	SetStatus(LOCTEXT("Creating", "Creating issue..."), false);

	TWeakPtr<SExtendedAtlassianBugReportDialog> WeakDialog = SharedThis(this);

	FExtendedAtlassianJira::CreateIssue(NewIssue,
		FExtendedAtlassianCreateIssueDelegate::CreateLambda(
			[WeakDialog, State](bool bSuccess, const FString& IssueKey, const FExtendedAtlassianError& Error)
			{
				if (!bSuccess)
				{
					// Keep the dialog open so nothing the user typed is lost.
					if (const TSharedPtr<SExtendedAtlassianBugReportDialog> Dialog = WeakDialog.Pin())
					{
						Dialog->bSubmitting = false;
						Dialog->SetStatus(FText::Format(
							LOCTEXT("CreateFailed", "Could not create the issue: {0}"),
							FText::FromString(Error.Message)), true);
					}
					return;
				}

				State->IssueKey = IssueKey;

				// The issue exists; the user is free to go. Attachments continue in the background
				// and report their own outcome.
				if (const TSharedPtr<SExtendedAtlassianBugReportDialog> Dialog = WeakDialog.Pin())
				{
					Dialog->CloseWindow();
				}

				UploadAttachments(State);
			}));
}

void SExtendedAtlassianBugReportDialog::CloseWindow()
{
	if (const TSharedPtr<SWindow> Window = ParentWindow.Pin())
	{
		Window->RequestDestroyWindow();
	}
}

void SExtendedAtlassianBugReportDialog::SetStatus(const FText& Message, bool bIsError)
{
	StatusMessage = Message;
	bStatusIsError = bIsError;
}

#undef LOCTEXT_NAMESPACE
