// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianIssueFields.h"

#include "ExtendedAtlassianJira.h"

#define LOCTEXT_NAMESPACE "ExtendedAtlassianIssueFields"

const TCHAR* FExtendedAtlassianIssueFields::ProjectDefaultPriorityLabel = TEXT("(project default)");

void FExtendedAtlassianIssueFields::Load(
	const FString& ProjectKey,
	const FString& PreferredIssueType,
	const FString& PreferredPriority,
	TFunction<void()> OnChanged)
{
	// Seed from the preferred names first, so the form is usable even if the metadata calls fail.
	if (!PreferredIssueType.IsEmpty())
	{
		FExtendedAtlassianIssueType Fallback;
		Fallback.Name = PreferredIssueType;
		IssueTypes.Add(MakeShared<FExtendedAtlassianIssueType>(Fallback));
		SelectedIssueType = IssueTypes[0];
	}

	FExtendedAtlassianPriority DefaultPriority;
	DefaultPriority.Name = ProjectDefaultPriorityLabel;
	Priorities.Add(MakeShared<FExtendedAtlassianPriority>(DefaultPriority));
	SelectedPriority = Priorities[0];

	// Without a project there is nothing to ask createmeta about, and nothing could be created from
	// the answer anyway. The seeded fallbacks above still leave a usable form.
	if (ProjectKey.TrimStartAndEnd().IsEmpty())
	{
		return;
	}

	TWeakPtr<FExtendedAtlassianIssueFields> WeakSelf = AsShared();

	FExtendedAtlassianJira::GetIssueTypes(ProjectKey,
		FExtendedAtlassianIssueTypesDelegate::CreateLambda(
			[WeakSelf, PreferredIssueType, OnChanged](bool bSuccess, const TArray<FExtendedAtlassianIssueType>& InTypes, const FExtendedAtlassianError& Error)
			{
				const TSharedPtr<FExtendedAtlassianIssueFields> Self = WeakSelf.Pin();
				if (!Self.IsValid() || !bSuccess || InTypes.Num() == 0)
				{
					return;
				}

				Self->IssueTypes.Reset();
				for (const FExtendedAtlassianIssueType& IssueType : InTypes)
				{
					Self->IssueTypes.Add(MakeShared<FExtendedAtlassianIssueType>(IssueType));
				}

				Self->SelectedIssueType = Self->IssueTypes[0];
				for (const FIssueTypePtr& IssueType : Self->IssueTypes)
				{
					if (IssueType.IsValid() && IssueType->Name.Equals(PreferredIssueType, ESearchCase::IgnoreCase))
					{
						Self->SelectedIssueType = IssueType;
						break;
					}
				}

				if (OnChanged)
				{
					OnChanged();
				}
			}));

	FExtendedAtlassianJira::GetPriorities(
		FExtendedAtlassianPrioritiesDelegate::CreateLambda(
			[WeakSelf, PreferredPriority, OnChanged](bool bSuccess, const TArray<FExtendedAtlassianPriority>& InPriorities, const FExtendedAtlassianError& Error)
			{
				const TSharedPtr<FExtendedAtlassianIssueFields> Self = WeakSelf.Pin();
				if (!Self.IsValid() || !bSuccess)
				{
					return;
				}

				for (const FExtendedAtlassianPriority& Priority : InPriorities)
				{
					Self->Priorities.Add(MakeShared<FExtendedAtlassianPriority>(Priority));
				}

				if (!PreferredPriority.IsEmpty())
				{
					for (const FPriorityPtr& Priority : Self->Priorities)
					{
						if (Priority.IsValid() && Priority->Name.Equals(PreferredPriority, ESearchCase::IgnoreCase))
						{
							Self->SelectedPriority = Priority;
							break;
						}
					}
				}

				if (OnChanged)
				{
					OnChanged();
				}
			}));
}

FString FExtendedAtlassianIssueFields::GetIssueTypeNameToSubmit() const
{
	return SelectedIssueType.IsValid() ? SelectedIssueType->Name : FString();
}

FString FExtendedAtlassianIssueFields::GetPriorityNameToSubmit() const
{
	// The sentinel entry has no id; anything else is a real priority.
	return SelectedPriority.IsValid() && !SelectedPriority->Id.IsEmpty()
		? SelectedPriority->Name
		: FString();
}

FText FExtendedAtlassianIssueFields::GetIssueTypeLabel() const
{
	return SelectedIssueType.IsValid()
		? FText::FromString(SelectedIssueType->Name)
		: LOCTEXT("LoadingTypes", "Loading...");
}

FText FExtendedAtlassianIssueFields::GetPriorityLabel() const
{
	return SelectedPriority.IsValid()
		? FText::FromString(SelectedPriority->Name)
		: FText::FromString(ProjectDefaultPriorityLabel);
}

#undef LOCTEXT_NAMESPACE
