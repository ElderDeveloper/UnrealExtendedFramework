// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Confluence storage format — the XHTML dialect Confluence stores and accepts on write.
 *
 * Reading for display uses body-format=view (rendered, macros expanded). Editing must use storage,
 * because storage is what a write accepts, and because macros are *visible* in storage as
 * <ac:structured-macro> elements. That visibility is the whole safety mechanism: a lossy read
 * becomes a destructive write the moment saving is possible, so a page whose constructs cannot be
 * rebuilt is detected here and never offered for editing.
 */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianStorage
{
public:
	/** Storage XHTML to Markdown, for the working copy in Saved/Documents. */
	static FString ToMarkdown(const FString& Storage);

	/** Markdown back to storage XHTML, for writing to Confluence. */
	static FString FromMarkdown(const FString& Markdown);

	/**
	 * Reports constructs this plugin cannot faithfully rebuild.
	 *
	 * Returns true when the page is safe to edit. When false, OutReasons names what was found so
	 * the UI can explain the refusal rather than just disabling a button.
	 */
	static bool CanRoundTrip(const FString& Storage, TArray<FString>& OutReasons);
};
