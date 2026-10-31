// Copyright Moon Punch Games. All Rights Reserved.

#include "EFLogRegistry.h"

#include "EFLog.h"
#include "HAL/PlatformMisc.h"
#include "Misc/ScopeRWLock.h"

FEFLogRegistry& FEFLogRegistry::Get()
{
	// Leaked on purpose: call sites hold pointers into it and may log while static destructors run.
	static FEFLogRegistry* Instance = new FEFLogRegistry();
	return *Instance;
}

FEFLogCategory& FEFLogRegistry::FindOrAdd(FStringView RawName)
{
	const FString DisplayName = EFLog::SanitizeCategoryName(RawName);
	const FName Key(*DisplayName);

	{
		FReadScopeLock ReadLock(Lock);
		if (FEFLogCategory* const* Existing = Categories.Find(Key))
		{
			return **Existing;
		}
	}

	FWriteScopeLock WriteLock(Lock);

	// Another thread may have created it between the two locks.
	if (FEFLogCategory* const* Existing = Categories.Find(Key))
	{
		return **Existing;
	}

	FEFLogCategory* Category = new FEFLogCategory(Key, DisplayName, GetConfiguredVerbosity(Key));
	Categories.Add(Key, Category);
	PendingRuntimeVerbosity.Remove(Key);

	// Said once, when the category is created: a name that arrived as data ("Hostage Follow") is
	// written under a different file name than the one asked for.
	const FString Requested = FString(RawName).TrimStartAndEnd();
	if (!Requested.Equals(DisplayName, ESearchCase::CaseSensitive))
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("EFLog: category '%s' is written as '%s.log'.\n"), *Requested, *DisplayName);
	}

	return *Category;
}

FEFLogCategory* FEFLogRegistry::Find(FName RawName) const
{
	if (RawName.IsNone())
	{
		return nullptr;
	}

	const FName Key(*EFLog::SanitizeCategoryName(RawName.ToString()));

	FReadScopeLock ReadLock(Lock);
	FEFLogCategory* const* Existing = Categories.Find(Key);
	return Existing ? *Existing : nullptr;
}

void FEFLogRegistry::ForEach(TFunctionRef<void(FEFLogCategory&)> Visitor) const
{
	FReadScopeLock ReadLock(Lock);
	for (const TPair<FName, FEFLogCategory*>& Pair : Categories)
	{
		Visitor(*Pair.Value);
	}
}

void FEFLogRegistry::SetConfiguredVerbosity(EEFLogVerbosity InDefaultVerbosity, const TMap<FName, EEFLogVerbosity>& PerCategory)
{
	FWriteScopeLock WriteLock(Lock);

	DefaultVerbosity = InDefaultVerbosity;
	PendingRuntimeVerbosity.Reset();

	// Keyed the same way categories are, so "araf" in the settings matches EF_LOG(Araf, ...).
	ConfiguredVerbosity.Reset();
	for (const TPair<FName, EEFLogVerbosity>& Pair : PerCategory)
	{
		if (!Pair.Key.IsNone())
		{
			ConfiguredVerbosity.Add(FName(*EFLog::SanitizeCategoryName(Pair.Key.ToString())), Pair.Value);
		}
	}

	for (const TPair<FName, FEFLogCategory*>& Pair : Categories)
	{
		Pair.Value->SetVerbosity(GetConfiguredVerbosity(Pair.Key));
	}
}

void FEFLogRegistry::SetCategoryVerbosity(FName RawName, EEFLogVerbosity Verbosity)
{
	if (RawName.IsNone())
	{
		return;
	}

	const FName Key(*EFLog::SanitizeCategoryName(RawName.ToString()));

	FWriteScopeLock WriteLock(Lock);
	if (FEFLogCategory* const* Existing = Categories.Find(Key))
	{
		(*Existing)->SetVerbosity(Verbosity);
	}
	else
	{
		PendingRuntimeVerbosity.Add(Key, Verbosity);
	}
}

void FEFLogRegistry::SetAllVerbosity(EEFLogVerbosity Verbosity)
{
	FWriteScopeLock WriteLock(Lock);

	DefaultVerbosity = Verbosity;
	PendingRuntimeVerbosity.Reset();
	for (const TPair<FName, FEFLogCategory*>& Pair : Categories)
	{
		Pair.Value->SetVerbosity(Verbosity);
	}
}

EEFLogVerbosity FEFLogRegistry::GetConfiguredVerbosity(FName Key) const
{
	// Called with the lock already held. A runtime threshold set before the category existed wins.
	if (const EEFLogVerbosity* Pending = PendingRuntimeVerbosity.Find(Key))
	{
		return *Pending;
	}

	const EEFLogVerbosity* Configured = ConfiguredVerbosity.Find(Key);
	return Configured ? *Configured : DefaultVerbosity;
}
