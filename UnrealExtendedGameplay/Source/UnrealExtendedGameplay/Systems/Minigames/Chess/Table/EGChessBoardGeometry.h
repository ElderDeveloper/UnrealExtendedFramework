// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Core/EGChessTypes.h"

/**
 * FEGChessBoardGeometry
 *
 * Squares to world and back. The frame is the table's BoardSurface component:
 *
 *  - its origin is the centre of the corner square on Seat A's left, on the playing surface;
 *  - its +X points from Seat A towards Seat B;
 *  - its +Y points to Seat A's right.
 *
 * With Seat A playing white that corner is a1, files run along +Y and ranks along +X. With Seat
 * B playing white the board is read from the opposite corner, so white always starts in front of
 * whoever plays white; nothing in the world moves.
 */
struct UNREALEXTENDEDGAMEPLAY_API FEGChessBoardGeometry
{
	FTransform SurfaceToWorld = FTransform::Identity;
	float SquareSize = 5.0f;
	EEGChessSeat WhiteSeat = EEGChessSeat::A;

	/** Grid cell in the surface frame: X towards Seat B, Y to Seat A's right. */
	FIntPoint SquareToGrid(int32 Square) const
	{
		const int32 File = EGChess::FileOf(Square);
		const int32 Rank = EGChess::RankOf(Square);
		return WhiteSeat == EEGChessSeat::A ? FIntPoint(Rank, File) : FIntPoint(7 - Rank, 7 - File);
	}

	int32 GridToSquare(FIntPoint Grid) const
	{
		if (Grid.X < 0 || Grid.X > 7 || Grid.Y < 0 || Grid.Y > 7)
		{
			return INDEX_NONE;
		}
		return WhiteSeat == EEGChessSeat::A ? EGChess::MakeSquare(Grid.Y, Grid.X) : EGChess::MakeSquare(7 - Grid.Y, 7 - Grid.X);
	}

	FVector SquareToLocal(int32 Square) const
	{
		const FIntPoint Grid = SquareToGrid(Square);
		return FVector(Grid.X * SquareSize, Grid.Y * SquareSize, 0.0f);
	}

	FVector SquareToWorld(int32 Square) const
	{
		return SurfaceToWorld.TransformPosition(SquareToLocal(Square));
	}

	FVector GetCenterWorld() const
	{
		return SurfaceToWorld.TransformPosition(FVector(3.5f * SquareSize, 3.5f * SquareSize, 0.0f));
	}

	FVector GetUpVector() const
	{
		return SurfaceToWorld.GetUnitAxis(EAxis::Z);
	}

	/** Square under a world point projected onto the surface. INDEX_NONE off the board. */
	int32 WorldToSquare(const FVector& WorldPoint) const
	{
		const FVector Local = SurfaceToWorld.InverseTransformPosition(WorldPoint);
		const FIntPoint Grid(FMath::RoundToInt(Local.X / SquareSize), FMath::RoundToInt(Local.Y / SquareSize));
		return GridToSquare(Grid);
	}

	/** Intersect a world ray with the playing surface. */
	bool RayToSquare(const FVector& Origin, const FVector& Direction, int32& OutSquare) const
	{
		const FVector LocalOrigin = SurfaceToWorld.InverseTransformPosition(Origin);
		const FVector LocalDir = SurfaceToWorld.InverseTransformVector(Direction);
		if (FMath::IsNearlyZero(LocalDir.Z))
		{
			return false;
		}
		const float T = -LocalOrigin.Z / LocalDir.Z;
		if (T < 0.0f)
		{
			return false;
		}
		const FVector Hit = LocalOrigin + LocalDir * T;
		const FIntPoint Grid(FMath::RoundToInt(Hit.X / SquareSize), FMath::RoundToInt(Hit.Y / SquareSize));
		OutSquare = GridToSquare(Grid);
		return OutSquare != INDEX_NONE;
	}

	/**
	 * Move a square one step in the direction a seated player means: +Up is away from that
	 * player, +Right is to their right. Returns the input square at the board's edge.
	 */
	int32 StepForSeat(int32 Square, EEGChessSeat Seat, int32 Right, int32 Up) const
	{
		FIntPoint Grid = SquareToGrid(Square);
		const int32 Sign = Seat == EEGChessSeat::A ? 1 : -1;
		Grid.X = FMath::Clamp(Grid.X + Up * Sign, 0, 7);
		Grid.Y = FMath::Clamp(Grid.Y + Right * Sign, 0, 7);
		return GridToSquare(Grid);
	}

	/** Yaw that makes a piece face away from its own side, in world space. */
	FQuat FacingFor(EEGChessColor Color) const
	{
		// White faces away from the white seat: +X when Seat A is white.
		const bool bFacesPlusX = (Color == EEGChessColor::White) == (WhiteSeat == EEGChessSeat::A);
		const FQuat Local = bFacesPlusX ? FQuat::Identity : FQuat(FVector::UpVector, PI);
		return SurfaceToWorld.GetRotation() * Local;
	}
};
