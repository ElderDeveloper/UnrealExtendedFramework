// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessTableStyle.h"

UEGChessTableStyle::UEGChessTableStyle()
{
	BoardSurfaceTransform.SetLocation(FVector(-3.5f * SquareSize, -3.5f * SquareSize, 1.0f));

	auto AddMarker = [this](EEGChessMarkType Type, const FLinearColor& Color, float Scale, float Height) -> FEGChessMarkerVisual&
	{
		FEGChessMarkerVisual Visual;
		Visual.Color = Color;
		Visual.Scale = Scale;
		Visual.HeightOffset = Height;
		return Markers.Add(Type, Visual);
	};
	// Every local mark carries a dark stroke, so it reads on light squares as well as dark ones.
	// HeightOffset is also the draw order: the square being aimed at sits on top of everything.
	const FLinearColor Stroke(0.0f, 0.0f, 0.0f, 0.8f);
	AddMarker(EEGChessMarkType::LastMoveFrom, FLinearColor(0.9f, 0.75f, 0.25f, 0.22f), 1.0f, 0.05f);
	AddMarker(EEGChessMarkType::LastMoveTo, FLinearColor(0.9f, 0.75f, 0.25f, 0.32f), 1.0f, 0.06f);
	AddMarker(EEGChessMarkType::Check, FLinearColor(1.0f, 0.12f, 0.08f, 0.55f), 1.0f, 0.07f);
	{
		FEGChessMarkerVisual& Selected = AddMarker(EEGChessMarkType::Selected, FLinearColor(0.35f, 0.7f, 1.0f, 0.95f), 1.0f, 0.08f);
		Selected.Shape = EEGChessMarkerShape::Frame;
		Selected.FrameWidth = 0.08f;
		Selected.FillColor = FLinearColor(0.35f, 0.7f, 1.0f, 0.18f);
		Selected.OutlineColor = Stroke;
	}
	{
		FEGChessMarkerVisual& Partner = AddMarker(EEGChessMarkType::CastlePartner, FLinearColor(0.35f, 0.7f, 1.0f, 0.6f), 1.0f, 0.09f);
		Partner.Shape = EEGChessMarkerShape::Frame;
		Partner.FrameWidth = 0.05f;
		Partner.OutlineColor = Stroke;
	}
	{
		FEGChessMarkerVisual& Capture = AddMarker(EEGChessMarkType::Capture, FLinearColor(1.0f, 0.22f, 0.1f, 0.95f), 0.96f, 0.10f);
		Capture.Shape = EEGChessMarkerShape::Frame;
		Capture.FrameWidth = 0.09f;
		Capture.OutlineColor = Stroke;
	}
	{
		FEGChessMarkerVisual& Cursor = AddMarker(EEGChessMarkType::Cursor, FLinearColor(1.0f, 1.0f, 1.0f, 0.75f), 0.94f, 0.11f);
		Cursor.Shape = EEGChessMarkerShape::Frame;
		Cursor.FrameWidth = 0.04f;
		Cursor.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);
		Cursor.OutlineWidth = 0.02f;
	}
	{
		FEGChessMarkerVisual& Move = AddMarker(EEGChessMarkType::Move, FLinearColor(1.0f, 0.85f, 0.45f, 0.9f), 0.28f, 0.13f);
		Move.Shape = EEGChessMarkerShape::Disc;
		Move.OutlineColor = Stroke;
		Move.OutlineWidth = 0.045f;
	}
	{
		// Above the cursor and the move dot: the one square the click will land on.
		FEGChessMarkerVisual& Hover = AddMarker(EEGChessMarkType::HoverTarget, FLinearColor(1.6f, 1.05f, 0.25f, 1.0f), 1.0f, 0.14f);
		Hover.Shape = EEGChessMarkerShape::Frame;
		Hover.FrameWidth = 0.13f;
		Hover.FillColor = FLinearColor(1.0f, 0.7f, 0.2f, 0.35f);
		Hover.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.9f);
		Hover.OutlineWidth = 0.04f;
		Hover.PulseAmount = 0.35f;
		Hover.PulsePeriod = 0.9f;
	}
	AddMarker(EEGChessMarkType::Illegal, FLinearColor(1.0f, 0.05f, 0.05f, 0.6f), 1.0f, 0.15f);
}

void UEGChessTableStyle::GatherSoftPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	auto Add = [&OutPaths](const FSoftObjectPath& Path)
	{
		if (Path.IsValid())
		{
			OutPaths.AddUnique(Path);
		}
	};
	Add(TableMesh.ToSoftObjectPath());
	Add(ChairMesh.ToSoftObjectPath());
	Add(BoardMesh.ToSoftObjectPath());
	Add(ClockMesh.ToSoftObjectPath());
	Add(ClockButtonMesh.ToSoftObjectPath());
	Add(ClockHandMesh.ToSoftObjectPath());
	for (const TPair<EEGChessMarkType, FEGChessMarkerVisual>& Pair : Markers)
	{
		Add(Pair.Value.Mesh.ToSoftObjectPath());
		Add(Pair.Value.Material.ToSoftObjectPath());
	}
}
