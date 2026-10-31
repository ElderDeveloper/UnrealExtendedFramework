// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "UObject/SoftObjectPtr.h"

/**
 * Loading soft references without hitches.
 *
 * LoadSynchronous on the game thread stalls the frame until the asset and its dependencies are read
 * from disk. Preload what a system will need when it starts (BeginPlay, Initialize, when a data
 * asset is assigned), then Resolve at the point of use: Resolve returns the loaded asset without
 * touching the disk, and only falls back to a synchronous load (and says so in development builds)
 * when nothing preloaded it.
 *
 *     // When the component starts:
 *     PreloadHandle = EFAssets::Preload({ InputAction.ToSoftObjectPath() });
 *
 *     // Later, where it is used:
 *     if (UInputAction* Action = EFAssets::Resolve(InputAction, TEXT("UEGInteractionComponent"))) { ... }
 *
 * Keep the returned handle for as long as the assets must stay loaded; releasing it lets them unload.
 */
namespace EFAssets
{
	/**
	 * Starts loading Paths asynchronously through the foundation's streamable manager. OnLoaded
	 * runs on the game thread when everything is loaded, or immediately when it already was.
	 * Invalid paths are skipped. Returns null when there is nothing to load.
	 */
	EXTENDEDFOUNDATION_API TSharedPtr<FStreamableHandle> Preload(TArray<FSoftObjectPath> Paths, TFunction<void()> OnLoaded = nullptr);

	/**
	 * Development builds: logs once per asset that it was loaded synchronously at a point of use,
	 * so missing preloads show up in the log. Does nothing in Shipping.
	 */
	EXTENDEDFOUNDATION_API void ReportSynchronousLoad(const FSoftObjectPath& Path, const TCHAR* Context);

	/** The asset if it is loaded; otherwise loads it synchronously and reports that. Null for an unset reference. */
	template <typename T>
	T* Resolve(const TSoftObjectPtr<T>& Asset, const TCHAR* Context)
	{
		if (Asset.IsNull())
		{
			return nullptr;
		}
		if (T* Loaded = Asset.Get())
		{
			return Loaded;
		}
		ReportSynchronousLoad(Asset.ToSoftObjectPath(), Context);
		return Asset.LoadSynchronous();
	}

	/** Class version of Resolve. */
	template <typename T>
	UClass* Resolve(const TSoftClassPtr<T>& Class, const TCHAR* Context)
	{
		if (Class.IsNull())
		{
			return nullptr;
		}
		if (UClass* Loaded = Class.Get())
		{
			return Loaded;
		}
		ReportSynchronousLoad(Class.ToSoftObjectPath(), Context);
		return Class.LoadSynchronous();
	}
}
