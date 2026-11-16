// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementAvailabilityProvider.h"

#include "Engine/World.h"
#include "Misc/EngineVersionComparison.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#if UE_VERSION_OLDER_THAN(5, 8, 0)
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#endif

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
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 8, 0)
	Source.bIncludeAnyDataLayer = true;
#else
	// No bIncludeAnyDataLayer before UE 5.8: a query returns a data layer's cells only when it names the layer.
	if (const UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(World))
	{
		DataLayerManager->ForEachDataLayerInstance([&Source](UDataLayerInstance* DataLayer)
		{
			Source.DataLayers.Add(DataLayer->GetDataLayerFName());
			return true;
		});
	}
#endif
	// Regions register when their components register, which happens when a cell is activated.
	return WorldPartition->IsStreamingCompleted(EWorldPartitionRuntimeCellState::Activated, { Source }, false);
}
