// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * FEGSoftCollisionGrid
 * Uniform 2D hash grid of registry indices. Plain C++, so it is unit tested and read by worker
 * threads while nothing writes to it.
 *
 * The owner keeps the cell size at least twice the largest push extent in the world, so every
 * pawn that can touch another is in one of the 3x3 cells around it. Entries are re-binned only
 * when their cell changes.
 */
class UNREALEXTENDEDGAMEPLAY_API FEGSoftCollisionGrid
{
public:
	using FBucket = TArray<int32, TInlineAllocator<8>>;

	explicit FEGSoftCollisionGrid(float InCellSize = 200.0f) { Reset(InCellSize); }

	/** Empties the grid and changes its cell size. */
	void Reset(float InCellSize);

	float GetCellSize() const { return CellSize; }
	int32 GetNumCells() const { return Cells.Num(); }
	int32 GetNumEntries() const { return NumEntries; }

	FIntPoint GetCell(const FVector& Location) const
	{
		return FIntPoint(FMath::FloorToInt32(Location.X * InvCellSize), FMath::FloorToInt32(Location.Y * InvCellSize));
	}

	void Add(int32 Id, const FIntPoint& Cell);

	/** Returns false when Id was not in Cell. */
	bool Remove(int32 Id, const FIntPoint& Cell);

	void Move(int32 Id, const FIntPoint& From, const FIntPoint& To);

	const FBucket* FindBucket(const FIntPoint& Cell) const { return Cells.Find(Cell); }

	/** Visits every Id in the (2 * Radius + 1)^2 cells centred on Cell. Radius 1 is the 3x3 neighbourhood. */
	template <typename TVisitor>
	void ForEachNear(const FIntPoint& Cell, TVisitor&& Visitor, int32 Radius = 1) const
	{
		for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
		{
			for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
			{
				if (const FBucket* Bucket = Cells.Find(FIntPoint(Cell.X + OffsetX, Cell.Y + OffsetY)))
				{
					for (const int32 Id : *Bucket)
					{
						Visitor(Id);
					}
				}
			}
		}
	}

	template <typename TFunc>
	void ForEachCell(TFunc&& Func) const
	{
		for (const TPair<FIntPoint, FBucket>& Pair : Cells)
		{
			Func(Pair.Key, Pair.Value);
		}
	}

private:
	TMap<FIntPoint, FBucket> Cells;
	float CellSize = 200.0f;
	double InvCellSize = 1.0 / 200.0;
	int32 NumEntries = 0;
};
