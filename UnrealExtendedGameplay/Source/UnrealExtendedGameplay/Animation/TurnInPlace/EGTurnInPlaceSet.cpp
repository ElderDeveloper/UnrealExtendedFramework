// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGTurnInPlaceSet.h"

#include "Animation/AnimSequence.h"
#include "UnrealExtendedGameplay/Animation/EGAnimationTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "EGTurnInPlaceSet"

// ---------------------------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------------------------

const FEGTurnAngles* FEGTurnInPlaceEntry::GetAngles(const FGameplayTag& ModeTag) const
{
	if (ModeTag == TAG_EGTurnInPlace_Mode_Movement)
	{
		return &MovementAngles;
	}
	if (ModeTag == TAG_EGTurnInPlace_Mode_Strafe)
	{
		return &StrafeAngles;
	}
	return CustomModeAngles.Find(ModeTag);
}

UAnimSequence* FEGTurnInPlaceEntry::GetTurnSequence(int32 StepIndex, bool bRight) const
{
	const TArray<TObjectPtr<UAnimSequence>>& Turns = bRight ? RightTurns : LeftTurns;
	return Turns.IsValidIndex(StepIndex) ? Turns[StepIndex].Get() : nullptr;
}

const FEGTurnInPlaceEntry& FEGTurnInPlaceGraphInput::GetEntry() const
{
	return Set ? Set->GetEntry(StanceKey) : UEGTurnInPlaceSet::GetEmptyEntry();
}

// ---------------------------------------------------------------------------------------------
// UEGTurnInPlaceSet
// ---------------------------------------------------------------------------------------------

bool UEGTurnInPlaceSet::HasTurnCurves(const UAnimSequenceBase* Sequence, const FEGTurnInPlaceCurveNames& Names)
{
	// Through the base class, with both arguments: UAnimSequence's override has no default.
	return Sequence && Sequence->HasCurveData(Names.RemainingTurnYaw, false) && Sequence->HasCurveData(Names.TurnYawWeight, false);
}

const FEGTurnInPlaceEntry& UEGTurnInPlaceSet::GetEmptyEntry()
{
	static const FEGTurnInPlaceEntry Empty;
	return Empty;
}

const FEGTurnInPlaceEntry& UEGTurnInPlaceSet::GetEntry(const FGameplayTag& StanceKey) const
{
	if (StanceKey.IsValid())
	{
		if (const FEGTurnInPlaceEntry* Entry = Stances.Find(StanceKey))
		{
			return *Entry;
		}
	}
	return Default;
}

FGameplayTag UEGTurnInPlaceSet::FindStanceKey(const FGameplayTagContainer& StateTags) const
{
	for (const TPair<FGameplayTag, FEGTurnInPlaceEntry>& Stance : Stances)
	{
		if (StateTags.HasTag(Stance.Key))
		{
			return Stance.Key;
		}
	}
	return FGameplayTag();
}

#if WITH_EDITOR
namespace
{
	void ValidateEntry(const FEGTurnInPlaceEntry& Entry, const FText& Label, const FEGTurnInPlaceCurveNames& Curves, FDataValidationContext& Context, EDataValidationResult& Result)
	{
		if (Entry.State == EEGTurnInPlaceState::Paused)
		{
			return;
		}

		for (int32 Index = 0; Index < Entry.StepSizes.Num(); ++Index)
		{
			if (Entry.StepSizes[Index] <= 0 || (Index > 0 && Entry.StepSizes[Index] <= Entry.StepSizes[Index - 1]))
			{
				Context.AddError(FText::Format(LOCTEXT("StepOrder", "{0}: StepSizes must be positive and ascending."), Label));
				Result = EDataValidationResult::Invalid;
				break;
			}
		}

		const bool bMissingSteps = Entry.LeftTurns.Num() < Entry.StepSizes.Num() || Entry.RightTurns.Num() < Entry.StepSizes.Num();
		if ((bMissingSteps && !Entry.Procedural.bTurnWithoutAnimation)
			|| Entry.LeftTurns.Num() > Entry.StepSizes.Num() || Entry.RightTurns.Num() > Entry.StepSizes.Num())
		{
			Context.AddError(FText::Format(LOCTEXT("StepCount", "{0}: LeftTurns and RightTurns need one sequence per step ({1}); fewer is allowed while Procedural.bTurnWithoutAnimation is on."), Label, FText::AsNumber(Entry.StepSizes.Num())));
			Result = EDataValidationResult::Invalid;
		}

		auto CheckSequences = [&](const TArray<TObjectPtr<UAnimSequence>>& Turns, const FText& Side)
		{
			for (int32 Index = 0; Index < Turns.Num(); ++Index)
			{
				const UAnimSequence* Sequence = Turns[Index];
				if (!Sequence)
				{
					// A missing clip turns procedurally when the entry allows it.
					if (!Entry.Procedural.bTurnWithoutAnimation)
					{
						Context.AddError(FText::Format(LOCTEXT("MissingSequence", "{0}: {1} turn {2} is empty and Procedural.bTurnWithoutAnimation is off."), Label, Side, FText::AsNumber(Index)));
						Result = EDataValidationResult::Invalid;
					}
					continue;
				}
				// A warning: the Turn In Place node reads the turn from the root track without them.
				if (!HasTurnCurves(Sequence, Curves))
				{
					Context.AddWarning(FText::Format(LOCTEXT("MissingCurves", "{0}: {1} lacks the {2} / {3} curves. The Turn In Place node reads its root track instead; the state machine recipe and Pseudo servers need the curves (run the Turn In Place Curves modifier)."),
						Label, FText::FromString(Sequence->GetName()), FText::FromName(Curves.RemainingTurnYaw), FText::FromName(Curves.TurnYawWeight)));
				}
			}
		};
		CheckSequences(Entry.LeftTurns, LOCTEXT("Left", "left"));
		CheckSequences(Entry.RightTurns, LOCTEXT("Right", "right"));

		auto CheckAngles = [&](const FEGTurnAngles& Angles, const FText& Mode)
		{
			if (Angles.MaxTurnAngle > 0.0f && Angles.MaxTurnAngle < Angles.MinTurnAngle)
			{
				Context.AddError(FText::Format(LOCTEXT("AngleOrder", "{0}: {1} MaxTurnAngle is below MinTurnAngle."), Label, Mode));
				Result = EDataValidationResult::Invalid;
			}
		};
		CheckAngles(Entry.MovementAngles, LOCTEXT("Movement", "Movement"));
		CheckAngles(Entry.StrafeAngles, LOCTEXT("Strafe", "Strafe"));
		for (const TPair<FGameplayTag, FEGTurnAngles>& Custom : Entry.CustomModeAngles)
		{
			CheckAngles(Custom.Value, FText::FromString(Custom.Key.ToString()));
		}
	}
}

EDataValidationResult UEGTurnInPlaceSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	ValidateEntry(Default, LOCTEXT("Default", "Default"), Curves, Context, Result);
	for (const TPair<FGameplayTag, FEGTurnInPlaceEntry>& Stance : Stances)
	{
		if (!Stance.Key.IsValid())
		{
			Context.AddError(LOCTEXT("EmptyStance", "A stance has no tag."));
			Result = EDataValidationResult::Invalid;
			continue;
		}
		ValidateEntry(Stance.Value, FText::FromString(Stance.Key.ToString()), Curves, Context, Result);
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
