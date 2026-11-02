// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementDebug.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementSpatialSubsystem.h"

namespace EGPlacementDebugPrivate
{
	static TAutoConsoleVariable<int32> CVarPlacementDebug(
		TEXT("Extended.Placement.Debug"),
		0,
		TEXT("Placement debug drawing. 0 off, 1 evaluations, 2 also regions and anchors near the pose."),
		ECVF_Cheat);

	FColor StatusColor(EEGPlacementStatus Status)
	{
		switch (Status)
		{
		case EEGPlacementStatus::Valid: return FColor(60, 220, 90);
		case EEGPlacementStatus::Pending: return FColor(240, 200, 40);
		default: return FColor(235, 60, 50);
		}
	}

	const TCHAR* OrientationName(EEGPlacementOrientation Orientation)
	{
		switch (Orientation)
		{
		case EEGPlacementOrientation::Floor: return TEXT("Floor");
		case EEGPlacementOrientation::Wall: return TEXT("Wall");
		case EEGPlacementOrientation::Ceiling: return TEXT("Ceiling");
		default: return TEXT("None");
		}
	}
}

namespace EGPlacementDebug
{
	int32 GetLevel()
	{
		return EGPlacementDebugPrivate::CVarPlacementDebug.GetValueOnGameThread();
	}

	void DrawOrientedBox(const UWorld* World, const FEGPlacementOrientedBox& Box, const FColor& Color, float Duration, float Thickness)
	{
#if ENABLE_DRAW_DEBUG
		if (World)
		{
			DrawDebugBox(World, Box.Center, Box.Extent, Box.Rotation, Color, false, Duration, SDPG_Foreground, Thickness);
		}
#endif
	}

	void DrawEvaluation(const UWorld* World, const FEGPlacementEvaluation& Evaluation, float Duration)
	{
#if ENABLE_DRAW_DEBUG
		using namespace EGPlacementDebugPrivate;
		if (!World)
		{
			return;
		}
		const FColor Color = StatusColor(Evaluation.Status);
		const FEGPlacementState& State = Evaluation.State;
		for (const FEGPlacementOrientedBox& Box : State.FootprintBoxes)
		{
			DrawOrientedBox(World, Box, Color, Duration, 1.5f);
		}
		for (const FEGPlacementOrientedBox& Box : State.ClearanceBoxes)
		{
			DrawOrientedBox(World, Box, FColor(80, 200, 230), Duration, 0.5f);
		}
		const FVector Location = Evaluation.Pose.GetLocation();
		DrawDebugCoordinateSystem(World, Location, Evaluation.Pose.Rotator(), 25.0f, false, Duration, SDPG_Foreground, 1.0f);
		if (State.bHasSurface)
		{
			const FVector Impact = State.SurfaceHit.ImpactPoint;
			DrawDebugPoint(World, Impact, 8.0f, Color, false, Duration, SDPG_Foreground);
			DrawDebugDirectionalArrow(World, Impact, Impact + State.Normal * 40.0f, 8.0f, Color, false, Duration, SDPG_Foreground, 1.0f);
		}

		FString Text = FString::Printf(TEXT("%s  %s  slope %.0f"),
			Evaluation.Status == EEGPlacementStatus::Valid ? TEXT("Valid") : (Evaluation.Status == EEGPlacementStatus::Pending ? TEXT("Pending") : TEXT("Invalid")),
			OrientationName(Evaluation.Orientation), State.SlopeDegrees);
		if (const UPrimitiveComponent* Support = Evaluation.Support.Component.Get())
		{
			Text += FString::Printf(TEXT("\non %s"), Support->GetOwner() ? *Support->GetOwner()->GetActorNameOrLabel() : *Support->GetName());
		}
		if (Evaluation.bApproximate)
		{
			Text += TEXT("  (approximate)");
		}
		for (const FEGPlacementReason& Reason : Evaluation.Reasons)
		{
			Text += FString::Printf(TEXT("\n%s"), Reason.Text.IsEmpty() ? *Reason.Tag.ToString() : *Reason.Text.ToString());
		}
		DrawDebugString(World, Location + Evaluation.Pose.GetRotation().GetUpVector() * 40.0f, Text, nullptr, Color, Duration > 0.0f ? Duration : 0.0f, true, 1.0f);
#endif
	}

	void DrawSpatial(const UWorld* World, const FVector& Center, float Radius, float Duration)
	{
#if ENABLE_DRAW_DEBUG
		const UEGPlacementSpatialSubsystem* Spatial = World ? World->GetSubsystem<UEGPlacementSpatialSubsystem>() : nullptr;
		if (!Spatial)
		{
			return;
		}
		TArray<UEGPlacementRegionComponent*> Regions;
		Spatial->QueryRegions(FBox(Center - FVector(Radius), Center + FVector(Radius)), Regions);
		for (const UEGPlacementRegionComponent* Region : Regions)
		{
			const FColor Color = Region->Mode == EEGPlacementRegionMode::Include ? FColor(70, 130, 255) : FColor(255, 70, 160);
			DrawOrientedBox(World, Region->GetWorldBox(), Color, Duration, 1.0f);
		}
		TArray<UEGPlacementAnchorComponent*> Anchors;
		Spatial->QueryAnchors(Center, Radius, Anchors);
		for (const UEGPlacementAnchorComponent* Anchor : Anchors)
		{
			const FTransform Transform = Anchor->GetAnchorTransform();
			const FColor Color = Anchor->bAnchorEnabled ? FColor(255, 170, 40) : FColor(120, 120, 120);
			DrawDebugCoordinateSystem(World, Transform.GetLocation(), Transform.Rotator(), 15.0f, false, Duration, SDPG_Foreground, 1.0f);
			DrawDebugCircle(World, Transform.GetLocation(), Anchor->SnapRadius, 24, Color, false, Duration, SDPG_Foreground, 0.5f,
				Transform.GetRotation().GetAxisX(), Transform.GetRotation().GetAxisY(), false);
		}
#endif
	}
}
