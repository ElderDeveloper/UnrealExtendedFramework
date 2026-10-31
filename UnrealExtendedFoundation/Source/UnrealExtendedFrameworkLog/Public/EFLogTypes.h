// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <atomic>

#include "EFLogTypes.generated.h"

/**
 * How important an EF_LOG line is. Lower values are more severe, so "is this line enabled" is a
 * single compare against the category's threshold. There is no Fatal: EF_LOG never stops the process.
 */
UENUM(BlueprintType)
enum class EEFLogVerbosity : uint8
{
	Error,
	Warning,
	Display,
	Log,
	Verbose,
	VeryVerbose
};

/** The level name exactly as it is written into the log file ("Warning", "VeryVerbose", ...). */
UNREALEXTENDEDFRAMEWORKLOG_API const TCHAR* LexToString(EEFLogVerbosity Verbosity);

/**
 * One EF_LOG category. Created the first time any call site names it and never destroyed, so a
 * pointer to it stays valid for the life of the process (the EF_LOG macro caches one per call site).
 * The category owns only its identity and its runtime threshold; its file lives with the writer.
 */
class UNREALEXTENDEDFRAMEWORKLOG_API FEFLogCategory
{
public:
	FEFLogCategory(FName InName, const FString& InDisplayName, EEFLogVerbosity InVerbosity);

	FEFLogCategory(const FEFLogCategory&) = delete;
	FEFLogCategory& operator=(const FEFLogCategory&) = delete;

	/** Case-insensitive key: "Araf" and "araf" are the same category. */
	FName GetName() const { return Name; }

	/** The spelling seen first; it is also the file's base name. */
	const FString& GetDisplayName() const { return DisplayName; }

	EEFLogVerbosity GetVerbosity() const { return static_cast<EEFLogVerbosity>(Verbosity.load(std::memory_order_relaxed)); }
	void SetVerbosity(EEFLogVerbosity InVerbosity) { Verbosity.store(static_cast<uint8>(InVerbosity), std::memory_order_relaxed); }

	FORCEINLINE bool IsEnabled(EEFLogVerbosity InVerbosity) const
	{
		return static_cast<uint8>(InVerbosity) <= Verbosity.load(std::memory_order_relaxed);
	}

	/** Lines refused because the write queue was full; the writer reports and clears it. */
	void AddDroppedLine() { DroppedLines.fetch_add(1, std::memory_order_relaxed); }
	uint64 ConsumeDroppedLines() { return DroppedLines.exchange(0, std::memory_order_relaxed); }

private:
	const FName Name;
	const FString DisplayName;
	std::atomic<uint8> Verbosity;
	std::atomic<uint64> DroppedLines { 0 };
};
