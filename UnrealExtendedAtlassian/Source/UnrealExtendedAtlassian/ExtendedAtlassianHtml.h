// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianDocBlock.h"

/**
 * Converts Confluence's rendered HTML into readable plain text.
 *
 * The conversion is deliberately tolerant: it never fails, and unknown tags are dropped while their
 * text is kept. Structure is preserved with light Markdown-style markers (# headings, - bullets,
 * ``` code fences, | table cells) to match how Jira descriptions are flattened elsewhere in the
 * plugin, so both panes read the same way.
 *
 * Fidelity limits are real. Complex macros, embedded Jira tables and images degrade to text or
 * placeholders. Every page in the browser has an "Open in Browser" escape hatch for that reason.
 */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianHtml
{
public:
	/**
	 * Structured conversion for the document viewer.
	 *
	 * Emits blocks directly rather than going via Markdown: Confluence HTML is richer than Markdown,
	 * so converting down and re-parsing would lose detail twice and cap fidelity permanently.
	 */
	static TArray<FExtendedAtlassianDocBlock> ToBlocks(const FString& Html);

	/** Flat conversion, still used for the Jira panes and the bug-report context block. */
	static FString ToPlainText(const FString& Html);

	/** Decodes the named and numeric entities Confluence emits. */
	static FString DecodeEntities(const FString& In);
};
