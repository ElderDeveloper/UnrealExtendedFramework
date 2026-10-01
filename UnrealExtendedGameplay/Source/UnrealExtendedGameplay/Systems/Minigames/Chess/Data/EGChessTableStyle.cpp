// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessTableStyle.h"

UEGChessTableStyle::UEGChessTableStyle()
{
	BoardSurfaceTransform.SetLocation(FVector(-3.5f * SquareSize, -3.5f * SquareSize, 1.0f));

	auto AddMarker = [this](EEGChessMarkType Type, const FLinearColor& Color, float Scale, float Height)
	{
		FEGChessMarkerVisual Visual;
		Visual.Color = Color;
		Visual.Scale = Scale;
		Visual.HeightOffset = Height;
		Markers.Add(Type, Visual);
	};
	AddMarker(EEGChessMarkType::LastMoveFrom, FLinearColor(0.9f, 0.8f, 0.2f, 0.35f), 1.0f, 0.05f);
	AddMarker(EEGChessMarkType::LastMoveTo, FLinearColor(0.9f, 0.8f, 0.2f, 0.45f), 1.0f, 0.06f);
	AddMarker(EEGChessMarkType::Check, FLinearColor(0.9f, 0.1f, 0.1f, 0.6f), 1.0f, 0.07f);
	AddMarker(EEGChessMarkType::Selected, FLinearColor(0.3f, 0.6f, 1.0f, 0.5f), 1.0f, 0.08f);
	AddMarker(EEGChessMarkType::CastlePartner, FLinearColor(0.3f, 0.6f, 1.0f, 0.25f), 1.0f, 0.08f);
	AddMarker(EEGChessMarkType::HoverTarget, FLinearColor(0.3f, 0.6f, 1.0f, 0.6f), 1.0f, 0.09f);
	AddMarker(EEGChessMarkType::Move, FLinearColor(0.1f, 0.1f, 0.1f, 0.5f), 0.3f, 0.12f);
	AddMarker(EEGChessMarkType::Capture, FLinearColor(0.9f, 0.3f, 0.2f, 0.6f), 0.95f, 0.11f);
	AddMarker(EEGChessMarkType::Cursor, FLinearColor(1.0f, 1.0f, 1.0f, 0.7f), 1.0f, 0.13f);
	AddMarker(EEGChessMarkType::Illegal, FLinearColor(1.0f, 0.0f, 0.0f, 0.7f), 1.0f, 0.14f);
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
