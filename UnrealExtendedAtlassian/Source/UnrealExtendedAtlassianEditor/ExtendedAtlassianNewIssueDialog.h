// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianIssueFields.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SMultiLineEditableTextBox;
class SWindow;

template <typename OptionType> class SComboBox;

/**
 * Form for filing an ordinary work item — a task, story or anything else the project offers.
 *
 * Deliberately separate from the bug report dialog rather than a mode of it. A bug report is a
 * capture flow: it exists to attach the screenshot, log and editor state taken at the moment
 * something broke, and it files into the bug project. This is a plain create against the work
 * project, and folding the two together would put half a form behind a checkbox in both.
 *
 * What they do share is FExtendedAtlassianIssueFields, since the issue type and priority lists
 * carry the same Jira quirks either way.
 */
class SExtendedAtlassianNewIssueDialog : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnIssueCreated, const FString& /*IssueKey*/);

	SLATE_BEGIN_ARGS(SExtendedAtlassianNewIssueDialog) {}
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
		SLATE_EVENT(FOnIssueCreated, OnIssueCreated)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Opens the dialog. OnCreated fires on the game thread once Jira has accepted the issue. */
	static void Open(FOnIssueCreated OnCreated = FOnIssueCreated());

private:
	using FIssueTypePtr = FExtendedAtlassianIssueFields::FIssueTypePtr;
	using FPriorityPtr = FExtendedAtlassianIssueFields::FPriorityPtr;

	void Submit();
	void CloseWindow();
	void SetStatus(const FText& Message, bool bIsError);

	TWeakPtr<SWindow> ParentWindow;
	FOnIssueCreated OnIssueCreated;

	TSharedPtr<FExtendedAtlassianIssueFields> Fields;

	/** The project the issue is filed into, resolved once on open so the label cannot lie. */
	FString ProjectKey;

	TSharedPtr<SEditableTextBox> SummaryBox;
	TSharedPtr<SMultiLineEditableTextBox> DescriptionBox;
	TSharedPtr<SEditableTextBox> LabelsBox;
	TSharedPtr<SComboBox<FIssueTypePtr>> IssueTypeCombo;
	TSharedPtr<SComboBox<FPriorityPtr>> PriorityCombo;

	bool bSubmitting = false;

	FText StatusMessage;
	bool bStatusIsError = false;
};
