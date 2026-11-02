// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementComponentVisualizers.h"

#include "SceneManagement.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementAnchorComponent.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementRegionComponent.h"

namespace EGPlacementVisualizerPrivate
{
	void DrawOrientedBox(FPrimitiveDrawInterface* PDI, const FEGPlacementOrientedBox& Box, const FLinearColor& Color, float Thickness)
	{
		FVector Corners[8];
		Box.GetCorners(Corners);
		// Corner index bits: 1 = +X, 2 = +Y, 4 = +Z. An edge joins corners that differ in one bit.
		for (int32 Index = 0; Index < 8; ++Index)
		{
			for (int32 Bit = 1; Bit < 8; Bit <<= 1)
			{
				if ((Index & Bit) == 0)
				{
					PDI->DrawLine(Corners[Index], Corners[Index | Bit], Color, SDPG_World, Thickness);
				}
			}
		}
	}
}

void FEGPlacementRegionVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UEGPlacementRegionComponent* Region = Cast<UEGPlacementRegionComponent>(Component);
	if (!Region)
	{
		return;
	}
	const FLinearColor Color = Region->Mode == EEGPlacementRegionMode::Include ? FLinearColor(0.27f, 0.5f, 1.0f) : FLinearColor(1.0f, 0.27f, 0.63f);
	const FEGPlacementOrientedBox Box = Region->GetWorldBox();
	EGPlacementVisualizerPrivate::DrawOrientedBox(PDI, Box, Color, 1.5f);
	// A cross on the floor face marks which way is down.
	const FVector Up = Box.GetAxis(2) * Box.Extent.Z;
	const FVector X = Box.GetAxis(0) * Box.Extent.X;
	const FVector Y = Box.GetAxis(1) * Box.Extent.Y;
	const FVector Floor = Box.Center - Up;
	PDI->DrawLine(Floor - X - Y, Floor + X + Y, Color * 0.6f, SDPG_World, 0.5f);
	PDI->DrawLine(Floor - X + Y, Floor + X - Y, Color * 0.6f, SDPG_World, 0.5f);
}

void FEGPlacementAnchorVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UEGPlacementAnchorComponent* Anchor = Cast<UEGPlacementAnchorComponent>(Component);
	if (!Anchor)
	{
		return;
	}
	const FTransform Transform = Anchor->GetAnchorTransform();
	const FVector Location = Transform.GetLocation();
	const FQuat Rotation = Transform.GetRotation();
	const FLinearColor Color = Anchor->bAnchorEnabled ? FLinearColor(1.0f, 0.67f, 0.16f) : FLinearColor(0.5f, 0.5f, 0.5f);
	const float AxisLength = FMath::Max(10.0f, Anchor->SnapRadius * 0.6f);
	PDI->DrawLine(Location, Location + Rotation.GetAxisX() * AxisLength, FLinearColor::Red, SDPG_World, 1.5f);
	PDI->DrawLine(Location, Location + Rotation.GetAxisY() * AxisLength, FLinearColor::Green, SDPG_World, 1.5f);
	PDI->DrawLine(Location, Location + Rotation.GetAxisZ() * AxisLength, FLinearColor::Blue, SDPG_World, 1.5f);
	DrawCircle(PDI, Location, Rotation.GetAxisX(), Rotation.GetAxisY(), Color, Anchor->SnapRadius, 32, SDPG_World, 1.0f);
}
