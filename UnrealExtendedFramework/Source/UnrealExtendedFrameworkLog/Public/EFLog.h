// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EFLogTypes.h"

class UWorld;

/**
 * Extended Log
 *
 * EF_LOG(Category, Verbosity, Format, ...) writes one line into Saved/Logs/Extended/<Category>.log.
 *
 *     EF_LOG(Araf, Warning, TEXT("No exit door marker in %s"), *LevelName);
 *     EF_CLOG(!Door, Araf, Warning, TEXT("Door actor missing for %s"), *PlayerName);
 *     EF_LOG_DYNAMIC(CategoryNameVariable, Log, TEXT("Picked %s"), *ItemName);
 *
 * - The category is a bare name written at the call site. The first line that uses it creates the
 *   category and its file; there is nothing to declare. The name must be a C++ identifier; a name
 *   that is data rather than code goes through EF_LOG_DYNAMIC.
 * - Arguments are printf-style and checked at compile time, exactly as UE_LOG's are.
 * - Independent of UE_LOG: nothing goes to GLog, the Output Log or the engine's .log file.
 * - Any thread. Formatting happens on the caller only when the line is enabled; the file write happens
 *   on a background thread, so the caller never waits on the disk.
 * - Window > Log > Extended Log shows every file as a tab, live.
 *
 * Plan: Documents/ExtendedFrameworkLogSystemPlan.md (DOP repository).
 */

/** 0 compiles every EF_LOG out. Defaults to off in Shipping; a Target.cs can define it either way. */
#ifndef EF_LOG_ENABLED
	#define EF_LOG_ENABLED (!UE_BUILD_SHIPPING)
#endif

/**
 * The most verbose level still compiled in; anything chattier is removed at compile time.
 * Name an EEFLogVerbosity entry (Error, Warning, Display, Log, Verbose, VeryVerbose).
 */
#ifndef EF_LOG_COMPILED_MIN_VERBOSITY
	#define EF_LOG_COMPILED_MIN_VERBOSITY VeryVerbose
#endif

/** What a separator line marks. A run separator is also repeated into any file first opened during that run. */
enum class EEFLogSeparatorKind : uint8
{
	Plain,
	RunStarted,
	RunEnded
};

/** Full paths of the category files that just received data. Broadcast on the game thread. */
DECLARE_MULTICAST_DELEGATE_OneParam(FEFLogFilesFlushed, const TArray<FString>& /*FilePaths*/);

namespace EFLog
{
	namespace Private
	{
		/** Registers (or finds) a category. EF_LOG calls this once per call site; EF_LOG_DYNAMIC on every line. */
		UNREALEXTENDEDFRAMEWORKLOG_API FEFLogCategory& FindOrAddCategory(const TCHAR* Name);
		UNREALEXTENDEDFRAMEWORKLOG_API FEFLogCategory& FindOrAddCategory(FStringView Name);
		UNREALEXTENDEDFRAMEWORKLOG_API FEFLogCategory& FindOrAddCategory(const FString& Name);
		UNREALEXTENDEDFRAMEWORKLOG_API FEFLogCategory& FindOrAddCategory(FName Name);

		/** Stamps the line (time, frame, PIE instance) and hands it to the writer thread. */
		UNREALEXTENDEDFRAMEWORKLOG_API void Submit(FEFLogCategory& Category, EEFLogVerbosity Verbosity, const ANSICHAR* File, int32 Line, FString&& Message);

		/**
		 * Submit() for a caller that knows its world better than the global PIE id does (the Blueprint
		 * node), and has no __FILE__: the source is written as SourceText.
		 */
		UNREALEXTENDEDFRAMEWORKLOG_API void SubmitFromWorld(FEFLogCategory& Category, EEFLogVerbosity Verbosity, const UWorld* World, FString&& SourceText, FString&& Message);
	}

	/** Cached per EF_LOG call site, so the registry lookup happens once per site, not once per line. */
	class FCategoryHandle
	{
	public:
		explicit FCategoryHandle(const TCHAR* Name)
			: Category(&Private::FindOrAddCategory(Name))
		{
		}

		FORCEINLINE bool IsEnabled(EEFLogVerbosity Verbosity) const { return Category->IsEnabled(Verbosity); }
		FORCEINLINE FEFLogCategory& Get() const { return *Category; }

	private:
		FEFLogCategory* Category;
	};

	/**
	 * Turns any text into a category name that is safe as a file name: identifier characters only,
	 * never empty, never a reserved Windows device name ("Con" becomes "Con_"). EF_LOG names are
	 * already identifiers; this matters for names that arrive as data.
	 */
	UNREALEXTENDEDFRAMEWORKLOG_API FString SanitizeCategoryName(FStringView Name);

	/** Parses a level name ("warning", "VeryVerbose", ...), ignoring case. */
	UNREALEXTENDEDFRAMEWORKLOG_API bool ParseVerbosity(FStringView Text, EEFLogVerbosity& OutVerbosity);

	/**
	 * Sets a category's runtime threshold. It can be set up front: for a category no line has used
	 * yet, the threshold is remembered and applied when its first line creates it (so the file keeps
	 * the EF_LOG call site's spelling).
	 */
	UNREALEXTENDEDFRAMEWORKLOG_API void SetCategoryVerbosity(FName CategoryName, EEFLogVerbosity Verbosity);

	/** Sets every existing category's threshold, and the threshold new categories start with. */
	UNREALEXTENDEDFRAMEWORKLOG_API void SetAllCategoriesVerbosity(EEFLogVerbosity Verbosity);

	/** False when no such category exists yet. */
	UNREALEXTENDEDFRAMEWORKLOG_API bool GetCategoryVerbosity(FName CategoryName, EEFLogVerbosity& OutVerbosity);

	/** Every category created so far, by display name, sorted. */
	UNREALEXTENDEDFRAMEWORKLOG_API TArray<FString> GetCategoryNames();

	/**
	 * Writes "[time] ==== Text ====" into every open category file, in order with the lines around it.
	 * A RunStarted separator is also written into any file first opened before the matching RunEnded,
	 * so every file of a run carries its start marker. The editor writes these at PIE start and end.
	 */
	UNREALEXTENDEDFRAMEWORKLOG_API void WriteSeparator(const FString& Text, EEFLogSeparatorKind Kind = EEFLogSeparatorKind::Plain);

	/** Blocks until every line submitted before this call is written and flushed to disk. */
	UNREALEXTENDEDFRAMEWORKLOG_API void Flush();

	/** Saved/Logs/Extended/ (or the configured folder), whichever process owns it. Empty if file output is off. */
	UNREALEXTENDEDFRAMEWORKLOG_API FString GetRootDirectory();

	/** The folder this process writes into: the root, or its own subfolder when another process owns the root. Empty if file output is off. */
	UNREALEXTENDEDFRAMEWORKLOG_API FString GetLogDirectory();

	/** Where a category's file is (or will be, before its first line). Empty if the category does not exist. */
	UNREALEXTENDEDFRAMEWORKLOG_API FString GetCategoryFilePath(FName CategoryName);

	/**
	 * Called on the game thread with the files that received data, at most once per frame. Costs
	 * nothing while nobody listens. The Extended Log window uses it to update its own process's
	 * tabs without waiting for the operating system's file notifications.
	 */
	UNREALEXTENDEDFRAMEWORKLOG_API FDelegateHandle AddFilesFlushedListener(FEFLogFilesFlushed::FDelegate&& Listener);
	UNREALEXTENDEDFRAMEWORKLOG_API void RemoveFilesFlushedListener(FDelegateHandle Handle);
}

#if EF_LOG_ENABLED

	// FString::Printf is called here, at the call site, on purpose: in UE 5.8 it validates the format
	// through a consteval constructor, which needs the TEXT("...") literal itself and cannot see
	// through a forwarding function. That same call is what gives EF_LOG UE_LOG's compile-time checks.
	#define EF_PRIVATE_LOG(Condition, Category, Verbosity, Format, ...) \
		do \
		{ \
			if constexpr (::EEFLogVerbosity::Verbosity <= ::EEFLogVerbosity::EF_LOG_COMPILED_MIN_VERBOSITY) \
			{ \
				static const ::EFLog::FCategoryHandle EFLog_CategoryHandle(TEXT(#Category)); \
				if (EFLog_CategoryHandle.IsEnabled(::EEFLogVerbosity::Verbosity) && (Condition)) \
				{ \
					::EFLog::Private::Submit(EFLog_CategoryHandle.Get(), ::EEFLogVerbosity::Verbosity, __FILE__, __LINE__, FString::Printf(Format, ##__VA_ARGS__)); \
				} \
			} \
		} \
		while (false)

	// No per-site cache here: the name can change from one call to the next, so it is looked up
	// (sanitized, then found under a read lock) on every line.
	#define EF_LOG_DYNAMIC(CategoryName, Verbosity, Format, ...) \
		do \
		{ \
			if constexpr (::EEFLogVerbosity::Verbosity <= ::EEFLogVerbosity::EF_LOG_COMPILED_MIN_VERBOSITY) \
			{ \
				FEFLogCategory& EFLog_DynamicCategory = ::EFLog::Private::FindOrAddCategory(CategoryName); \
				if (EFLog_DynamicCategory.IsEnabled(::EEFLogVerbosity::Verbosity)) \
				{ \
					::EFLog::Private::Submit(EFLog_DynamicCategory, ::EEFLogVerbosity::Verbosity, __FILE__, __LINE__, FString::Printf(Format, ##__VA_ARGS__)); \
				} \
			} \
		} \
		while (false)

#else

	// Compiled out, but the arguments stay referenced so a variable used only in a log line does not
	// turn into an "unused variable" warning in the builds that strip logging.
	#define EF_PRIVATE_LOG(Condition, Category, Verbosity, Format, ...) \
		do \
		{ \
			if constexpr (false) \
			{ \
				(void)::EEFLogVerbosity::Verbosity; \
				(void)(Condition); \
				(void)FString::Printf(Format, ##__VA_ARGS__); \
			} \
		} \
		while (false)

	#define EF_LOG_DYNAMIC(CategoryName, Verbosity, Format, ...) \
		do \
		{ \
			if constexpr (false) \
			{ \
				(void)::EEFLogVerbosity::Verbosity; \
				(void)(CategoryName); \
				(void)FString::Printf(Format, ##__VA_ARGS__); \
			} \
		} \
		while (false)

#endif

/** Writes one line into Saved/Logs/Extended/<Category>.log. See the top of this file. */
#define EF_LOG(Category, Verbosity, Format, ...) EF_PRIVATE_LOG(true, Category, Verbosity, Format, ##__VA_ARGS__)

/** EF_LOG, but only when Condition is true. Condition is evaluated only when the category is enabled for that level. */
#define EF_CLOG(Condition, Category, Verbosity, Format, ...) EF_PRIVATE_LOG(Condition, Category, Verbosity, Format, ##__VA_ARGS__)
