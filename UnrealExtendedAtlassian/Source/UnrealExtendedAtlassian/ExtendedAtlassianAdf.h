// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

/**
 * Minimal Atlassian Document Format support.
 *
 * Jira Cloud's v3 API takes and returns ADF JSON for descriptions and comments rather than plain
 * text or wiki markup. Full fidelity is a project of its own, so this deliberately supports a
 * subset: paragraphs, headings, lists, code blocks, links, tables and the common inline nodes.
 *
 * Reading is tolerant — unrecognised node types fall through to their child content rather than
 * being dropped, so an unsupported macro degrades to its text instead of vanishing.
 *
 * Writing produces only paragraphs, hard breaks and code blocks. Anything richer should be edited
 * in Jira itself.
 */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianAdf
{
public:
	/** Flattens an ADF node tree to plain text. Safe to call with a null or non-ADF node. */
	static FString ToPlainText(const TSharedPtr<FJsonObject>& DocumentNode);

	/** Builds an ADF doc from plain text. Blank lines separate paragraphs; single newlines become hard breaks. */
	static TSharedPtr<FJsonObject> MakeDoc(const FString& PlainText);

	/**
	 * Builds an ADF doc from plain text followed by a code block.
	 *
	 * Used by the bug reporter to keep captured editor context in a monospaced block, away from the
	 * human-written description.
	 */
	static TSharedPtr<FJsonObject> MakeDocWithCodeBlock(const FString& PlainText, const FString& CodeBlockContent);

	/** Serialises a node to a compact JSON string suitable for a request body. */
	static FString ToJsonString(const TSharedPtr<FJsonObject>& DocumentNode);
};
