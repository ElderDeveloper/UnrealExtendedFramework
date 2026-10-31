// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EFLogTypes.h"
#include "Engine/DeveloperSettings.h"

#include "EFLogSettings.generated.h"

/**
 * Project Settings > Plugins > Extended Log.
 *
 * Thresholds and flushing apply at once; the folder and the archive count decide where a session
 * writes, so they take effect at the next launch.
 */
UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "Extended Log"))
class UNREALEXTENDEDFRAMEWORKLOG_API UEFLogSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** Folder under Saved/Logs that EF_LOG writes into. Letters, digits and underscores only. */
	UPROPERTY(config, EditAnywhere, Category = "Files", meta = (ConfigRestartRequired = true))
	FString FolderName = TEXT("Extended");

	/** Earlier sessions kept under <Folder>/Archive; older ones are deleted when a session starts. */
	UPROPERTY(config, EditAnywhere, Category = "Files", meta = (ClampMin = "1", UIMin = "1", ConfigRestartRequired = true))
	int32 ArchivesToKeep = 10;

	/** Longest time a written line waits before it is flushed to disk (seconds). Warnings and errors flush at once. The Extended Log window reads the files, so this is also its latency. */
	UPROPERTY(config, EditAnywhere, Category = "Files", meta = (ClampMin = "0.01", ClampMax = "5.0"))
	float FlushIntervalSeconds = 0.1f;

	/** A category file larger than this (megabytes) moves into this session's archive folder and a fresh file continues. */
	UPROPERTY(config, EditAnywhere, Category = "Files", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxFileSizeMB = 256;

	/** Threshold a category starts with. Verbose and VeryVerbose lines are dropped until a category is raised. */
	UPROPERTY(config, EditAnywhere, Category = "Verbosity")
	EEFLogVerbosity DefaultVerbosity = EEFLogVerbosity::Log;

	/** Per-category thresholds, by the name used in EF_LOG (case does not matter). */
	UPROPERTY(config, EditAnywhere, Category = "Verbosity")
	TMap<FName, EEFLogVerbosity> CategoryVerbosity;

	/** Pushes thresholds and flushing into the running system. */
	void ApplyToRuntime() const;

protected:
	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
