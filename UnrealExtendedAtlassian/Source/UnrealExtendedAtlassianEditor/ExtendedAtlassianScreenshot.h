// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Captures the editor or PIE viewport straight to PNG bytes, without touching the filesystem. */
class FExtendedAtlassianScreenshot
{
public:
	/**
	 * Reads the active viewport and compresses it to PNG.
	 *
	 * Prefers the PIE viewport when one exists — a bug raised during play should show the game, not
	 * the editor around it. Returns false when no viewport is available or the read fails.
	 */
	static bool CaptureActiveViewport(TArray<uint8>& OutPngData, FIntPoint& OutSize);
};
