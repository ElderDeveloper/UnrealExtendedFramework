// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Editor state captured at the moment a bug was raised. */
struct FExtendedAtlassianCapturedContext
{
	FString LevelName;

	/** "Editor", "Play In Editor" or "Simulate In Editor". */
	FString WorldContext;

	bool bHasCamera = false;
	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;

	TArray<FString> SelectedActors;

	/** Selected actors beyond the cap, reported as a count rather than silently dropped. */
	int32 SelectedActorOverflow = 0;

	FString EngineVersion;
	FString Platform;
	FString GraphicsRhi;
	FString Gpu;
	FString GitBranch;
	FString GitCommit;

	/** Renders the context as a plain-text block for the issue description. */
	FString ToContextBlock() const;
};

/**
 * Gathers the surrounding editor state for a bug report.
 *
 * This is the part of the plugin that no external integration can replace: the level, camera,
 * selection, build and log tail at the instant something broke.
 */
class FExtendedAtlassianContextCapture
{
public:
	static FExtendedAtlassianCapturedContext Capture();

	/** Tail of the current editor log. Returns empty when the log cannot be read. */
	static FString ReadLogTail(int32 Kilobytes);

	/**
	 * Reads the project repository's branch and short SHA straight from .git.
	 *
	 * Parsing the files avoids spawning a process and works when git is not on PATH. Handles a .git
	 * file (worktrees and submodules) and refs that have been packed.
	 */
	static bool ReadGitRevision(FString& OutBranch, FString& OutShortSha);
};
