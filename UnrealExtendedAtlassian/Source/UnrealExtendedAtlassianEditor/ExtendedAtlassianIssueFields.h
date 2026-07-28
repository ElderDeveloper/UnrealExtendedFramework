// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianTypes.h"

/**
 * Issue type and priority options backing a create form.
 *
 * Shared by the bug report and new issue dialogs so the two cannot drift. Both need the same
 * fallback seeding, the same "(project default)" priority sentinel and the same preferred-value
 * matching by name, and every one of those exists to work around a Jira behaviour worth fixing
 * once rather than twice.
 *
 * Held by shared pointer and captured weakly by the requests it starts, so a dialog closed before
 * the metadata arrives simply drops the result.
 */
class FExtendedAtlassianIssueFields : public TSharedFromThis<FExtendedAtlassianIssueFields>
{
public:
	using FIssueTypePtr = TSharedPtr<FExtendedAtlassianIssueType>;
	using FPriorityPtr = TSharedPtr<FExtendedAtlassianPriority>;

	/**
	 * Label of the sentinel priority entry.
	 *
	 * Selecting it leaves priority out of the request entirely, which is the only thing that works
	 * on a project with no priority field — sending one there fails the whole create.
	 */
	static const TCHAR* ProjectDefaultPriorityLabel;

	TArray<FIssueTypePtr> IssueTypes;
	FIssueTypePtr SelectedIssueType;

	TArray<FPriorityPtr> Priorities;
	FPriorityPtr SelectedPriority;

	/**
	 * Seeds usable fallbacks from the preferred names, then asks Jira for the real lists.
	 *
	 * OnChanged fires on the game thread once per list that arrives, so the caller can refresh its
	 * combo boxes. A preferred name that the project does not offer loses to the project's first
	 * issue type rather than being forced into the request.
	 */
	void Load(
		const FString& ProjectKey,
		const FString& PreferredIssueType,
		const FString& PreferredPriority,
		TFunction<void()> OnChanged);

	bool HasIssueType() const { return SelectedIssueType.IsValid() && !SelectedIssueType->Name.IsEmpty(); }

	/** The issue type to send. Empty when nothing is selected yet. */
	FString GetIssueTypeNameToSubmit() const;

	/** The priority to send, or empty when the sentinel is selected and the field should be omitted. */
	FString GetPriorityNameToSubmit() const;

	FText GetIssueTypeLabel() const;
	FText GetPriorityLabel() const;
};
