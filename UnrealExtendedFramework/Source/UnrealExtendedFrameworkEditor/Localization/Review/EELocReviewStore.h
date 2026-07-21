// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

/** LW-5 review states — editor-only metadata, never written into compiled localization resources. */
enum class EEELocReviewState : uint8
{
	None,
	Draft,
	MachineDraft,
	HumanReviewed,
	Locked,
	NeedsContext,
	NeedsNativeRewrite
};

struct FEELocReviewRecord
{
	EEELocReviewState State = EEELocReviewState::None;
	FString Reviewer;
	FString SourceTextAtReview;
	TArray<FString> Comments;
};

/**
 * Review-state sidecar: one versioned JSON file per localization target
 * (Content/Localization/<Target>/<Target>.review.json). Keyed by "Namespace,Key" + culture.
 * Locked entries are enforced by every workbench write path.
 */
class UNREALEXTENDEDFRAMEWORKEDITOR_API FEELocReviewStore
{
public:
	static FString ReviewStateToString(EEELocReviewState State);
	static EEELocReviewState ReviewStateFromString(const FString& Text);

	bool Load(const FString& TargetName);
	bool Save(FString& OutError) const;

	FEELocReviewRecord GetRecord(const FString& Namespace, const FString& Key, const FString& Culture) const;
	void SetState(const FString& Namespace, const FString& Key, const FString& Culture,
		EEELocReviewState State, const FString& Reviewer, const FString& SourceTextAtReview);
	void AddComment(const FString& Namespace, const FString& Key, const FString& Culture, const FString& Comment);

	bool IsLocked(const FString& Namespace, const FString& Key, const FString& Culture) const
	{
		return GetRecord(Namespace, Key, Culture).State == EEELocReviewState::Locked;
	}

private:
	static FString MakeEntryKey(const FString& Namespace, const FString& Key, const FString& Culture)
	{
		return Namespace + TEXT(",") + Key + TEXT("@") + Culture;
	}

	FString GetFilePath() const;

	FString TargetName;
	TMap<FString, FEELocReviewRecord> Records;
};

#endif // WITH_EDITOR
