// Copyright Moon Punch Games. All Rights Reserved.

#include "EEUIFixtureProviderRegistry.h"

#if WITH_EDITOR

FEEUIFixtureProviderRegistry& FEEUIFixtureProviderRegistry::Get()
{
	static FEEUIFixtureProviderRegistry Instance;
	return Instance;
}

void FEEUIFixtureProviderRegistry::RegisterFactory(const TSharedRef<IEEUIFixtureProviderFactory>& Factory)
{
	Factories.AddUnique(Factory);
}

void FEEUIFixtureProviderRegistry::UnregisterFactory(const TSharedRef<IEEUIFixtureProviderFactory>& Factory)
{
	Factories.Remove(Factory);
}

TSharedPtr<IEEUIFixtureProvider> FEEUIFixtureProviderRegistry::CreateProvider(const FName ProviderType, const FEEUIFixtureContext& Context) const
{
	for (const TSharedRef<IEEUIFixtureProviderFactory>& Factory : Factories)
	{
		if (Factory->CanProvide(ProviderType))
		{
			if (TSharedPtr<IEEUIFixtureProvider> Provider = Factory->CreateProvider(Context))
			{
				return Provider;
			}
		}
	}
	return nullptr;
}

#endif // WITH_EDITOR
