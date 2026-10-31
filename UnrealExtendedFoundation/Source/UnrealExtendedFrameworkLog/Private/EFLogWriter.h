// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "EFLog.h"
#include "EFLogSessionFolder.h"
#include "EFLogTypes.h"
#include "HAL/CriticalSection.h"
#include "HAL/Runnable.h"
#include "Misc/StringBuilder.h"
#include <atomic>

class FEvent;
class FRunnableThread;

enum class EEFLogEntryKind : uint8
{
	/** One EF_LOG line, for one category's file. */
	Line,
	/** "==== text ====" into every open file. */
	Separator,
	/** Flush request: everything queued before it is written and flushed, then FenceEvent fires. */
	Fence
};

/** One queued item, stamped on the calling thread. */
struct FEFLogEntry
{
	/** Line: the message. Separator: the separator text. */
	FString Message;

	/** Replaces SourceFile:SourceLine when set (Blueprint callers have no __FILE__). */
	FString SourceText;

	FEFLogCategory* Category = nullptr;
	FEvent* FenceEvent = nullptr;
	const ANSICHAR* SourceFile = nullptr;
	double Seconds = 0.0;
	uint64 Frame = 0;

	/** "Server", "Client 1", ... resolved on the game thread; None prints the raw PIE id or "-". */
	FName InstanceLabel;

	int32 SourceLine = 0;
	int32 PIEInstance = INDEX_NONE;
	EEFLogVerbosity Verbosity = EEFLogVerbosity::Log;
	EEFLogEntryKind Kind = EEFLogEntryKind::Line;
	EEFLogSeparatorKind SeparatorKind = EEFLogSeparatorKind::Plain;
};

/**
 * Owns every category file and the one background thread that writes them.
 *
 * Callers never touch the disk: Submit() pushes onto a lock-free queue and wakes the thread only
 * when it is asleep. The thread sleeps on an event while there is nothing to write; while written
 * data is waiting to be flushed it sleeps only until that data's flush is due, and a Warning or Error
 * flushes immediately. No polling while idle.
 *
 * Leaked singleton, for the same reason as the registry: lines may be submitted during shutdown.
 * Lines submitted after Shutdown() are dropped.
 */
class FEFLogWriter final : public FRunnable
{
public:
	static FEFLogWriter& Get();

	/** Takes the session folder and starts the thread. Idempotent; the first Submit calls it too. */
	void Startup();

	/** Writes everything still queued, closes the files and releases the folder. */
	void Shutdown();

	/** Queues a Line or Separator entry. */
	void Submit(FEFLogEntry&& Entry);

	/** Blocks until everything submitted before this call is written and flushed. */
	void Flush();

	/** Called from FCoreDelegates::OnHandleSystemError: gets what it can onto disk, bounded in time. */
	void FlushForCrash();

	/**
	 * Called from ApplicationWillTerminate, which a forced exit (RequestExit(true): "quit force",
	 * -TestExit) broadcasts right before it terminates the process without shutting modules down.
	 * Flushes, bounded in time, and leaves the writer running for a normal exit's remaining lines.
	 */
	void FlushBeforeExit();

	/** Runtime-tunable options (UEFLogSettings). The folder and archive count only apply at startup. */
	void SetRuntimeOptions(float InFlushIntervalSeconds, int32 InMaxFileSizeMB);

	/** Empty when file output is off (no folder could be created, or already shut down). */
	FString GetRootDirectory();
	FString GetWriteDirectory();
	FString GetCategoryFilePath(const FEFLogCategory& Category);

	/** Game thread only. */
	FDelegateHandle AddFilesFlushedListener(FEFLogFilesFlushed::FDelegate&& Listener);
	void RemoveFilesFlushedListener(FDelegateHandle Handle);

	//~ Begin FRunnable
	virtual uint32 Run() override;
	//~ End FRunnable

private:
	enum class EState : uint8
	{
		NotStarted,
		Running,
		/** The platform has no threads (-nothreading): entries are written on the caller, under a lock. */
		Synchronous,
		Stopped,
		/** No folder could be created; every line is dropped. */
		Disabled
	};

	struct FOpenFile
	{
		TUniquePtr<FArchive> Archive;
		FString Path;
		int64 BytesWritten = 0;

		/** After a failed roll (the file was held open elsewhere), the next attempt waits for this size. */
		int64 NextRollAttemptBytes = 0;

		bool bFailed = false;

		/** Written since the last flush; the flush notification reports only these files. */
		bool bDirty = false;
	};

	FEFLogWriter() = default;

	/** True while there is a folder to write into. */
	bool EnsureStarted();

	void WakeIfSleeping();

	/** Queues a fence and waits for it, up to TimeoutMs. */
	void FlushQueue(uint32 TimeoutMs);

	/** Writer-side only (the writer thread, or the caller in Synchronous mode / after the thread stopped). */
	void DrainQueue(bool& bOutUrgent);
	void ProcessEntry(const FEFLogEntry& Entry, bool& bOutUrgent);
	void WriteLine(const FEFLogEntry& Entry);
	void WriteSeparator(const FEFLogEntry& Entry);

	/** "[2026.09.24-14.03.11.482]": local time, from the monotonic clock an entry was stamped with. */
	void AppendTimestamp(FStringBuilderBase& Builder, double Seconds) const;
	void WriteDroppedLineNotices();
	void WriteText(FOpenFile& File, const FStringView Text);
	void FlushAll();
	FOpenFile* GetOrOpenFile(FEFLogCategory& Category);
	bool OpenFile(FEFLogCategory& Category, FOpenFile& File, bool bWriteHeader);
	void RollFile(FEFLogCategory& Category, FOpenFile& File);
	FString MakeFilePath(const FEFLogCategory& Category) const;
	const FString& GetSourcePath(const ANSICHAR* SourceFile);

	/** Game-thread delivery of FilesFlushed, coalesced so a burst of flushes costs one task. */
	void QueueFlushNotification(TArray<FString>&& Paths);
	void DeliverFlushNotifications();

	static void ReportProblem(const FString& Problem);

	std::atomic<EState> State { EState::NotStarted };
	FCriticalSection LifecycleMutex;
	FCriticalSection SynchronousMutex;

	TQueue<FEFLogEntry, EQueueMode::Mpsc> Queue;
	std::atomic<int32> PendingEntries { 0 };
	std::atomic<bool> bAnyDropped { false };
	std::atomic<bool> bWriterSleeping { false };
	std::atomic<bool> bStopRequested { false };
	std::atomic<bool> bCrashing { false };
	FEvent* WakeEvent = nullptr;
	FRunnableThread* Thread = nullptr;

	std::atomic<float> FlushIntervalSeconds { 0.1f };
	std::atomic<int64> MaxFileSizeBytes { 256ll * 1024 * 1024 };

	FEFLogSessionFolder Folder;
	FString ProjectDirectory;
	FDateTime SessionStartLocal;
	double SessionStartSeconds = 0.0;

	/** "+03:00": what the local times in this session's files are ahead of UTC, for merging logs from several machines. */
	FString UtcOffsetText;

	// Writer-side state.
	TMap<FEFLogCategory*, FOpenFile> OpenFiles;
	TMap<const ANSICHAR*, FString> SourcePaths;
	TStringBuilder<1024> LineBuilder;
	FString ActiveRunSeparatorLine;
	bool bHasUnflushedData = false;
	double UnflushedSinceSeconds = 0.0;

	// Flush notifications.
	FEFLogFilesFlushed FilesFlushed;
	std::atomic<int32> FlushListenerCount { 0 };
	std::atomic<bool> bNotifyScheduled { false };
	FCriticalSection NotifyMutex;
	TSet<FString> PendingNotifyPaths;
};
