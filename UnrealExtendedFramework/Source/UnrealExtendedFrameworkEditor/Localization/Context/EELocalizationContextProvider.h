// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

struct FEELocEntry;

/** One labelled context fact about a localized entry. */
struct FEELocContextField
{
	FString Label;
	FString Value;
};

/** Aggregated context for an entry, built by all matching providers. */
struct FEELocContext
{
	TArray<FEELocContextField> Fields;

	void Add(const FString& Label, const FString& Value)
	{
		if (!Value.IsEmpty())
		{
			Fields.Add({Label, Value});
		}
	}
};

/**
 * LW-3 context provider contract (the plan's IEFLocalizationContextProvider).
 *
 * Registered by editor modules only — project adapters (quest, item, dialogue, ...) implement
 * this to describe where, why, and under what constraints their text appears. Multiple providers
 * may describe one entry; contexts aggregate.
 */
class IEELocalizationContextProvider
{
public:
	virtual ~IEELocalizationContextProvider() = default;

	virtual FName GetProviderName() const = 0;
	virtual bool CanDescribe(const FEELocEntry& Entry) const = 0;
	virtual void BuildContext(const FEELocEntry& Entry, FEELocContext& OutContext) const = 0;

	/** Opens the owning asset/source location. Returns true when handled. */
	virtual bool OpenUsage(const FEELocEntry& Entry) const { return false; }

	/** Optional: names of related entries (same screen/dialogue/etc.) for translator context. */
	virtual void EnumerateRelatedEntries(const FEELocEntry& Entry, TArray<FString>& OutRelated) const {}
};

/** Global registry; built-in providers self-register on first use. */
class UNREALEXTENDEDFRAMEWORKEDITOR_API FEELocContextProviderRegistry
{
public:
	static FEELocContextProviderRegistry& Get();

	void Register(const TSharedRef<IEELocalizationContextProvider>& Provider);
	void Unregister(const TSharedRef<IEELocalizationContextProvider>& Provider);

	/** Aggregates context from every provider that can describe the entry. */
	void BuildContext(const FEELocEntry& Entry, FEELocContext& OutContext) const;

	/** Tries providers in registration order until one opens the usage. */
	bool OpenUsage(const FEELocEntry& Entry) const;

private:
	FEELocContextProviderRegistry();
	TArray<TSharedRef<IEELocalizationContextProvider>> Providers;
};

#endif // WITH_EDITOR
