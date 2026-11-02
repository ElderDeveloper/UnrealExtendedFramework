// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementSpatialSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementSettings.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTags.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementAvailabilityProvider.h"

namespace EGPlacementSpatial
{
	uint32 HashTransform(const FTransform& Transform)
	{
		const FVector Location = Transform.GetLocation();
		const FRotator Rotation = Transform.Rotator();
		const FVector Scale = Transform.GetScale3D();
		uint32 Hash = GetTypeHash(FIntVector(FMath::RoundToInt32(Location.X), FMath::RoundToInt32(Location.Y), FMath::RoundToInt32(Location.Z)));
		Hash = HashCombine(Hash, GetTypeHash(FIntVector(FMath::RoundToInt32(Rotation.Pitch * 2.0), FMath::RoundToInt32(Rotation.Yaw * 2.0), FMath::RoundToInt32(Rotation.Roll * 2.0))));
		Hash = HashCombine(Hash, GetTypeHash(FIntVector(FMath::RoundToInt32(Scale.X * 1000.0), FMath::RoundToInt32(Scale.Y * 1000.0), FMath::RoundToInt32(Scale.Z * 1000.0))));
		return Hash == 0 ? 1u : Hash;
	}
}

UEGPlacementSpatialSubsystem* UEGPlacementSpatialSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UEGPlacementSpatialSubsystem>() : nullptr;
}

void UEGPlacementSpatialSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const UEGPlacementSettings* Settings = UEGPlacementSettings::Get();
	CellSize = FMath::Max(50.0, static_cast<double>(Settings->SpatialCellSize));
	MaxCellsPerEntry = FMath::Max(1, Settings->MaxCellsPerEntry);
	CreateDefaultProvider();
}

void UEGPlacementSpatialSubsystem::Deinitialize()
{
	Regions.Reset();
	Anchors.Reset();
	Availability = nullptr;
	Super::Deinitialize();
}

bool UEGPlacementSpatialSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor || WorldType == EWorldType::EditorPreview || WorldType == EWorldType::GamePreview;
}

void UEGPlacementSpatialSubsystem::CreateDefaultProvider()
{
	UClass* ProviderClass = UEGPlacementAvailabilityProvider::StaticClass();
	const TSoftClassPtr<UEGPlacementAvailabilityProvider>& Configured = UEGPlacementSettings::Get()->AvailabilityProviderClass;
	if (!Configured.IsNull())
	{
		// Settings classes are small and needed before the first query; a world start is the right time to load one.
		if (UClass* Loaded = Configured.LoadSynchronous())
		{
			ProviderClass = Loaded;
		}
		else
		{
			UE_LOG(LogEGPlacement, Warning, TEXT("Placement availability provider %s could not be loaded; using World Partition streaming."), *Configured.ToString());
		}
	}
	Availability = NewObject<UEGPlacementAvailabilityProvider>(this, ProviderClass);
}

void UEGPlacementSpatialSubsystem::RegisterRegion(UEGPlacementRegionComponent* Region)
{
	if (Region)
	{
		Regions.Insert(Region, Region->GetWorldBox().GetBounds(), CellSize, MaxCellsPerEntry);
	}
}

void UEGPlacementSpatialSubsystem::UnregisterRegion(UEGPlacementRegionComponent* Region)
{
	Regions.Remove(Region);
}

void UEGPlacementSpatialSubsystem::RegisterAnchor(UEGPlacementAnchorComponent* Anchor)
{
	if (Anchor)
	{
		const FVector Location = Anchor->GetComponentLocation();
		Anchors.Insert(Anchor, FBox(Location, Location), CellSize, MaxCellsPerEntry);
	}
}

void UEGPlacementSpatialSubsystem::UnregisterAnchor(UEGPlacementAnchorComponent* Anchor)
{
	Anchors.Remove(Anchor);
}

void UEGPlacementSpatialSubsystem::QueryRegions(const FBox& Bounds, TArray<UEGPlacementRegionComponent*>& OutRegions) const
{
	++QueryCount;
	OutRegions.Reset();
	TArray<UEGPlacementRegionComponent*> Candidates;
	Regions.Query(Bounds, CellSize, Candidates);
	for (UEGPlacementRegionComponent* Region : Candidates)
	{
		if (Region->GetWorldBox().GetBounds().Intersect(Bounds))
		{
			OutRegions.Add(Region);
		}
	}
}

void UEGPlacementSpatialSubsystem::QueryAnchors(FVector Location, float Radius, TArray<UEGPlacementAnchorComponent*>& OutAnchors) const
{
	++QueryCount;
	OutAnchors.Reset();
	TArray<UEGPlacementAnchorComponent*> Candidates;
	Anchors.Query(FBox(Location - FVector(Radius), Location + FVector(Radius)), CellSize, Candidates);
	const double RadiusSquared = FMath::Square(static_cast<double>(Radius));
	for (UEGPlacementAnchorComponent* Anchor : Candidates)
	{
		if (FVector::DistSquared(Anchor->GetComponentLocation(), Location) <= RadiusSquared)
		{
			OutAnchors.Add(Anchor);
		}
	}
}

bool UEGPlacementSpatialSubsystem::IsAreaAvailable(const FBox& Bounds) const
{
	return !Availability || Availability->IsAreaAvailable(GetWorld(), Bounds);
}

void UEGPlacementSpatialSubsystem::SetAvailabilityProvider(UEGPlacementAvailabilityProvider* Provider)
{
	if (Provider)
	{
		Availability = Provider;
	}
	else
	{
		CreateDefaultProvider();
	}
}

void UEGPlacementSpatialSubsystem::ForEachRegion(TFunctionRef<void(UEGPlacementRegionComponent&)> Function) const
{
	Regions.ForEach([&Function](UEGPlacementRegionComponent& Region) { Function(Region); });
}

void UEGPlacementSpatialSubsystem::ForEachAnchor(TFunctionRef<void(UEGPlacementAnchorComponent&)> Function) const
{
	Anchors.ForEach([&Function](UEGPlacementAnchorComponent& Anchor) { Function(Anchor); });
}
