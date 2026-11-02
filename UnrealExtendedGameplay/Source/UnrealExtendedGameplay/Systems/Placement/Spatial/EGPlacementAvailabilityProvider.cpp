// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementAvailabilityProvider.h"

#include "Engine/World.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

bool UEGPlacementAvailabilityProvider::IsAreaAvailable_Implementation(UWorld* World, const FBox& Bounds) const
{
	if (!World || !World->IsGameWorld() || !World->GetWorldPartition() || !Bounds.IsValid)
	{
		return true;
	}
	const UWorldPartitionSubsystem* WorldPartition = World->GetSubsystem<UWorldPartitionSubsystem>();
	if (!WorldPartition)
	{
		return true;
	}
	FWorldPartitionStreamingQuerySource Source(Bounds.GetCenter());
	Source.Radius = static_cast<float>(Bounds.GetExtent().Size());
	Source.bUseGridLoadingRange = false;
	Source.bIncludeAnyDataLayer = true;
	// Regions register when their components register, which happens when a cell is activated.
	return WorldPartition->IsStreamingCompleted(EWorldPartitionRuntimeCellState::Activated, { Source }, false);
}
