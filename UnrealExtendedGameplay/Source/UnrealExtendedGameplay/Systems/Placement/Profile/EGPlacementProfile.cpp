// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementProfile.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/DataValidation.h"
#include "UnrealExtendedGameplay/Systems/Placement/Rules/EGPlacementRule.h"
#include "UnrealExtendedGameplay/Systems/Placement/Solvers/EGPlacementPoseSolver.h"

#define LOCTEXT_NAMESPACE "EGPlacementProfile"

FCollisionQueryParams UEGPlacementProfile::MakeQueryParams(const FEGPlacementContext& Context, FName TraceTag, bool bReturnPhysicalMaterial) const
{
	FCollisionQueryParams Params(TraceTag, Query.bTraceComplex);
	Params.bReturnPhysicalMaterial = bReturnPhysicalMaterial;
	if (Context.Requester)
	{
		Params.AddIgnoredActor(Context.Requester);
	}
	for (AActor* Actor : Context.IgnoredActors)
	{
		if (Actor)
		{
			Params.AddIgnoredActor(Actor);
		}
	}
	for (UPrimitiveComponent* Component : Context.IgnoredComponents)
	{
		if (Component)
		{
			Params.AddIgnoredComponent(Component);
		}
	}
	return Params;
}

bool UEGPlacementProfile::LineTrace(const FEGPlacementContext& Context, const FVector& Start, const FVector& End, FHitResult& OutHit, FName TraceTag) const
{
	if (!Context.World)
	{
		return false;
	}
	const FCollisionQueryParams Params = MakeQueryParams(Context, TraceTag, SurfaceTagMap.Num() > 0);
	if (!Query.CollisionProfile.IsNone())
	{
		return Context.World->LineTraceSingleByProfile(OutHit, Start, End, Query.CollisionProfile, Params);
	}
	return Context.World->LineTraceSingleByChannel(OutHit, Start, End, Query.Channel, Params);
}

bool UEGPlacementProfile::OverlapBox(const FEGPlacementContext& Context, const FEGPlacementOrientedBox& Box, TArray<FOverlapResult>& OutOverlaps, FName TraceTag) const
{
	if (!Context.World)
	{
		return false;
	}
	const FCollisionQueryParams Params = MakeQueryParams(Context, TraceTag);
	const FCollisionShape Shape = FCollisionShape::MakeBox(FVector3f(Box.Extent));
	if (!Query.CollisionProfile.IsNone())
	{
		return Context.World->OverlapMultiByProfile(OutOverlaps, Box.Center, Box.Rotation, Query.CollisionProfile, Shape, Params);
	}
	return Context.World->OverlapMultiByChannel(OutOverlaps, Box.Center, Box.Rotation, Query.Channel, Shape, Params);
}

#if WITH_EDITOR
EDataValidationResult UEGPlacementProfile::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!Solver)
	{
		Context.AddError(LOCTEXT("NoSolver", "The profile has no pose solver; every evaluation answers NotConfigured."));
		Result = EDataValidationResult::Invalid;
	}
	if (Rules.Num() == 0)
	{
		Context.AddError(LOCTEXT("NoRules", "The profile has no rules; every evaluation answers NotConfigured. Add the rules this kind of placement needs."));
		Result = EDataValidationResult::Invalid;
	}
	for (int32 Index = 0; Index < Rules.Num(); ++Index)
	{
		const UEGPlacementRule* Rule = Rules[Index];
		if (!Rule)
		{
			Context.AddError(FText::Format(LOCTEXT("EmptyRule", "Rule {0} is empty."), Index));
			Result = EDataValidationResult::Invalid;
			continue;
		}
		const EEGPlacementRuleScope Meaningful = Rule->GetMeaningfulScope();
		const bool bNeverMeaningful = Meaningful != EEGPlacementRuleScope::Both && Rule->Scope != EEGPlacementRuleScope::Both && Rule->Scope != Meaningful;
		if (!Rule->bEnabled)
		{
			Context.AddWarning(FText::Format(LOCTEXT("DisabledRule", "Rule {0} ({1}) is disabled and never runs."), Index, Rule->GetClass()->GetDisplayNameText()));
		}
		else if (bNeverMeaningful)
		{
			Context.AddWarning(FText::Format(LOCTEXT("ScopeRule", "Rule {0} ({1}) only means something in another scope and never checks anything here."), Index, Rule->GetClass()->GetDisplayNameText()));
		}
	}
	if (SurfaceBands.FloorMinNormalZ <= SurfaceBands.CeilingMaxNormalZ)
	{
		Context.AddError(LOCTEXT("Bands", "The floor band must start above the ceiling band."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
