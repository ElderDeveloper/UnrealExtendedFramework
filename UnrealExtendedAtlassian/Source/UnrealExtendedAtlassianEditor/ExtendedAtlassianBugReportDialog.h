// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianContextCapture.h"
#include "ExtendedAtlassianTypes.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SMultiLineEditableTextBox;
class SWindow;
struct FSlateDynamicImageBrush;

template <typename OptionType> class SComboBox;

/**
 * Bug report form, pre-filled with editor state captured at the moment it was opened.
 *
 * Open() captures the screenshot and context *before* creating the window, so the attached image
 * shows what the user was looking at rather than this dialog sitting on top of it.
 */
class SExtendedAtlassianBugReportDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExtendedAtlassianBugReportDialog) {}
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
		SLATE_ARGUMENT(FExtendedAtlassianCapturedContext, CapturedContext)
		SLATE_ARGUMENT(TArray<uint8>, ScreenshotPng)
		SLATE_ARGUMENT(FIntPoint, ScreenshotSize)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Captures context and a screenshot, then opens the dialog. Bound to the Report Bug command. */
	static void Open();

private:
	using FIssueTypePtr = TSharedPtr<FExtendedAtlassianIssueType>;
	using FPriorityPtr = TSharedPtr<FExtendedAtlassianPriority>;

	void LoadFieldOptions();
	void Submit();
	void CloseWindow();
	void SetStatus(const FText& Message, bool bIsError);

	TWeakPtr<SWindow> ParentWindow;

	FExtendedAtlassianCapturedContext CapturedContext;
	TArray<uint8> ScreenshotPng;
	FIntPoint ScreenshotSize = FIntPoint::ZeroValue;
	TSharedPtr<FSlateDynamicImageBrush> ThumbnailBrush;

	TSharedPtr<SEditableTextBox> SummaryBox;
	TSharedPtr<SMultiLineEditableTextBox> DescriptionBox;
	TSharedPtr<SEditableTextBox> LabelsBox;

	TArray<FIssueTypePtr> IssueTypes;
	FIssueTypePtr SelectedIssueType;
	TSharedPtr<SComboBox<FIssueTypePtr>> IssueTypeCombo;

	TArray<FPriorityPtr> Priorities;
	FPriorityPtr SelectedPriority;
	TSharedPtr<SComboBox<FPriorityPtr>> PriorityCombo;

	bool bIncludeScreenshot = true;
	bool bIncludeLogTail = true;
	bool bIncludeContext = true;
	bool bSubmitting = false;

	FText StatusMessage;
	bool bStatusIsError = false;
};
