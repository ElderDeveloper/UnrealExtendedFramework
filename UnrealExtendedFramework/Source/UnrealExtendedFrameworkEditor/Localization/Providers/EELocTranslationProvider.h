// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

/** Everything a translation provider receives per entry (LW-8 / plan 2.9). */
struct FEELocTranslationRequest
{
	FString Namespace;
	FString Key;
	FString SourceText;
	FString NativeCulture;
	FString TargetCulture;
	/** Aggregated context-provider fields, human-readable. */
	FString ContextText;
	/** Glossary constraints applying to this entry (approved terms, do-not-translate). */
	TArray<FString> GlossaryNotes;
	/** Placeholders/tags that must be preserved verbatim. */
	TArray<FString> ProtectedTokens;
	/** 0 = unconstrained. */
	int32 MaxPreferredLength = 0;
};

/**
 * LW-8 optional machine-translation provider contract.
 *
 * Rules enforced by the workbench, not the provider:
 *  - returned text is always staged as Machine Draft (never auto-saved, never auto-reviewed);
 *  - Locked translations are never requested or overwritten;
 *  - the user reviews staged drafts in the grid before Save writes archives;
 *  - provider settings involving secrets stay per-user (never in project config/VCS);
 *  - the entire workbench is fully functional with zero providers registered.
 */
class IEELocTranslationProvider
{
public:
	virtual ~IEELocTranslationProvider() = default;

	virtual FName GetProviderName() const = 0;

	/** False (with reason) when unconfigured — e.g. missing per-user credentials. */
	virtual bool IsAvailable(FString& OutReason) const = 0;

	/**
	 * Translates a batch. OutDrafts is keyed by "Namespace,Key". Identifiers and
	 * ProtectedTokens must survive verbatim; violations are dropped by the caller.
	 */
	virtual bool Translate(const TArray<FEELocTranslationRequest>& Requests,
		TMap<FString, FString>& OutDrafts, FString& OutError) = 0;

	/** Optional human-readable cost/request estimate shown before running. */
	virtual FText GetCostEstimate(int32 RequestCount) const { return FText::GetEmpty(); }
};

/** Registry for translation providers; editor modules register at startup. */
class UNREALEXTENDEDFRAMEWORKEDITOR_API FEELocTranslationProviderRegistry
{
public:
	static FEELocTranslationProviderRegistry& Get();

	void Register(const TSharedRef<IEELocTranslationProvider>& Provider);
	void Unregister(const TSharedRef<IEELocTranslationProvider>& Provider);

	const TArray<TSharedRef<IEELocTranslationProvider>>& GetProviders() const { return Providers; }

	/** First available provider, or null (the normal zero-provider state). */
	TSharedPtr<IEELocTranslationProvider> GetFirstAvailable(FString& OutUnavailableReason) const;

private:
	TArray<TSharedRef<IEELocTranslationProvider>> Providers;
};

#endif // WITH_EDITOR
