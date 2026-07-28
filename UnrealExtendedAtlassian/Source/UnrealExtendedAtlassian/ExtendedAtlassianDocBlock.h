// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The document model every source converts into.
 *
 * This is the contract between parsers (Confluence HTML, Markdown, ADF) and the renderer.
 * Deliberately NOT Markdown: Confluence HTML is richer than Markdown, and routing it through
 * Markdown would lose detail twice and cap fidelity permanently at whatever Markdown can express.
 * Markdown is one input to this model, not the pivot.
 *
 * Contains no Slate and no Atlassian types, so it stays testable headlessly and the whole document
 * stack can be lifted into a shared plugin later without changes.
 */
enum class EExtendedAtlassianBlockKind : uint8
{
	Paragraph,
	Heading,
	BulletItem,
	OrderedItem,
	TaskItem,
	CodeBlock,
	Quote,
	TableRow,
	Rule,
	Image,
};

/**
 * One renderable block.
 *
 * Inline formatting lives in Markup as Slate rich-text markup, already escaped — block structure is
 * expressed by separate widgets, inline styling by rich text within a block. Hand-assembling styled
 * runs as sibling widgets cannot wrap correctly, which rules out the alternative.
 */
struct FExtendedAtlassianDocBlock
{
	EExtendedAtlassianBlockKind Kind = EExtendedAtlassianBlockKind::Paragraph;

	/** Inline content as escaped Slate rich-text markup. Empty for Rule and CodeBlock. */
	FString Markup;

	/** Heading level, 1-6. */
	int32 Level = 0;

	/** List nesting depth; 0 is top level. */
	int32 IndentDepth = 0;

	/** Number shown for an OrderedItem. */
	int32 OrderedIndex = 0;

	/** Checked state of a TaskItem. */
	bool bChecked = false;

	/** Verbatim text for a CodeBlock — never marked up, never escaped. */
	FString RawText;

	/** Language hint from a fenced code block, if any. */
	FString CodeLanguage;

	/** Cells of a TableRow, each as escaped markup. */
	TArray<FString> Cells;

	/** True for a table header row. */
	bool bIsHeaderRow = false;

	FString ImageAlt;
	FString ImageUrl;
};

/**
 * Builds Slate rich-text markup safely.
 *
 * Slate's markup parser treats '<' as a tag opener, so any text taken from a document must be
 * escaped before it is concatenated with tags. Getting this wrong turns a stray '<' in a page into
 * silently swallowed content, so it lives in one place with its own tests.
 */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianMarkup
{
public:
	/** Escapes text for inclusion in rich-text markup. */
	static FString Escape(const FString& PlainText);

	/** Wraps already-escaped content in a named text style. */
	static FString Styled(const FString& StyleName, const FString& EscapedInner);

	/** Wraps already-escaped content in a clickable hyperlink run. */
	static FString Link(const FString& Url, const FString& EscapedInner);
};
