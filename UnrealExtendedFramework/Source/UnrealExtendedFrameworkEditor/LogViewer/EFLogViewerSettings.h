// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "EFLogViewerSettings.generated.h"

/**
 * Editor Preferences > Plugins > Extended Log Viewer. Per user, per project: which tabs were open
 * and which were closed on purpose live here too, so the window comes back as it was left.
 */
UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "Extended Log Viewer"))
class UEFLogViewerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override { return TEXT("Editor"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** A log file that appears while the window is open gets a tab of its own, unless you closed that file's tab before. */
	UPROPERTY(config, EditAnywhere, Category = "Tabs")
	bool bAutoOpenNewFiles = true;

	/** Rows a tab keeps; the oldest go first. "Load earlier" raises the limit for that tab. */
	UPROPERTY(config, EditAnywhere, Category = "Tabs", meta = (ClampMin = "1000", UIMin = "1000"))
	int32 MaxRowsPerTab = 100000;

	/** How much of a long file (megabytes) a tab reads when it opens, and again per "Load earlier". */
	UPROPERTY(config, EditAnywhere, Category = "Tabs", meta = (ClampMin = "1", ClampMax = "512"))
	int32 LoadChunkMB = 32;

	/** Open tabs, in order, as paths relative to the log folder. Restored when the window opens. */
	UPROPERTY(config)
	TArray<FString> OpenTabs;

	/** Tabs you closed; their files are not opened automatically again until you reopen them from the Open log menu. */
	UPROPERTY(config)
	TArray<FString> ClosedTabs;

	/** Columns hidden with a tab's eye menu (or by right-clicking the header). Shared by every tab. */
	UPROPERTY(config)
	TArray<FName> HiddenColumns;

	/** Files the All tab leaves out (unticked in its eye menu), as paths relative to the log folder. */
	UPROPERTY(config)
	TArray<FString> AllTabHiddenFiles;

	/** Broadcast on the game thread after HiddenColumns changes, so every open tab shows the same columns. */
	static FSimpleMulticastDelegate& OnHiddenColumnsChanged();
};
