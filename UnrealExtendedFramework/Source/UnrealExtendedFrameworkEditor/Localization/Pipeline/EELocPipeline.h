// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

class FEELocalizationSession;
class FEELocReviewStore;
class SWindow;
class ULocalizationTarget;

/**
 * LW-7 pipeline orchestration: invokes the normal Unreal localization commandlets
 * (gather/import/export/compile) through the engine's LocalizationCommandletTasks — the same
 * path the Localization Dashboard uses — plus live LocRes refresh and translator
 * context-package export. The workbench never bypasses the standard pipeline.
 */
namespace EELocPipeline
{
	UNREALEXTENDEDFRAMEWORKEDITOR_API ULocalizationTarget* FindGameTarget(const FString& TargetName);

	UNREALEXTENDEDFRAMEWORKEDITOR_API bool Gather(const FString& TargetName, const TSharedRef<SWindow>& ParentWindow);
	UNREALEXTENDEDFRAMEWORKEDITOR_API bool Import(const FString& TargetName, const TSharedRef<SWindow>& ParentWindow);
	UNREALEXTENDEDFRAMEWORKEDITOR_API bool Export(const FString& TargetName, const TSharedRef<SWindow>& ParentWindow);
	UNREALEXTENDEDFRAMEWORKEDITOR_API bool Compile(const FString& TargetName, const TSharedRef<SWindow>& ParentWindow);

	/** Hot-reloads compiled localization resources into the running editor. */
	UNREALEXTENDEDFRAMEWORKEDITOR_API void RefreshLiveResources();

	/**
	 * Exports a translator context package (CSV): identity, source, translation, states,
	 * review info, and aggregated provider context per entry for one culture.
	 */
	UNREALEXTENDEDFRAMEWORKEDITOR_API bool ExportContextPackage(const FEELocalizationSession& Session,
		const FEELocReviewStore& ReviewStore, const FString& Culture, FString& OutFilePath, FString& OutError);
}

#endif // WITH_EDITOR
