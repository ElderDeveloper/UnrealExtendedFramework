// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EFLogTypes.h"
#include "HAL/CriticalSection.h"

/**
 * Every EF_LOG category in the process, keyed by sanitized name (case-insensitive, since FName is).
 *
 * Categories are created on first use and never freed: each EF_LOG call site caches a raw pointer
 * to its category, so a category must outlive every call site, including ones that run during
 * module shutdown. The registry itself is leaked for the same reason.
 *
 * A new category starts at its configured threshold (UEFLogSettings::CategoryVerbosity), or the
 * default one. Settings can arrive after the first categories exist (a module may log before this
 * one has read its settings), which is why applying them also updates every existing category.
 */
class FEFLogRegistry
{
public:
	static FEFLogRegistry& Get();

	/** Sanitizes the name, then finds or creates the category. Safe on any thread. */
	FEFLogCategory& FindOrAdd(FStringView RawName);

	/** Finds an existing category; the name is sanitized first. Null when nothing has created it. */
	FEFLogCategory* Find(FName RawName) const;

	/** Visits every category under the read lock. The visitor must not create categories. */
	void ForEach(TFunctionRef<void(FEFLogCategory&)> Visitor) const;

	/**
	 * The thresholds from settings: applied to every existing category now, and to new ones as they
	 * appear. Settings win over earlier runtime changes, so pending runtime thresholds are dropped.
	 */
	void SetConfiguredVerbosity(EEFLogVerbosity DefaultVerbosity, const TMap<FName, EEFLogVerbosity>& PerCategory);

	/**
	 * A runtime threshold for one category (console, window, SetCategoryVerbosity). A category that
	 * does not exist yet is not created for it: the threshold waits for the category's first line, so
	 * the file is named by the EF_LOG call site's spelling, not the console's.
	 */
	void SetCategoryVerbosity(FName RawName, EEFLogVerbosity Verbosity);

	/** Changes the default for new categories and every existing one (EF.Log.Verbosity * <Level>). Per-category settings are kept for new categories. */
	void SetAllVerbosity(EEFLogVerbosity Verbosity);

private:
	FEFLogRegistry() = default;

	EEFLogVerbosity GetConfiguredVerbosity(FName Key) const;

	mutable FRWLock Lock;
	TMap<FName, FEFLogCategory*> Categories;
	TMap<FName, EEFLogVerbosity> ConfiguredVerbosity;
	TMap<FName, EEFLogVerbosity> PendingRuntimeVerbosity;
	EEFLogVerbosity DefaultVerbosity = EEFLogVerbosity::Log;
};
