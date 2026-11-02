// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementEvaluator.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementMath.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementSupportInterface.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTags.h"
#include "UnrealExtendedGameplay/Systems/Placement/Profile/EGPlacementProfile.h"
#include "UnrealExtendedGameplay/Systems/Placement/Rules/EGPlacementRule.h"
#include "UnrealExtendedGameplay/Systems/Placement/Solvers/EGPlacementPoseSolver.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementAnchorComponent.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementRegionComponent.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementSpatialSubsystem.h"

#define LOCTEXT_NAMESPACE "EGPlacementEvaluator"

int32 FEGPlacementStats::ActiveSessions = 0;
int64 FEGPlacementStats::Evaluations = 0;
double FEGPlacementStats::EvaluationSeconds = 0.0;
int64 FEGPlacementStats::CacheHits = 0;
int64 FEGPlacementStats::CacheMisses = 0;
int64 FEGPlacementStats::Commits = 0;

namespace EGPlacementEvaluatorPrivate
{
	void RefuseState(FEGPlacementState& State, const FGameplayTag& Tag, FText Text)
	{
		State.SolveStatus = EEGPlacementStatus::Invalid;
		State.SolveReasons.Emplace(Tag, MoveTemp(Text));
	}

	void AddRevision(TArray<FEGPlacementRevision>& Revisions, UObject* Source)
	{
		const int32 Revision = EGPlacement::GetRevision(Source);
		if (Source && Revision != 0)
		{
			FEGPlacementRevision& Entry = Revisions.AddDefaulted_GetRef();
			Entry.Source = Source;
			Entry.Revision = Revision;
		}
	}
}

namespace EGPlacement
{
	FEGPlacementState SolvePose(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent, const FEGPlacementContext& Context)
	{
		using namespace EGPlacementEvaluatorPrivate;

		FEGPlacementState State;
		const bool bFinite = EGPlacementMath::IsFiniteTransform(Intent.Pose);
		State.Pose = bFinite ? Intent.Pose : FTransform::Identity;
		State.Normal = State.Pose.GetRotation().GetUpVector();

		if (!Profile)
		{
			RefuseState(State, TAG_EGPlacement_Reason_NotConfigured, LOCTEXT("NoProfile", "No placement profile."));
		}
		else if (!Profile->Solver)
		{
			RefuseState(State, TAG_EGPlacement_Reason_NotConfigured, LOCTEXT("NoSolver", "The profile has no pose solver."));
		}
		else if (!Context.World)
		{
			RefuseState(State, TAG_EGPlacement_Reason_InvalidInput, LOCTEXT("NoWorld", "The context has no world."));
		}
		else if (!bFinite)
		{
			RefuseState(State, TAG_EGPlacement_Reason_InvalidInput, LOCTEXT("NaN", "The proposed pose is not finite."));
		}
		else if (!EGPlacementMath::IsUniformPositiveScale(Intent.Pose.GetScale3D()))
		{
			RefuseState(State, TAG_EGPlacement_Reason_UnsupportedTransform, LOCTEXT("Scale", "Only positive uniform scale is supported."));
		}
		else if (Spec.Shape.IsEmpty())
		{
			RefuseState(State, TAG_EGPlacement_Reason_InvalidInput, LOCTEXT("NoShape", "The spec has no footprint boxes."));
		}
		else
		{
			State.Scale = static_cast<float>(Intent.Pose.GetScale3D().X);
			Profile->Solver->Solve(Profile, Spec, Intent, Context, State);
			if (!EGPlacementMath::IsFiniteTransform(State.Pose))
			{
				State.Pose = Intent.Pose;
				RefuseState(State, TAG_EGPlacement_Reason_InvalidInput, LOCTEXT("SolverNaN", "The solver produced a pose that is not finite."));
			}
			State.Pose.SetScale3D(FVector(State.Scale));
		}
		CompleteState(Profile, Spec, Context, State);
		return State;
	}

	FEGPlacementEvaluation ValidatePose(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent, const FEGPlacementState& State, const FEGPlacementContext& Context)
	{
		FEGPlacementEvaluation Evaluation;
		Evaluation.Pose = State.Pose;
		Evaluation.Orientation = State.Orientation;
		Evaluation.Support = State.Support;
		Evaluation.Anchor = State.Anchor;
		Evaluation.Revisions = State.Revisions;
		Evaluation.State = State;

		if (State.SolveStatus != EEGPlacementStatus::Valid)
		{
			Evaluation.Status = State.SolveStatus;
			Evaluation.Reasons = State.SolveReasons;
			return Evaluation;
		}
		if (!Profile)
		{
			Evaluation.Status = EEGPlacementStatus::Invalid;
			Evaluation.Reasons.Emplace(TAG_EGPlacement_Reason_NotConfigured, LOCTEXT("NoProfileValidate", "No placement profile."));
			return Evaluation;
		}
		if (Profile->Rules.Num() == 0)
		{
			Evaluation.Status = EEGPlacementStatus::Invalid;
			Evaluation.Reasons.Emplace(TAG_EGPlacement_Reason_NotConfigured, LOCTEXT("NoRules", "The profile has no rules."), Profile);
			return Evaluation;
		}

		bool bFailed = false;
		bool bPending = false;
		for (const UEGPlacementRule* Rule : Profile->Rules)
		{
			if (!Rule || !Rule->RunsIn(Context.Scope))
			{
				continue;
			}
			FEGPlacementRuleOutput Output;
			const EEGPlacementRuleResult Result = Rule->Check(Profile, Spec, Intent, Context, State, Output);
			Evaluation.bApproximate |= Output.bApproximate;
			Evaluation.ClaimMismatches |= Output.ClaimMismatches;
			if (Result == EEGPlacementRuleResult::Fail)
			{
				bFailed = true;
				if (Output.Reasons.Num() == 0)
				{
					Output.Reasons.Emplace(TAG_EGPlacement_Reason, FText::Format(LOCTEXT("Unexplained", "{0} refused without a reason."), FText::FromString(Rule->GetClass()->GetName())), Rule);
				}
				Evaluation.Reasons.Append(Output.Reasons);
				if (!Profile->bCollectAllReasons)
				{
					break;
				}
			}
			else if (Result == EEGPlacementRuleResult::Pending)
			{
				bPending = true;
				if (Output.Reasons.Num() == 0)
				{
					Output.Reasons.Emplace(TAG_EGPlacement_Reason_Pending, FText::Format(LOCTEXT("UnexplainedPending", "{0} is waiting for data."), FText::FromString(Rule->GetClass()->GetName())), Rule);
				}
				Evaluation.Reasons.Append(Output.Reasons);
			}
		}
		Evaluation.Status = bFailed ? EEGPlacementStatus::Invalid : (bPending ? EEGPlacementStatus::Pending : EEGPlacementStatus::Valid);
		return Evaluation;
	}

	FEGPlacementEvaluation Evaluate(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent, const FEGPlacementContext& Context)
	{
		const double Start = FPlatformTime::Seconds();
		const FEGPlacementState State = SolvePose(Profile, Spec, Intent, Context);
		FEGPlacementEvaluation Evaluation = ValidatePose(Profile, Spec, Intent, State, Context);
		++FEGPlacementStats::Evaluations;
		FEGPlacementStats::EvaluationSeconds += FPlatformTime::Seconds() - Start;
		return Evaluation;
	}

	FEGPlacementState MakeStateFromPose(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FTransform& Pose, UPrimitiveComponent* Support, const FEGPlacementContext& Context)
	{
		FEGPlacementState State;
		State.Pose = Pose;
		State.Scale = static_cast<float>(Pose.GetScale3D().X);
		State.Normal = Pose.GetRotation().GetUpVector();
		State.Support.Component = Support;
		if (!EGPlacementMath::IsFiniteTransform(Pose))
		{
			State.Pose = FTransform::Identity;
			EGPlacementEvaluatorPrivate::RefuseState(State, TAG_EGPlacement_Reason_InvalidInput, LOCTEXT("NaNPose", "The pose is not finite."));
		}
		else if (!EGPlacementMath::IsUniformPositiveScale(Pose.GetScale3D()))
		{
			EGPlacementEvaluatorPrivate::RefuseState(State, TAG_EGPlacement_Reason_UnsupportedTransform, LOCTEXT("ScalePose", "Only positive uniform scale is supported."));
		}
		CompleteState(Profile, Spec, Context, State);
		return State;
	}

	void CompleteState(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementContext& Context, FEGPlacementState& State)
	{
		using namespace EGPlacementEvaluatorPrivate;

		if (State.Normal.IsNearlyZero())
		{
			State.Normal = (State.bHasSurface && State.SurfaceHit.bBlockingHit) ? FVector(State.SurfaceHit.ImpactNormal) : State.GetUp();
		}
		State.Normal = State.Normal.GetSafeNormal();
		const FEGPlacementSurfaceBands Bands = Profile ? Profile->SurfaceBands : FEGPlacementSurfaceBands();
		State.Orientation = EGPlacementMath::ClassifyNormal(State.Normal, Bands);
		State.SlopeDegrees = EGPlacementMath::SlopeDegrees(State.Normal, State.Orientation);

		if (!State.Support.Component.IsValid() && State.bHasSurface)
		{
			State.Support.Component = State.SurfaceHit.GetComponent();
		}
		if (const UPrimitiveComponent* SupportComponent = State.Support.Component.Get())
		{
			State.Support.Actor = SupportComponent->GetOwner();
			State.Support.Handle = GetSupportHandle(SupportComponent);
		}

		State.FootprintBoxes.Reset();
		State.ClearanceBoxes.Reset();
		FBox Bounds(ForceInit);
		for (const FEGPlacementBox& Box : Spec.Shape.Footprint)
		{
			const FEGPlacementOrientedBox& World = State.FootprintBoxes.Add_GetRef(EGPlacementMath::TransformBox(Box, State.Pose));
			Bounds += World.GetBounds();
		}
		for (const FEGPlacementBox& Box : Spec.Shape.GetClearance())
		{
			const FEGPlacementOrientedBox& World = State.ClearanceBoxes.Add_GetRef(EGPlacementMath::TransformBox(Box, State.Pose));
			Bounds += World.GetBounds();
		}

		State.Regions.Reset();
		State.Revisions.Reset();
		if (UEGPlacementSpatialSubsystem* Spatial = ResolveSpatial(Context))
		{
			if (Bounds.IsValid)
			{
				TArray<UEGPlacementRegionComponent*> Regions;
				Spatial->QueryRegions(Bounds, Regions);
				for (UEGPlacementRegionComponent* Region : Regions)
				{
					State.Regions.Add(Region);
					AddRevision(State.Revisions, Region);
				}
				State.bAreaAvailable = Spatial->IsAreaAvailable(Bounds);
			}
		}
		AddRevision(State.Revisions, State.Anchor.Get());
		AddRevision(State.Revisions, State.Support.Component.Get());
	}

	FEGPlacementHandle GetSupportHandle(const UPrimitiveComponent* Component)
	{
		if (!Component)
		{
			return FEGPlacementHandle();
		}
		AActor* Owner = Component->GetOwner();
		if (Owner && Owner->GetClass()->ImplementsInterface(UEGPlacementSupportInterface::StaticClass()))
		{
			FEGPlacementHandle Handle = IEGPlacementSupportInterface::Execute_GetPlacementSupportHandle(Owner, Component);
			if (Handle.IsValid())
			{
				return Handle;
			}
		}
		// The component's path inside its level, prefixed with the level package without the PIE
		// prefix, so it reads the same in every PIE instance and on every machine for level actors.
		const ULevel* Level = Owner ? Owner->GetLevel() : nullptr;
		const UObject* StopOuter = Level ? Level->GetOuter() : nullptr;
		FString LevelName = Level ? UWorld::RemovePIEPrefix(Level->GetOutermost()->GetName()) : FString();
		return FEGPlacementHandle(TEXT("Path"), FString::Printf(TEXT("%s:%s"), *FPackageName::GetShortName(LevelName), *Component->GetPathName(StopOuter)));
	}

	int32 GetRevision(const UObject* Source)
	{
		if (const UEGPlacementRegionComponent* Region = Cast<UEGPlacementRegionComponent>(Source))
		{
			return Region->GetRevision();
		}
		if (const UEGPlacementAnchorComponent* Anchor = Cast<UEGPlacementAnchorComponent>(Source))
		{
			return Anchor->GetRevision();
		}
		if (const UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Source))
		{
			AActor* Owner = Component->GetOwner();
			if (Owner && Owner->GetClass()->ImplementsInterface(UEGPlacementSupportInterface::StaticClass()))
			{
				return IEGPlacementSupportInterface::Execute_GetPlacementSupportRevision(Owner, Component);
			}
		}
		return 0;
	}

	void GatherSurfaceTags(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementState& State, FGameplayTagContainer& OutTags)
	{
		auto AddNamedTags = [&OutTags](const TArray<FName>& Names)
		{
			for (const FName& Name : Names)
			{
				const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Name, false);
				if (Tag.IsValid())
				{
					OutTags.AddTag(Tag);
				}
			}
		};

		const UPrimitiveComponent* Component = State.Support.Component.Get();
		if (Component)
		{
			AddNamedTags(Component->ComponentTags);
			if (AActor* Owner = Component->GetOwner())
			{
				AddNamedTags(Owner->Tags);
				if (Owner->GetClass()->ImplementsInterface(UEGPlacementSupportInterface::StaticClass()))
				{
					IEGPlacementSupportInterface::Execute_GetPlacementSurfaceTags(Owner, Component, OutTags);
				}
			}
		}
		if (Profile && State.bHasSurface)
		{
			if (const UPhysicalMaterial* Material = State.SurfaceHit.PhysMaterial.Get())
			{
				for (const TPair<TObjectPtr<UPhysicalMaterial>, FGameplayTagContainer>& Pair : Profile->SurfaceTagMap)
				{
					if (Pair.Key == Material)
					{
						OutTags.AppendTags(Pair.Value);
					}
				}
			}
		}
		const FVector Contact = State.GetContactPoint(Spec.ContactOffset);
		for (const TWeakObjectPtr<UEGPlacementRegionComponent>& WeakRegion : State.Regions)
		{
			const UEGPlacementRegionComponent* Region = WeakRegion.Get();
			if (Region && Region->Mode == EEGPlacementRegionMode::Include && EGPlacementMath::BoxContainsPoint(Region->GetWorldBox(), Contact))
			{
				OutTags.AppendTags(Region->RegionTags);
			}
		}
	}

	UEGPlacementSpatialSubsystem* ResolveSpatial(const FEGPlacementContext& Context)
	{
		if (Context.Spatial)
		{
			return Context.Spatial;
		}
		return Context.World ? Context.World->GetSubsystem<UEGPlacementSpatialSubsystem>() : nullptr;
	}

	FTransform GetGridFrame(const FEGPlacementGrid& Grid, const UPrimitiveComponent* Support)
	{
		if (Grid.Frame == EEGPlacementGridFrame::Support && Support)
		{
			FTransform Frame = Support->GetComponentTransform();
			Frame.SetScale3D(FVector::OneVector);
			return Frame;
		}
		return FTransform::Identity;
	}

	FEGPlacementContext MakeContext(UWorld* World, AActor* Requester, EEGPlacementScope Scope)
	{
		FEGPlacementContext Context;
		Context.World = World ? World : (Requester ? Requester->GetWorld() : nullptr);
		Context.Requester = Requester;
		Context.Scope = Scope;
		return Context;
	}
}

#undef LOCTEXT_NAMESPACE
