// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianTypes.h"

/** A Markdown working copy on disk, plus the Confluence identity recorded in its front matter. */
struct FExtendedAtlassianDocumentFile
{
	FString PageId;
	FString SpaceId;
	FString SpaceKey;
	FString Title;

	/** The Confluence version this copy was pulled from; a save is based on it. */
	int32 Version = 0;

	/** Body only, with the front matter removed. */
	FString Markdown;

	FString FilePath;

	bool IsLinkedToConfluence() const { return !PageId.IsEmpty(); }
};

/**
 * Markdown working copies under Saved/Documents.
 *
 * Files are plain Markdown so anything — a person, an LLM, another tool — can read and edit them
 * without knowing about this plugin. A YAML front matter block records which Confluence page the
 * file came from and at what version, which makes the file self-describing and lets the mapping
 * survive page renames and editor restarts.
 *
 * The file is a *working copy*, never an authority. Pull overwrites it; Push is always an explicit
 * action. Auto-pushing on file change would let an unattended editor silently overwrite Confluence.
 */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianDocumentStore
{
public:
	/** ProjectDir/Saved/Documents. */
	static FString GetRootDirectory();

	/** Absolute path a page maps to. Stable across title changes because the id is in the name. */
	static FString MakeFilePath(const FString& SpaceKey, const FString& Title, const FString& PageId);

	/** Writes a pulled page as Markdown with front matter. */
	static bool Save(const FExtendedAtlassianPage& Page, const FString& SpaceKey, FString& OutPath);

	/** Reads a working copy, splitting front matter from body. */
	static bool Load(const FString& Path, FExtendedAtlassianDocumentFile& OutFile);

	/** Splits a file's front matter from its body. Exposed for testing. */
	static bool ParseFrontMatter(const FString& Contents, FExtendedAtlassianDocumentFile& OutFile);

	/** Renders a front matter block, including the trailing delimiter and blank line. */
	static FString MakeFrontMatter(const FExtendedAtlassianDocumentFile& File);
};
