// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianTypes.h"

struct FSlateDynamicImageBrush;

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

	/**
	 * Burns a copy of the normalized annotations into captured PNG bytes.
	 * The original capture is never modified.
	 */
	static bool BurnAnnotations(
		const TArray<uint8>& SourcePng,
		const FIntPoint& SourceSize,
		const TArray<FExtendedAtlassianAnnotation>& Annotations,
		TArray<uint8>& OutAnnotatedPng);

	/** Creates a transient Slate brush from PNG bytes for composer preview. */
	static TSharedPtr<FSlateDynamicImageBrush> CreatePreviewBrush(
		const TArray<uint8>& SourcePng,
		const FVector2D& PreviewSize);

	/** Version-independent normalized annotation payload for issue metadata/tests. */
	static FString SerializeAnnotations(
		const TArray<FExtendedAtlassianAnnotation>& Annotations);
	static bool DeserializeAnnotations(
		const FString& Json,
		TArray<FExtendedAtlassianAnnotation>& OutAnnotations,
		FString& OutError);
};
