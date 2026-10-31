// Copyright Moon Punch Games. All Rights Reserved.

#include "Assets/EFAssetLoading.h"

#include "ExtendedFoundationModule.h"

namespace EFAssetLoadingPrivate
{
	/** One streamable manager for every Extended plugin; independent of the project's asset manager setup. */
	FStreamableManager& GetStreamableManager()
	{
		static FStreamableManager Manager;
		return Manager;
	}
}

TSharedPtr<FStreamableHandle> EFAssets::Preload(TArray<FSoftObjectPath> Paths, TFunction<void()> OnLoaded)
{
	Paths.RemoveAll([](const FSoftObjectPath& Path) { return Path.IsNull(); });
	if (Paths.Num() == 0)
	{
		if (OnLoaded)
		{
			OnLoaded();
		}
		return nullptr;
	}

	FStreamableDelegate Delegate;
	if (OnLoaded)
	{
		Delegate = FStreamableDelegate::CreateLambda([Callback = MoveTemp(OnLoaded)]() { Callback(); });
	}
	return EFAssetLoadingPrivate::GetStreamableManager().RequestAsyncLoad(MoveTemp(Paths), MoveTemp(Delegate));
}

void EFAssets::ReportSynchronousLoad(const FSoftObjectPath& Path, const TCHAR* Context)
{
#if !UE_BUILD_SHIPPING
	static TSet<FSoftObjectPath> Reported;
	if (IsInGameThread() && !Reported.Contains(Path))
	{
		Reported.Add(Path);
		UE_LOG(LogExtendedFoundation, Log, TEXT("%s loaded %s synchronously. Preload it (EFAssets::Preload) to avoid a hitch."),
			Context ? Context : TEXT("Unknown"), *Path.ToString());
	}
#endif
}
