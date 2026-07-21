// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

class UEFUIFixture;
class UEFUIFixtureProviderConfig;
class UUserWidget;
class UWorld;

/** Everything a provider needs about the current fixture host. */
struct FEEUIFixtureContext
{
	/** The fixture being applied (may be null when hosting a bare widget class). */
	const UEFUIFixture* Fixture = nullptr;
	/** The instantiated widget. */
	UUserWidget* Widget = nullptr;
	/** World the widget was created in (editor world for Fast Host, preview world for the sandbox). */
	UWorld* World = nullptr;
};

/** A live provider instance created for one fixture session. */
class IEEUIFixtureProvider
{
public:
	virtual ~IEEUIFixtureProvider() = default;

	/** Applies the typed state from the config to the hosted widget/session. */
	virtual void ApplyState(const FEEUIFixtureContext& Context, const UEFUIFixtureProviderConfig* Config) = 0;

	/** Restores any external state touched by ApplyState. Called when the fixture closes. */
	virtual void Reset() = 0;
};

/**
 * Factory registered by editor adapter modules (framework, gameplay, or game editor modules).
 * Matches the plan's IEFUIFixtureProviderFactory contract: CanProvide/CreateProvider, with
 * ApplyState/Reset living on the created provider instance.
 */
class IEEUIFixtureProviderFactory
{
public:
	virtual ~IEEUIFixtureProviderFactory() = default;

	virtual bool CanProvide(FName ProviderType) const = 0;
	virtual TSharedPtr<IEEUIFixtureProvider> CreateProvider(const FEEUIFixtureContext& Context) = 0;
};

/**
 * Global registry connecting provider configs (data on fixtures) to provider factories
 * (registered by editor modules). The UI Lab never hardcodes a project provider.
 */
class UNREALEXTENDEDFRAMEWORKEDITOR_API FEEUIFixtureProviderRegistry
{
public:
	static FEEUIFixtureProviderRegistry& Get();

	void RegisterFactory(const TSharedRef<IEEUIFixtureProviderFactory>& Factory);
	void UnregisterFactory(const TSharedRef<IEEUIFixtureProviderFactory>& Factory);

	/** Creates a provider for the given type, or null when no registered factory can provide it. */
	TSharedPtr<IEEUIFixtureProvider> CreateProvider(FName ProviderType, const FEEUIFixtureContext& Context) const;

private:
	TArray<TSharedRef<IEEUIFixtureProviderFactory>> Factories;
};

#endif // WITH_EDITOR
