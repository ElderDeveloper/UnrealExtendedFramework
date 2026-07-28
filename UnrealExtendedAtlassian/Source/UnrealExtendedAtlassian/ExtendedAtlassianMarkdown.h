// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianDocBlock.h"

/**
 * Markdown to document blocks.
 *
 * Deliberately a subset, not CommonMark. Chasing the full spec is a tar pit of edge cases that real
 * documents never exercise, so this targets what design docs and repo READMEs actually contain:
 *
 *   ATX headings, paragraphs, hard breaks, nested bullet and ordered lists, task lists,
 *   fenced and inline code, bold / italic / strikethrough, links, bare URLs, blockquotes,
 *   horizontal rules and GFM pipe tables.
 *
 * Knowingly unsupported: setext headings, reference-style links, raw HTML blocks, footnotes,
 * loose-vs-tight list semantics, and nested blockquotes. Unsupported syntax degrades to its literal
 * text rather than being dropped.
 */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianMarkdown
{
public:
	static TArray<FExtendedAtlassianDocBlock> ToBlocks(const FString& Markdown);

	/** Converts one line of inline Markdown to escaped rich-text markup. Exposed for testing. */
	static FString InlineToMarkup(const FString& Line);
};
