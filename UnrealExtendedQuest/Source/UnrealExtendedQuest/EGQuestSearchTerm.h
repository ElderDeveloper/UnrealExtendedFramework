// Copyright Devil of the Plague. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

// Which editor search filter a term is subject to. Declared in the runtime module (not the editor)
// so objectives and events can describe their searchable fields without depending on the editor.
enum class EEGQuestSearchTermKind : uint8
{
	// Always searched.
	Text,
	// Searched only when the "numerical types" filter is on.
	Number,
	// Searched only when the "node GUID" filter is on.
	NodeGUID
};

// One searchable field reported by an objective or an event to Find-in-Quests.
struct FEGQuestSearchTerm
{
	FEGQuestSearchTerm() = default;
	FEGQuestSearchTerm(FString InLabel, FString InValue, const EEGQuestSearchTermKind InKind = EEGQuestSearchTermKind::Text)
		: Label(MoveTemp(InLabel)), Value(MoveTemp(InValue)), Kind(InKind) {}

	// Field name as shown in the results tree, e.g. "NodeIndex".
	FString Label;
	FString Value;
	EEGQuestSearchTermKind Kind = EEGQuestSearchTermKind::Text;
};
