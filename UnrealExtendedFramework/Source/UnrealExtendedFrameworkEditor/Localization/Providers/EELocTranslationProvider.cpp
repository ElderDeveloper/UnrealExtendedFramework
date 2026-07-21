// Copyright Moon Punch Games. All Rights Reserved.

#include "EELocTranslationProvider.h"

#if WITH_EDITOR

FEELocTranslationProviderRegistry& FEELocTranslationProviderRegistry::Get()
{
	static FEELocTranslationProviderRegistry Instance;
	return Instance;
}

void FEELocTranslationProviderRegistry::Register(const TSharedRef<IEELocTranslationProvider>& Provider)
{
	Providers.AddUnique(Provider);
}

void FEELocTranslationProviderRegistry::Unregister(const TSharedRef<IEELocTranslationProvider>& Provider)
{
	Providers.Remove(Provider);
}

TSharedPtr<IEELocTranslationProvider> FEELocTranslationProviderRegistry::GetFirstAvailable(FString& OutUnavailableReason) const
{
	if (Providers.Num() == 0)
	{
		OutUnavailableReason = TEXT("No translation provider registered. The workbench is fully functional without one; register an IEELocTranslationProvider from an editor module to enable machine drafts.");
		return nullptr;
	}

	for (const TSharedRef<IEELocTranslationProvider>& Provider : Providers)
	{
		FString Reason;
		if (Provider->IsAvailable(Reason))
		{
			return Provider;
		}
		OutUnavailableReason = FString::Printf(TEXT("%s: %s"), *Provider->GetProviderName().ToString(), *Reason);
	}

	return nullptr;
}

#endif // WITH_EDITOR
