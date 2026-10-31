// Copyright Moon Punch Games. All Rights Reserved.

#include "EFLogWriter.h"

#include "Async/Async.h"
#include "CoreGlobals.h"
#include "EFLogRegistry.h"
#include "HAL/Event.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTLS.h"
#include "HAL/PlatformTime.h"
#include "HAL/RunnableThread.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"

namespace EFLogWriterPrivate
{
	/** Lines queued but not yet written. Past this, new lines are dropped (and counted) rather than growing memory without bound. */
	static constexpr int32 MaxPendingEntries = 262144;

	/** How long Flush() waits before giving up on a stuck disk. */
	static constexpr uint32 FlushTimeoutMs = 10000;

	/** A crash handler must not hang the crash report, so it waits far less. */
	static constexpr uint32 CrashFlushTimeoutMs = 2000;

	/** After a roll fails (the file was open elsewhere), how much more to write before trying again. */
	static constexpr int64 RollRetryBytes = 8ll * 1024 * 1024;

	/** UEFLogSettings' ini section. Read directly at startup, which can run before UObjects exist. */
	static const TCHAR* SettingsSection = TEXT("/Script/UnrealExtendedFrameworkLog.EFLogSettings");
}

FEFLogWriter& FEFLogWriter::Get()
{
	// Leaked on purpose: lines can arrive while static destructors run, and joining a thread from a
	// static destructor can deadlock on the loader lock. Shutdown() is the orderly stop.
	static FEFLogWriter* Instance = new FEFLogWriter();
	return *Instance;
}

void FEFLogWriter::Startup()
{
	using namespace EFLogWriterPrivate;

	FScopeLock Lock(&LifecycleMutex);

	if (State.load() != EState::NotStarted)
	{
		return;
	}

	SessionStartSeconds = FPlatformTime::Seconds();
	const FDateTime SessionStartUtc = FDateTime::UtcNow();
	SessionStartLocal = FDateTime::Now();

	// Taken a moment apart, so rounded to the minute; real offsets are whole quarter hours at worst.
	const int32 OffsetMinutes = FMath::RoundToInt((SessionStartLocal - SessionStartUtc).GetTotalMinutes());
	UtcOffsetText = FString::Printf(TEXT("%c%02d:%02d"), OffsetMinutes < 0 ? TEXT('-') : TEXT('+'), FMath::Abs(OffsetMinutes) / 60, FMath::Abs(OffsetMinutes) % 60);

	ProjectDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeFilename(ProjectDirectory);
	if (!ProjectDirectory.EndsWith(TEXT("/")))
	{
		ProjectDirectory.AppendChar(TEXT('/'));
	}

	// The folder and the archive count decide where this session writes, so they are read here, from
	// the ini, rather than from the settings object: the first EF_LOG line can come from a module
	// that loads before UObjects exist.
	FString FolderName = TEXT("Extended");
	int32 ArchivesToKeep = 10;
	if (GConfig && GConfig->IsReadyForUse())
	{
		GConfig->GetString(SettingsSection, TEXT("FolderName"), FolderName, GEngineIni);
		GConfig->GetInt(SettingsSection, TEXT("ArchivesToKeep"), ArchivesToKeep, GEngineIni);
	}

	if (!Folder.Initialize(EFLog::SanitizeCategoryName(FolderName), ArchivesToKeep))
	{
		ReportProblem(FString::Printf(TEXT("could not create Saved/Logs/%s; EF_LOG output is off for this session."), *FolderName));
		State.store(EState::Disabled);
		return;
	}

	if (FPlatformProcess::SupportsMultithreading())
	{
		WakeEvent = FPlatformProcess::GetSynchEventFromPool(false);
		Thread = FRunnableThread::Create(this, TEXT("EFLogWriter"), 0, TPri_BelowNormal);
	}

	State.store(Thread ? EState::Running : EState::Synchronous);
}

void FEFLogWriter::Shutdown()
{
	FScopeLock Lock(&LifecycleMutex);

	const EState Previous = State.exchange(EState::Stopped);

	if (Previous == EState::Running)
	{
		bStopRequested.store(true);
		WakeEvent->Trigger();
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;

		// The thread is gone, so this is now the only consumer: write whatever was queued after
		// its last pass (including any Flush fences, which this releases).
		bool bUrgent = false;
		DrainQueue(bUrgent);
	}

	if (Previous == EState::Running || Previous == EState::Synchronous)
	{
		FScopeLock SynchronousLock(&SynchronousMutex);
		WriteDroppedLineNotices();
		FlushAll();
		OpenFiles.Empty();
		Folder.Release();
	}

	// WakeEvent is deliberately kept: a Flush() or Submit() that checked the state just before this
	// ran may still be about to trigger it, and this object is never destroyed anyway.
}

bool FEFLogWriter::EnsureStarted()
{
	EState Current = State.load();
	if (Current == EState::NotStarted)
	{
		Startup();
		Current = State.load();
	}
	return Current == EState::Running || Current == EState::Synchronous;
}

void FEFLogWriter::Submit(FEFLogEntry&& Entry)
{
	using namespace EFLogWriterPrivate;

	if (!EnsureStarted())
	{
		return;
	}

	if (State.load() == EState::Synchronous)
	{
		FScopeLock SynchronousLock(&SynchronousMutex);
		bool bUrgent = false;
		ProcessEntry(Entry, bUrgent);
		FlushAll();
		return;
	}

	// Separators are rare and mark structure the window relies on, so only lines can be dropped.
	const int32 PendingBefore = PendingEntries.fetch_add(1);
	if (Entry.Kind == EEFLogEntryKind::Line && PendingBefore >= MaxPendingEntries)
	{
		PendingEntries.fetch_sub(1);
		Entry.Category->AddDroppedLine();
		bAnyDropped.store(true);
		return;
	}

	Queue.Enqueue(MoveTemp(Entry));
	WakeIfSleeping();
}

void FEFLogWriter::WakeIfSleeping()
{
	// Only the producer that finds the writer asleep pays for the trigger, so a burst of lines
	// costs one wake-up, not one per line.
	if (bWriterSleeping.exchange(false))
	{
		WakeEvent->Trigger();
	}
}

void FEFLogWriter::Flush()
{
	FlushQueue(EFLogWriterPrivate::FlushTimeoutMs);
}

void FEFLogWriter::FlushForCrash()
{
	bCrashing.store(true);

	const EState Current = State.load();

	if (Current == EState::Synchronous)
	{
		// The crash may have happened on this very thread while it held the lock mid-write.
		if (SynchronousMutex.TryLock())
		{
			FlushAll();
			SynchronousMutex.Unlock();
		}
		return;
	}

	if (Current != EState::Running)
	{
		return;
	}

	if (Thread && FPlatformTLS::GetCurrentThreadId() == Thread->GetThreadID())
	{
		// The writer itself crashed; nobody else can drain its queue, so flush what it has.
		FlushAll();
		return;
	}

	FlushQueue(EFLogWriterPrivate::CrashFlushTimeoutMs);
}

void FEFLogWriter::FlushBeforeExit()
{
	// The writer thread may be the one exiting only in a crash, which FlushForCrash covers.
	if (Thread && FPlatformTLS::GetCurrentThreadId() == Thread->GetThreadID())
	{
		return;
	}

	FlushQueue(EFLogWriterPrivate::CrashFlushTimeoutMs);
}

void FEFLogWriter::FlushQueue(uint32 TimeoutMs)
{
	const EState Current = State.load();

	if (Current == EState::Synchronous)
	{
		FScopeLock SynchronousLock(&SynchronousMutex);
		FlushAll();
		return;
	}

	if (Current != EState::Running)
	{
		return;
	}

	// A fence rides the same queue as the lines, so every line queued before it (by any thread)
	// is written before the writer reaches it.
	FEvent* FenceEvent = FPlatformProcess::GetSynchEventFromPool(true);

	FEFLogEntry Fence;
	Fence.Kind = EEFLogEntryKind::Fence;
	Fence.FenceEvent = FenceEvent;
	Queue.Enqueue(MoveTemp(Fence));
	bWriterSleeping.store(false);
	WakeEvent->Trigger();

	if (FenceEvent->Wait(TimeoutMs))
	{
		FPlatformProcess::ReturnSynchEventToPool(FenceEvent);
	}
	else
	{
		// The writer still holds this event and will trigger it eventually. Returning it to the
		// pool now would let that late trigger wake an unrelated waiter, so it is leaked instead.
		ReportProblem(TEXT("Flush timed out; the disk may be stalled."));
	}
}

void FEFLogWriter::SetRuntimeOptions(float InFlushIntervalSeconds, int32 InMaxFileSizeMB)
{
	FlushIntervalSeconds.store(FMath::Clamp(InFlushIntervalSeconds, 0.01f, 5.0f));
	MaxFileSizeBytes.store(static_cast<int64>(FMath::Max(1, InMaxFileSizeMB)) * 1024 * 1024);
}

FString FEFLogWriter::GetRootDirectory()
{
	return EnsureStarted() ? Folder.GetRootDirectory() : FString();
}

FString FEFLogWriter::GetWriteDirectory()
{
	return EnsureStarted() ? Folder.GetWriteDirectory() : FString();
}

FString FEFLogWriter::GetCategoryFilePath(const FEFLogCategory& Category)
{
	return EnsureStarted() ? MakeFilePath(Category) : FString();
}

FString FEFLogWriter::MakeFilePath(const FEFLogCategory& Category) const
{
	return Folder.GetWriteDirectory() / (Category.GetDisplayName() + TEXT(".log"));
}

FDelegateHandle FEFLogWriter::AddFilesFlushedListener(FEFLogFilesFlushed::FDelegate&& Listener)
{
	check(IsInGameThread());
	FlushListenerCount.fetch_add(1);
	return FilesFlushed.Add(MoveTemp(Listener));
}

void FEFLogWriter::RemoveFilesFlushedListener(FDelegateHandle Handle)
{
	check(IsInGameThread());
	if (FilesFlushed.Remove(Handle))
	{
		FlushListenerCount.fetch_sub(1);
	}
}

uint32 FEFLogWriter::Run()
{
	while (true)
	{
		bool bUrgent = false;
		DrainQueue(bUrgent);

		const double FlushInterval = FlushIntervalSeconds.load();
		const bool bStopping = bStopRequested.load();
		if (bHasUnflushedData && (bUrgent || bStopping || FPlatformTime::Seconds() - UnflushedSinceSeconds >= FlushInterval))
		{
			FlushAll();
		}

		if (bStopping)
		{
			return 0;
		}

		// Announce the sleep before the last emptiness check. The exchange (not a plain store)
		// pairs with the producer's exchange in WakeIfSleeping, so a line enqueued before a
		// producer saw "awake" is guaranteed to be visible to the check below.
		bWriterSleeping.exchange(true);
		if (!Queue.IsEmpty() || bStopRequested.load())
		{
			bWriterSleeping.store(false);
			continue;
		}

		if (bHasUnflushedData)
		{
			// Sleep only until the pending data is due to be flushed.
			const double Remaining = FlushInterval - (FPlatformTime::Seconds() - UnflushedSinceSeconds);
			WakeEvent->Wait(static_cast<uint32>(FMath::Max(1, FMath::CeilToInt32(Remaining * 1000.0))));
		}
		else
		{
			WakeEvent->Wait();
		}

		bWriterSleeping.store(false);
	}
}

void FEFLogWriter::DrainQueue(bool& bOutUrgent)
{
	FEFLogEntry Entry;
	while (Queue.Dequeue(Entry))
	{
		if (Entry.Kind != EEFLogEntryKind::Fence)
		{
			PendingEntries.fetch_sub(1);
		}
		ProcessEntry(Entry, bOutUrgent);
	}

	if (bAnyDropped.exchange(false))
	{
		WriteDroppedLineNotices();
		bOutUrgent = true;
	}
}

void FEFLogWriter::ProcessEntry(const FEFLogEntry& Entry, bool& bOutUrgent)
{
	switch (Entry.Kind)
	{
	case EEFLogEntryKind::Line:
		WriteLine(Entry);
		if (Entry.Verbosity <= EEFLogVerbosity::Warning)
		{
			bOutUrgent = true;
		}
		break;

	case EEFLogEntryKind::Separator:
		WriteSeparator(Entry);
		bOutUrgent = true;
		break;

	case EEFLogEntryKind::Fence:
		FlushAll();
		if (Entry.FenceEvent)
		{
			Entry.FenceEvent->Trigger();
		}
		break;
	}
}

void FEFLogWriter::WriteLine(const FEFLogEntry& Entry)
{
	FOpenFile* File = GetOrOpenFile(*Entry.Category);
	if (!File)
	{
		return;
	}

	// [2026.09.24-14.03.11.482][f123456][Client 1][Warning][Source/Game/Araf.cpp:212] Message
	LineBuilder.Reset();
	AppendTimestamp(LineBuilder, Entry.Seconds);
	LineBuilder.Appendf(TEXT("[f%llu]"), Entry.Frame);

	if (!Entry.InstanceLabel.IsNone())
	{
		LineBuilder << TEXT('[') << Entry.InstanceLabel.ToString() << TEXT(']');
	}
	else if (Entry.PIEInstance >= 0)
	{
		LineBuilder.Appendf(TEXT("[PIE %d]"), Entry.PIEInstance);
	}
	else
	{
		LineBuilder << TEXT("[-]");
	}

	LineBuilder << TEXT('[') << LexToString(Entry.Verbosity) << TEXT(']');
	LineBuilder << TEXT('[') << (Entry.SourceText.IsEmpty() ? GetSourcePath(Entry.SourceFile) : Entry.SourceText);
	LineBuilder.Appendf(TEXT(":%d] "), Entry.SourceLine);

	// Continuation lines of a multi-line message are indented, so a reader can attach every line
	// that does not start with '[' to the entry above it.
	const FString& Message = Entry.Message;
	int32 End = Message.Len();
	while (End > 0 && (Message[End - 1] == TEXT('\n') || Message[End - 1] == TEXT('\r')))
	{
		--End;
	}

	for (int32 Index = 0; Index < End; ++Index)
	{
		const TCHAR Character = Message[Index];
		if (Character == TEXT('\r'))
		{
			continue;
		}
		if (Character == TEXT('\n'))
		{
			LineBuilder << LINE_TERMINATOR << TEXT("    ");
			continue;
		}
		LineBuilder.AppendChar(Character);
	}
	LineBuilder << LINE_TERMINATOR;

	WriteText(*File, LineBuilder.ToView());

	const int64 MaxBytes = MaxFileSizeBytes.load();
	if (MaxBytes > 0 && File->BytesWritten >= MaxBytes && File->BytesWritten >= File->NextRollAttemptBytes)
	{
		RollFile(*Entry.Category, *File);
	}
}

void FEFLogWriter::AppendTimestamp(FStringBuilderBase& Builder, double Seconds) const
{
	// Local time, derived from the monotonic clock the caller stamped, so a line never needs a
	// clock query on the calling thread.
	const FDateTime Time = SessionStartLocal + FTimespan::FromSeconds(Seconds - SessionStartSeconds);
	Builder.Appendf(TEXT("[%04d.%02d.%02d-%02d.%02d.%02d.%03d]"),
		Time.GetYear(), Time.GetMonth(), Time.GetDay(), Time.GetHour(), Time.GetMinute(), Time.GetSecond(), Time.GetMillisecond());
}

void FEFLogWriter::WriteSeparator(const FEFLogEntry& Entry)
{
	// [2026.09.24-14.03.11.482] ==== PIE 1 started | L_Castle | listen server ====
	// The time is what places the marker among other files' lines when the window merges them.
	TStringBuilder<256> Builder;
	AppendTimestamp(Builder, Entry.Seconds);
	Builder.Appendf(TEXT(" ==== %s ====%s"), *Entry.Message, LINE_TERMINATOR);
	const FString Line(Builder.ToView());

	for (TPair<FEFLogCategory*, FOpenFile>& Pair : OpenFiles)
	{
		if (Pair.Value.Archive)
		{
			WriteText(Pair.Value, Line);
		}
	}

	// Remembered so a file first opened later in the same run still carries the run's start marker,
	// which is what lets the window show "current PIE run only" for every tab.
	if (Entry.SeparatorKind == EEFLogSeparatorKind::RunStarted)
	{
		ActiveRunSeparatorLine = Line;
	}
	else if (Entry.SeparatorKind == EEFLogSeparatorKind::RunEnded)
	{
		ActiveRunSeparatorLine.Reset();
	}
}

void FEFLogWriter::WriteDroppedLineNotices()
{
	// Lines are dropped only when the queue is full; say so in the category that lost them, so a
	// gap in a file is never silent.
	FEFLogRegistry::Get().ForEach([this](FEFLogCategory& Category)
	{
		const uint64 Dropped = Category.ConsumeDroppedLines();
		if (Dropped == 0)
		{
			return;
		}

		FEFLogEntry Notice;
		Notice.Category = &Category;
		Notice.Verbosity = EEFLogVerbosity::Warning;
		Notice.Seconds = FPlatformTime::Seconds();
		Notice.Frame = GFrameCounter;
		Notice.SourceText = TEXT("EFLog");
		Notice.Message = FString::Printf(TEXT("%llu line(s) dropped: the EF_LOG queue was full."), Dropped);
		WriteLine(Notice);
	});
}

void FEFLogWriter::WriteText(FOpenFile& File, const FStringView Text)
{
	const FTCHARToUTF8 Utf8(Text.GetData(), Text.Len());
	File.Archive->Serialize((void*)Utf8.Get(), Utf8.Length());
	File.BytesWritten += Utf8.Length();
	File.bDirty = true;

	if (!bHasUnflushedData)
	{
		bHasUnflushedData = true;
		UnflushedSinceSeconds = FPlatformTime::Seconds();
	}
}

void FEFLogWriter::FlushAll()
{
	TArray<FString> FlushedPaths;
	for (TPair<FEFLogCategory*, FOpenFile>& Pair : OpenFiles)
	{
		FOpenFile& File = Pair.Value;
		if (File.Archive && File.bDirty)
		{
			File.Archive->Flush();
			File.bDirty = false;
			FlushedPaths.Add(File.Path);
		}
	}
	bHasUnflushedData = false;

	QueueFlushNotification(MoveTemp(FlushedPaths));
}

FEFLogWriter::FOpenFile* FEFLogWriter::GetOrOpenFile(FEFLogCategory& Category)
{
	FOpenFile& File = OpenFiles.FindOrAdd(&Category);
	if (File.Archive)
	{
		return &File;
	}
	if (File.bFailed)
	{
		return nullptr;
	}
	return OpenFile(Category, File, true) ? &File : nullptr;
}

bool FEFLogWriter::OpenFile(FEFLogCategory& Category, FOpenFile& File, bool bWriteHeader)
{
	File.Path = MakeFilePath(Category);

	IFileManager& FileManager = IFileManager::Get();
	const int64 ExistingSize = FileManager.FileSize(*File.Path);

	// Append, not truncate: the previous session's file was archived at startup, so a file that is
	// still here could not be moved, and appending keeps it instead of destroying it. AllowRead lets
	// the Extended Log window (and any text editor) read while this writes.
	File.Archive.Reset(FileManager.CreateFileWriter(*File.Path, FILEWRITE_Append | FILEWRITE_AllowRead | FILEWRITE_Silent));
	if (!File.Archive)
	{
		File.bFailed = true;
		ReportProblem(FString::Printf(TEXT("could not open %s; category %s writes nothing this session."), *File.Path, *Category.GetDisplayName()));
		return false;
	}

	File.BytesWritten = FMath::Max<int64>(0, ExistingSize);

	if (bWriteHeader)
	{
		// # EFLog v1 | Category=Araf | Session=2026.09.24-14.03.11 | UtcOffset=+03:00 | Process=UnrealEditor pid 12345 | Project=DevilOfThePlague
		const FString Header = FString::Printf(TEXT("%s | Category=%s | Session=%s | UtcOffset=%s | Process=%s pid %u | Project=%s%s"),
			FEFLogSessionFolder::GetHeaderPrefix(),
			*Category.GetDisplayName(),
			*Folder.GetSessionStamp(),
			*UtcOffsetText,
			*Folder.GetProcessName(),
			FPlatformProcess::GetCurrentProcessId(),
			FApp::GetProjectName(),
			LINE_TERMINATOR);
		WriteText(File, Header);

		if (!ActiveRunSeparatorLine.IsEmpty())
		{
			WriteText(File, ActiveRunSeparatorLine);
		}
	}

	return true;
}

void FEFLogWriter::RollFile(FEFLogCategory& Category, FOpenFile& File)
{
	using namespace EFLogWriterPrivate;

	File.Archive->Flush();
	File.Archive->Close();
	File.Archive.Reset();
	File.bDirty = false;

	// The full file moves to this session's archive folder and a fresh one starts in its place. The
	// window sees its file shrink and simply carries on reading the new one.
	const FString Destination = Folder.MakeRollDestination(Category.GetDisplayName());
	const bool bMoved = IFileManager::Get().Move(*Destination, *File.Path, false, false, false, true);
	if (bMoved)
	{
		File.NextRollAttemptBytes = 0;
	}
	else
	{
		// Something outside this process has the file open. Keep appending and try again later,
		// rather than on every line.
		ReportProblem(FString::Printf(TEXT("could not move %s aside at its size cap; it keeps growing for now."), *File.Path));
		File.NextRollAttemptBytes = File.BytesWritten + RollRetryBytes;
	}

	// A moved file gets a fresh header (and the active run marker); one that stayed put continues.
	const int64 RetryAt = File.NextRollAttemptBytes;
	if (OpenFile(Category, File, bMoved))
	{
		File.NextRollAttemptBytes = RetryAt;
	}
}

const FString& FEFLogWriter::GetSourcePath(const ANSICHAR* SourceFile)
{
	// __FILE__ literals have stable addresses, so the pointer is a good cache key.
	if (const FString* Cached = SourcePaths.Find(SourceFile))
	{
		return *Cached;
	}

	FString Path = SourceFile ? FString(UTF8_TO_TCHAR(SourceFile)) : FString(TEXT("EFLog"));
	FPaths::NormalizeFilename(Path);

	// Relative to the project folder when it lives there; anything else keeps its full path so a
	// jump-to-source can still find it.
	if (Path.StartsWith(ProjectDirectory, ESearchCase::IgnoreCase))
	{
		Path.RightChopInline(ProjectDirectory.Len());
	}

	return SourcePaths.Add(SourceFile, MoveTemp(Path));
}

void FEFLogWriter::QueueFlushNotification(TArray<FString>&& Paths)
{
	// Costs nothing unless the Extended Log window (or another listener) is open. Never during a
	// crash or shutdown: the task graph is no place to be then.
	if (Paths.Num() == 0 || FlushListenerCount.load() == 0 || bCrashing.load() || State.load() == EState::Stopped)
	{
		return;
	}

	{
		FScopeLock Lock(&NotifyMutex);
		for (FString& Path : Paths)
		{
			PendingNotifyPaths.Add(MoveTemp(Path));
		}
	}

	// One game-thread task per burst: later flushes join the pending set until it is delivered.
	if (!bNotifyScheduled.exchange(true))
	{
		AsyncTask(ENamedThreads::GameThread, []()
		{
			FEFLogWriter::Get().DeliverFlushNotifications();
		});
	}
}

void FEFLogWriter::DeliverFlushNotifications()
{
	bNotifyScheduled.store(false);

	TArray<FString> Paths;
	{
		FScopeLock Lock(&NotifyMutex);
		Paths = PendingNotifyPaths.Array();
		PendingNotifyPaths.Reset();
	}

	if (Paths.Num() > 0)
	{
		FilesFlushed.Broadcast(Paths);
	}
}

void FEFLogWriter::ReportProblem(const FString& Problem)
{
	// Never through UE_LOG: this system is independent of GLog by design.
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("EFLog: %s\n"), *Problem);
}
