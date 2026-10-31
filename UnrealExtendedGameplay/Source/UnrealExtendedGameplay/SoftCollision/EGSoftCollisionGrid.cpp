// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSoftCollisionGrid.h"

void FEGSoftCollisionGrid::Reset(float InCellSize)
{
	CellSize = FMath::Max(InCellSize, 1.0f);
	InvCellSize = 1.0 / static_cast<double>(CellSize);
	Cells.Reset();
	NumEntries = 0;
}

void FEGSoftCollisionGrid::Add(int32 Id, const FIntPoint& Cell)
{
	Cells.FindOrAdd(Cell).Add(Id);
	++NumEntries;
}

bool FEGSoftCollisionGrid::Remove(int32 Id, const FIntPoint& Cell)
{
	FBucket* Bucket = Cells.Find(Cell);
	if (!Bucket)
	{
		return false;
	}

	const int32 Index = Bucket->Find(Id);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	Bucket->RemoveAtSwap(Index, EAllowShrinking::No);
	--NumEntries;
	if (Bucket->IsEmpty())
	{
		Cells.Remove(Cell);
	}
	return true;
}

void FEGSoftCollisionGrid::Move(int32 Id, const FIntPoint& From, const FIntPoint& To)
{
	if (From == To)
	{
		return;
	}
	// An Id missing from From is still added, so a stale cell can never lose an entry.
	Remove(Id, From);
	Add(Id, To);
}
