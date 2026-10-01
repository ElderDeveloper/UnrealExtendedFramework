// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UObject;
class UStaticMesh;

/**
 * The plugin ships no content. Where a game assigned nothing, these engine assets (always
 * present under /Engine) stand in, so a table placed with an empty config still plays.
 */
namespace EGChessEngineAssets
{
	UNREALEXTENDEDGAMEPLAY_API UStaticMesh* Plane();
	UNREALEXTENDEDGAMEPLAY_API UStaticMesh* Cube();
	UNREALEXTENDEDGAMEPLAY_API UStaticMesh* Cylinder();
	UNREALEXTENDEDGAMEPLAY_API UStaticMesh* Cone();
	UNREALEXTENDEDGAMEPLAY_API UStaticMesh* Sphere();

	/** Opaque engine material with a "Color" vector parameter. */
	UNREALEXTENDEDGAMEPLAY_API UMaterialInterface* BasicMaterial();

	/** A dynamic instance of BasicMaterial tinted to Color. */
	UNREALEXTENDEDGAMEPLAY_API UMaterialInstanceDynamic* TintedMaterial(UObject* Outer, const FLinearColor& Color);

	/** Loads a soft reference that should already be resident; synchronous as a last resort. */
	template <typename T>
	T* Resolve(const TSoftObjectPtr<T>& Soft)
	{
		if (Soft.IsNull())
		{
			return nullptr;
		}
		T* Loaded = Soft.Get();
		return Loaded ? Loaded : Soft.LoadSynchronous();
	}

	template <typename T>
	UClass* ResolveClass(const TSoftClassPtr<T>& Soft)
	{
		if (Soft.IsNull())
		{
			return nullptr;
		}
		UClass* Loaded = Soft.Get();
		return Loaded ? Loaded : Soft.LoadSynchronous();
	}
}
