// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementAnchorComponent.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementRegionComponent.h"

#include "EGPlacementSpatialSubsystem.generated.h"

class UEGPlacementAvailabilityProvider;

namespace EGPlacementSpatial
{
	/** A hash of a transform quantized to 1 cm, 0.5 degrees and 1/1000 scale. Never 0. */
	UNREALEXTENDEDGAMEPLAY_API uint32 HashTransform(const FTransform& Transform);

	/** Folds a hash into a non-zero revision. */
	inline int32 ToRevision(uint32 Hash) { return Hash == 0 ? 1 : static_cast<int32>(Hash); }

	/**
	 * A uniform grid of cells holding weak entries. Entries spanning more than MaxCellsPerEntry
	 * cells go to an oversized list that every query checks, so a huge zone costs one entry.
	 */
	template <typename T>
	class TSpatialHash
	{
	public:
		void Insert(T* Entry, const FBox& Bounds, double CellSize, int32 MaxCellsPerEntry)
		{
			Remove(Entry);
			const FIntVector Min = ToCell(Bounds.Min, CellSize);
			const FIntVector Max = ToCell(Bounds.Max, CellSize);
			const int64 Count = int64(Max.X - Min.X + 1) * int64(Max.Y - Min.Y + 1) * int64(Max.Z - Min.Z + 1);
			TWeakObjectPtr<T> Weak(Entry);
			if (Count > MaxCellsPerEntry)
			{
				Oversized.Add(Weak);
				return;
			}
			TArray<FIntVector>& Keys = KeysOf.Add(Weak);
			for (int32 X = Min.X; X <= Max.X; ++X)
			{
				for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
				{
					for (int32 Z = Min.Z; Z <= Max.Z; ++Z)
					{
						const FIntVector Key(X, Y, Z);
						Cells.FindOrAdd(Key).Add(Weak);
						Keys.Add(Key);
					}
				}
			}
		}

		void Remove(T* Entry)
		{
			TWeakObjectPtr<T> Weak(Entry);
			Oversized.Remove(Weak);
			TArray<FIntVector> Keys;
			if (KeysOf.RemoveAndCopyValue(Weak, Keys))
			{
				for (const FIntVector& Key : Keys)
				{
					if (TArray<TWeakObjectPtr<T>>* Cell = Cells.Find(Key))
					{
						Cell->RemoveSwap(Weak);
						if (Cell->Num() == 0)
						{
							Cells.Remove(Key);
						}
					}
				}
			}
		}

		void Query(const FBox& Bounds, double CellSize, TArray<T*>& Out) const
		{
			TSet<T*> Seen;
			auto Consider = [&Seen, &Out](const TWeakObjectPtr<T>& Weak)
			{
				if (T* Entry = Weak.Get())
				{
					bool bAlreadySeen = false;
					Seen.Add(Entry, &bAlreadySeen);
					if (!bAlreadySeen)
					{
						Out.Add(Entry);
					}
				}
			};
			for (const TWeakObjectPtr<T>& Weak : Oversized)
			{
				Consider(Weak);
			}
			const FIntVector Min = ToCell(Bounds.Min, CellSize);
			const FIntVector Max = ToCell(Bounds.Max, CellSize);
			const int64 Count = int64(Max.X - Min.X + 1) * int64(Max.Y - Min.Y + 1) * int64(Max.Z - Min.Z + 1);
			if (Count > Cells.Num())
			{
				// A query larger than the populated grid: walk the cells instead of the volume.
				for (const TPair<FIntVector, TArray<TWeakObjectPtr<T>>>& Pair : Cells)
				{
					if (Pair.Key.X >= Min.X && Pair.Key.X <= Max.X && Pair.Key.Y >= Min.Y && Pair.Key.Y <= Max.Y && Pair.Key.Z >= Min.Z && Pair.Key.Z <= Max.Z)
					{
						for (const TWeakObjectPtr<T>& Weak : Pair.Value)
						{
							Consider(Weak);
						}
					}
				}
				return;
			}
			for (int32 X = Min.X; X <= Max.X; ++X)
			{
				for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
				{
					for (int32 Z = Min.Z; Z <= Max.Z; ++Z)
					{
						if (const TArray<TWeakObjectPtr<T>>* Cell = Cells.Find(FIntVector(X, Y, Z)))
						{
							for (const TWeakObjectPtr<T>& Weak : *Cell)
							{
								Consider(Weak);
							}
						}
					}
				}
			}
		}

		template <typename FunctionType>
		void ForEach(FunctionType&& Function) const
		{
			for (const TWeakObjectPtr<T>& Weak : Oversized)
			{
				if (T* Entry = Weak.Get())
				{
					Function(*Entry);
				}
			}
			for (const TPair<TWeakObjectPtr<T>, TArray<FIntVector>>& Pair : KeysOf)
			{
				if (T* Entry = Pair.Key.Get())
				{
					Function(*Entry);
				}
			}
		}

		int32 Num() const { return KeysOf.Num() + Oversized.Num(); }
		void Reset() { Cells.Reset(); KeysOf.Reset(); Oversized.Reset(); }

	private:
		static FIntVector ToCell(const FVector& Location, double CellSize)
		{
			return FIntVector(
				FMath::FloorToInt32(Location.X / CellSize),
				FMath::FloorToInt32(Location.Y / CellSize),
				FMath::FloorToInt32(Location.Z / CellSize));
		}

		TMap<FIntVector, TArray<TWeakObjectPtr<T>>> Cells;
		TMap<TWeakObjectPtr<T>, TArray<FIntVector>> KeysOf;
		TArray<TWeakObjectPtr<T>> Oversized;
	};
}

/**
 * Finds placement regions and anchors by area. Components register themselves when they register
 * and update their cells when they move; there are no per-frame scans and no physics volumes.
 *
 * Also answers whether an area's regions are known to be loaded, through the availability
 * provider (Project Settings > Extended Placement). Unloaded data is not a refusal: region rules
 * answer Pending there.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementSpatialSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UEGPlacementSpatialSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	void RegisterRegion(UEGPlacementRegionComponent* Region);
	void UnregisterRegion(UEGPlacementRegionComponent* Region);
	void RegisterAnchor(UEGPlacementAnchorComponent* Anchor);
	void UnregisterAnchor(UEGPlacementAnchorComponent* Anchor);

	/** Regions whose world bounds overlap Bounds. Rules test the boxes exactly. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	void QueryRegions(const FBox& Bounds, TArray<UEGPlacementRegionComponent*>& OutRegions) const;

	/** Anchors within Radius of Location. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	void QueryAnchors(FVector Location, float Radius, TArray<UEGPlacementAnchorComponent*>& OutAnchors) const;

	/** Whether the regions inside Bounds are known to be loaded. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	bool IsAreaAvailable(const FBox& Bounds) const;

	/** Replaces the availability provider for this world. Null restores the project default. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	void SetAvailabilityProvider(UEGPlacementAvailabilityProvider* Provider);

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	UEGPlacementAvailabilityProvider* GetAvailabilityProvider() const { return Availability; }

	int32 GetNumRegions() const { return Regions.Num(); }
	int32 GetNumAnchors() const { return Anchors.Num(); }
	int64 GetQueryCount() const { return QueryCount; }

	void ForEachRegion(TFunctionRef<void(UEGPlacementRegionComponent&)> Function) const;
	void ForEachAnchor(TFunctionRef<void(UEGPlacementAnchorComponent&)> Function) const;

private:
	void CreateDefaultProvider();

	EGPlacementSpatial::TSpatialHash<UEGPlacementRegionComponent> Regions;
	EGPlacementSpatial::TSpatialHash<UEGPlacementAnchorComponent> Anchors;

	UPROPERTY(Transient)
	TObjectPtr<UEGPlacementAvailabilityProvider> Availability;

	double CellSize = 1000.0;
	int32 MaxCellsPerEntry = 512;
	mutable int64 QueryCount = 0;
};
