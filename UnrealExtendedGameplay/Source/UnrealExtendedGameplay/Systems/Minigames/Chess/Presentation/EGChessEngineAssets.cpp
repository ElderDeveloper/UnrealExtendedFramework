// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessEngineAssets.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace EGChessEngineAssets
{
	template <typename T>
	T* LoadCached(const TCHAR* Path)
	{
		// Engine content never unloads, so a weak cache is enough and costs one load per asset.
		static TMap<FString, TWeakObjectPtr<UObject>> Cache;
		if (const TWeakObjectPtr<UObject>* Found = Cache.Find(Path))
		{
			if (T* Existing = Cast<T>(Found->Get()))
			{
				return Existing;
			}
		}
		T* Loaded = LoadObject<T>(nullptr, Path);
		Cache.Add(Path, Loaded);
		return Loaded;
	}

	UStaticMesh* Plane() { return LoadCached<UStaticMesh>(TEXT("/Engine/BasicShapes/Plane.Plane")); }
	UStaticMesh* Cube() { return LoadCached<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube.Cube")); }
	UStaticMesh* Cylinder() { return LoadCached<UStaticMesh>(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); }
	UStaticMesh* Cone() { return LoadCached<UStaticMesh>(TEXT("/Engine/BasicShapes/Cone.Cone")); }
	UStaticMesh* Sphere() { return LoadCached<UStaticMesh>(TEXT("/Engine/BasicShapes/Sphere.Sphere")); }

	UMaterialInterface* BasicMaterial()
	{
		return LoadCached<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	UMaterialInstanceDynamic* TintedMaterial(UObject* Outer, const FLinearColor& Color)
	{
		UMaterialInterface* Base = BasicMaterial();
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, Outer);
		Instance->SetVectorParameterValue(TEXT("Color"), Color);
		return Instance;
	}
}
