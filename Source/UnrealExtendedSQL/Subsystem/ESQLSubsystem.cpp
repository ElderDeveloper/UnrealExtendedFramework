// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESQLSubsystem.h"

#include "Core/ESQLDatabase.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTLS.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Private/Paths/ESQLPathResolver.h"
#include "Private/USqlite/ESQLUsqliteBuilder.h"
#include "Private/USqlite/ESQLUsqliteSerializer.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/ESQLSettings.h"
#include "TableAsset/ESQLTableAsset.h"
#include "UnrealExtendedSQL.h"
#include "UObject/UnrealType.h"

#include "Async/Async.h"
#include "Algo/Sort.h"

namespace
{
constexpr uint32 WriterWakeWaitMs = 25;

FESQLError MakeSubsystemError(EESQLErrorCode Code, FString Message, FString SqlFragment = FString())
{
	return FESQLError::Make(Code, MoveTemp(Message), MoveTemp(SqlFragment));
}

FESQLQueryResult MakeSubsystemFailure(EESQLErrorCode Code, FString Message, FString SqlFragment = FString())
{
	return FESQLQueryResult::Failure(MakeSubsystemError(Code, MoveTemp(Message), MoveTemp(SqlFragment)));
}

FESQLQueryResult MakeSubsystemFailure(FESQLError Error)
{
	return FESQLQueryResult::Failure(MoveTemp(Error));
}

FESQLQueryResult MakeSubsystemSuccessFromErrorResult(const FESQLErrorResult& Result)
{
	return Result.bSuccess ? FESQLQueryResult::Success() : FESQLQueryResult::Failure(Result.Error);
}

FESQLQueryResult ExecuteDatabaseSql(FESQLDatabase& Database, const FString& SQL, const TArray<FESQLBindingValue>& Bindings)
{
	return Bindings.Num() > 0 ? Database.Execute(SQL, Bindings) : Database.Execute(SQL);
}

TArray<FESQLBindingValue> MakeBindingValues(const TArray<FString>& Bindings)
{
	TArray<FESQLBindingValue> Values;
	Values.Reserve(Bindings.Num());
	for (const FString& Binding : Bindings)
	{
		Values.Add(FESQLBindingValue::FromText(Binding));
	}
	return Values;
}

TArray<FString> ParseMigrationStatements(const FString& SqlContent)
{
	TArray<FString> Statements;
	FString CurrentStatement;

	TArray<FString> Lines;
	SqlContent.ParseIntoArrayLines(Lines);

	for (const FString& Line : Lines)
	{
		const FString TrimmedLine = Line.TrimStartAndEnd();
		if (TrimmedLine.IsEmpty() || TrimmedLine.StartsWith(TEXT("--")))
		{
			continue;
		}

		CurrentStatement += Line + TEXT("\n");
		if (TrimmedLine.EndsWith(TEXT(";")))
		{
			FString FinalStatement = CurrentStatement.TrimStartAndEnd();
			if (FinalStatement.EndsWith(TEXT(";")))
			{
				FinalStatement.LeftChopInline(1);
				FinalStatement.TrimEndInline();
			}

			if (!FinalStatement.IsEmpty())
			{
				Statements.Add(MoveTemp(FinalStatement));
			}

			CurrentStatement.Empty();
		}
	}

	if (!CurrentStatement.TrimStartAndEnd().IsEmpty())
	{
		FString FinalStatement = CurrentStatement.TrimStartAndEnd();
		if (FinalStatement.EndsWith(TEXT(";")))
		{
			FinalStatement.LeftChopInline(1);
			FinalStatement.TrimEndInline();
		}

		if (!FinalStatement.IsEmpty())
		{
			Statements.Add(MoveTemp(FinalStatement));
		}
	}

	return Statements;
}

FESQLQuerySpec BuildSingleFilterQuerySpec(const FName FieldName, const EESQLFilterOperation Operation, const FESQLBindingValue& Value)
{
	FESQLQuerySpec QuerySpec;
	QuerySpec.Filters.Add(FESQLFieldFilter(FieldName.ToString(), Operation, Value));
	return QuerySpec;
}

bool BuildPagedQuerySpec(const FESQLQuerySpec& BaseQuerySpec, int32 PageIndex, int32 PageSize, FESQLQuerySpec& OutQuerySpec, FString& OutError)
{
	if (PageIndex < 0)
	{
		OutError = TEXT("PageIndex cannot be negative");
		return false;
	}

	if (PageSize <= 0)
	{
		OutError = TEXT("PageSize must be greater than zero");
		return false;
	}

	const int64 RequestedOffset = static_cast<int64>(PageIndex) * static_cast<int64>(PageSize);
	if (RequestedOffset > MAX_int32)
	{
		OutError = TEXT("Requested page offset exceeds supported range");
		return false;
	}

	OutQuerySpec = BaseQuerySpec;
	OutQuerySpec.Limit = PageSize;
	OutQuerySpec.Offset = static_cast<int32>(RequestedOffset);
	OutError.Reset();
	return true;
}

FString MakePreparedTableKey(const UESQLTableAsset* TableAsset)
{
	const FESQLTableSchema Schema = TableAsset ? TableAsset->GetSchemaDescriptor() : FESQLTableSchema();
	return FString::Printf(
		TEXT("%s|%s|%s"),
		*GetNameSafe(TableAsset),
		*Schema.DatabaseName,
		Schema.RowStruct ? *Schema.RowStruct->GetPathName() : TEXT("<null>"));
}
}

struct FESQLTransactionLifetime
{
};

struct FESQLAsyncTicketState : public TSharedFromThis<FESQLAsyncTicketState, ESPMode::ThreadSafe>
{
	FESQLTicket Ticket;
	TAtomic<bool> bCancelled = false;
};

struct FESQLWriterWorkResult
{
	FESQLQueryResult Result = FESQLQueryResult::Success();
	FESQLTransactionHandle TransactionHandle;
	bool bHasTransactionHandle = false;
};

struct FESQLBlockingResponse : public TSharedFromThis<FESQLBlockingResponse, ESPMode::ThreadSafe>
{
	FESQLBlockingResponse()
		: Event(FPlatformProcess::GetSynchEventFromPool(true))
	{
	}

	~FESQLBlockingResponse()
	{
		if (Event)
		{
			FPlatformProcess::ReturnSynchEventToPool(Event);
			Event = nullptr;
		}
	}

	FEvent* Event = nullptr;
	FESQLQueryResult Result = FESQLQueryResult::Success();
	FESQLTransactionHandle TransactionHandle;
	bool bHasTransactionHandle = false;
};

struct FESQLWriterWorkItem : public TSharedFromThis<FESQLWriterWorkItem, ESPMode::ThreadSafe>
{
	TSharedPtr<FESQLAsyncTicketState, ESPMode::ThreadSafe> TicketState;
	TSharedPtr<FESQLBlockingResponse, ESPMode::ThreadSafe> BlockingResponse;
	TUniqueFunction<FESQLWriterWorkResult(FESQLDatabaseContext&)> Run;
	TUniqueFunction<void(const FESQLWriterWorkResult&)> Completion;
};

struct FESQLActiveTransactionState
{
	int64 Id = 0;
	FString SavepointName;
	TWeakPtr<FESQLTransactionLifetime, ESPMode::ThreadSafe> Lifetime;
};

struct FESQLReaderLease;

class FESQLDatabaseContextWriterRunnable final : public FRunnable
{
public:
	explicit FESQLDatabaseContextWriterRunnable(TSharedRef<FESQLDatabaseContext, ESPMode::ThreadSafe> InContext)
		: Context(MoveTemp(InContext))
	{
	}

	virtual uint32 Run() override;

private:
	void ExecuteWorkItem(const TSharedPtr<FESQLWriterWorkItem, ESPMode::ThreadSafe>& WorkItem) const;

	TSharedRef<FESQLDatabaseContext, ESPMode::ThreadSafe> Context;
};

struct FESQLDatabaseContext : public TSharedFromThis<FESQLDatabaseContext, ESPMode::ThreadSafe>
{
	~FESQLDatabaseContext()
	{
		if (WriterWake)
		{
			FPlatformProcess::ReturnSynchEventToPool(WriterWake);
			WriterWake = nullptr;
		}

		if (ReaderReturnedEvent)
		{
			FPlatformProcess::ReturnSynchEventToPool(ReaderReturnedEvent);
			ReaderReturnedEvent = nullptr;
		}
	}

	bool Initialize(FESQLError& OutError)
	{
		if (Persistence == EESQLDatabasePersistence::Session && bReadOnly)
		{
			OutError = MakeSubsystemError(EESQLErrorCode::InvalidArgument, TEXT("Readonly databases cannot use session persistence"));
			return false;
		}

		WriterWake = FPlatformProcess::GetSynchEventFromPool(false);
		ReaderReturnedEvent = FPlatformProcess::GetSynchEventFromPool(false);

		const FESQLDatabaseOpenResult OpenResult = Persistence == EESQLDatabasePersistence::Session
			? FESQLDatabase::OpenInMemory()
			: (bReadOnly ? FESQLDatabase::OpenReadOnly(ResolvedPath) : FESQLDatabase::Open(ResolvedPath));

		if (!OpenResult)
		{
			OutError = OpenResult.GetError();
			return false;
		}

		WriterDatabase = OpenResult.GetValue();
		MaxReaderPoolSize = (Persistence == EESQLDatabasePersistence::Session || ResolvedPath.IsEmpty())
			? 0
			: FMath::Clamp(FPlatformMisc::NumberOfCoresIncludingHyperthreads() / 2, 2, 4);

		WriterRunnable = MakeUnique<FESQLDatabaseContextWriterRunnable>(AsShared());
		WriterThread = FRunnableThread::Create(
			WriterRunnable.Get(),
			*FString::Printf(TEXT("ExtendedSQL.Writer.%s"), *LogicalName),
			0,
			TPri_BelowNormal);

		if (!WriterThread)
		{
			OutError = MakeSubsystemError(
				EESQLErrorCode::InvalidState,
				FString::Printf(TEXT("Failed to create writer thread for database '%s'"), *LogicalName));
			WriterRunnable.Reset();
			WriterDatabase->Close();
			WriterDatabase.Reset();
			return false;
		}

		return true;
	}

	void Shutdown()
	{
		const bool bWasStopping = bStopping.Exchange(true);
		if (bWasStopping)
		{
			return;
		}

		if (WriterWake)
		{
			WriterWake->Trigger();
		}

		if (WriterThread)
		{
			WriterThread->WaitForCompletion();
			delete WriterThread;
			WriterThread = nullptr;
		}

		while (true)
		{
			int32 CurrentActiveReaders = 0;
			{
				FScopeLock ReaderLock(&ReaderPoolCS);
				CurrentActiveReaders = ActiveReaders;
			}

			if (CurrentActiveReaders == 0)
			{
				break;
			}

			if (ReaderReturnedEvent)
			{
				ReaderReturnedEvent->Wait(WriterWakeWaitMs);
			}
		}

		{
			FScopeLock ReaderLock(&ReaderPoolCS);
			for (const TSharedPtr<FESQLDatabase>& Reader : OwnedReaderConnections)
			{
				if (Reader.IsValid())
				{
					Reader->Close();
				}
			}

			OwnedReaderConnections.Reset();
			AvailableReaders.Reset();
		}

		if (WriterDatabase.IsValid())
		{
			WriterDatabase->Close();
			WriterDatabase.Reset();
		}
	}

	bool IsWriterThread() const
	{
		const uint32 CurrentWriterThreadId = WriterThreadId.Load();
		return CurrentWriterThreadId != 0 && CurrentWriterThreadId == FPlatformTLS::GetCurrentThreadId();
	}

	void EnqueueWriter(const TSharedPtr<FESQLWriterWorkItem, ESPMode::ThreadSafe>& WorkItem)
	{
		if (bStopping.Load())
		{
			const FESQLWriterWorkResult CancelledResult{
				MakeSubsystemFailure(EESQLErrorCode::Cancelled, TEXT("Database context is stopping")),
				FESQLTransactionHandle(),
				false};

			if (WorkItem->BlockingResponse.IsValid())
			{
				WorkItem->BlockingResponse->Result = CancelledResult.Result;
				WorkItem->BlockingResponse->Event->Trigger();
			}

			if (WorkItem->Completion)
			{
				WorkItem->Completion(CancelledResult);
			}

			return;
		}

		WriterQueue.Enqueue(WorkItem);
		if (WriterWake)
		{
			WriterWake->Trigger();
		}
	}

	FESQLQueryResult RunWriterSync(TUniqueFunction<FESQLWriterWorkResult(FESQLDatabaseContext&)> Operation, FESQLTransactionHandle* OutHandle = nullptr)
	{
		if (bStopping.Load())
		{
			return MakeSubsystemFailure(EESQLErrorCode::Cancelled, TEXT("Database context is stopping"));
		}

		if (IsWriterThread())
		{
			const FESQLWriterWorkResult WorkResult = Operation(*this);
			if (OutHandle && WorkResult.bHasTransactionHandle)
			{
				*OutHandle = WorkResult.TransactionHandle;
			}
			return WorkResult.Result;
		}

		TSharedRef<FESQLBlockingResponse, ESPMode::ThreadSafe> BlockingResponse = MakeShared<FESQLBlockingResponse, ESPMode::ThreadSafe>();
		TSharedRef<FESQLWriterWorkItem, ESPMode::ThreadSafe> WorkItem = MakeShared<FESQLWriterWorkItem, ESPMode::ThreadSafe>();
		WorkItem->BlockingResponse = BlockingResponse;
		WorkItem->Run = MoveTemp(Operation);
		EnqueueWriter(WorkItem);
		BlockingResponse->Event->Wait();

		if (OutHandle && BlockingResponse->bHasTransactionHandle)
		{
			*OutHandle = BlockingResponse->TransactionHandle;
		}

		return BlockingResponse->Result;
	}

	bool IsTablePrepared(const FString& PreparedKey) const
	{
		FScopeLock PreparedLock(&PreparedTablesCS);
		return PreparedTableKeys.Contains(PreparedKey);
	}

	void MarkTablePrepared(const FString& PreparedKey)
	{
		FScopeLock PreparedLock(&PreparedTablesCS);
		PreparedTableKeys.Add(PreparedKey);
	}

	FESQLReaderLease AcquireReaderLease(FESQLError& OutError);
	void ReleaseReader(FESQLDatabase* Database, bool bPooled);
	void CancelPendingWriterWork();
	void RollbackAbandonedTransactions();
	bool RollbackTopTransaction();

	FString LogicalName;
	FString ResolvedPath;
	EESQLDatabaseScope Scope = EESQLDatabaseScope::Local;
	EESQLDatabasePersistence Persistence = EESQLDatabasePersistence::Persistent;
	bool bReadOnly = false;
	TAtomic<bool> bStopping = false;
	TAtomic<uint32> WriterThreadId = 0;

	FEvent* WriterWake = nullptr;
	FEvent* ReaderReturnedEvent = nullptr;
	FRunnableThread* WriterThread = nullptr;
	TUniquePtr<FESQLDatabaseContextWriterRunnable> WriterRunnable;
	TQueue<TSharedPtr<FESQLWriterWorkItem, ESPMode::ThreadSafe>, EQueueMode::Mpsc> WriterQueue;
	TSharedPtr<FESQLDatabase> WriterDatabase;

	mutable FCriticalSection ReaderPoolCS;
	TArray<TSharedPtr<FESQLDatabase>> OwnedReaderConnections;
	TArray<FESQLDatabase*> AvailableReaders;
	int32 ActiveReaders = 0;
	int32 MaxReaderPoolSize = 0;

	mutable FCriticalSection PreparedTablesCS;
	TSet<FString> PreparedTableKeys;

	int64 NextTransactionId = 1;
	TArray<FESQLActiveTransactionState> TransactionStack;
};

struct FESQLReaderLease
{
	TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context;
	FESQLDatabase* Database = nullptr;
	TSharedPtr<FESQLDatabase> TemporaryConnection;
	bool bPooled = false;

	FESQLReaderLease() = default;
	FESQLReaderLease(const FESQLReaderLease&) = delete;
	FESQLReaderLease& operator=(const FESQLReaderLease&) = delete;

	FESQLReaderLease(FESQLReaderLease&& Other) noexcept
		: Context(MoveTemp(Other.Context))
		, Database(Other.Database)
		, TemporaryConnection(MoveTemp(Other.TemporaryConnection))
		, bPooled(Other.bPooled)
	{
		Other.Database = nullptr;
		Other.bPooled = false;
	}

	FESQLReaderLease& operator=(FESQLReaderLease&& Other) noexcept
	{
		if (this != &Other)
		{
			Release();
			Context = MoveTemp(Other.Context);
			Database = Other.Database;
			TemporaryConnection = MoveTemp(Other.TemporaryConnection);
			bPooled = Other.bPooled;
			Other.Database = nullptr;
			Other.bPooled = false;
		}

		return *this;
	}

	~FESQLReaderLease()
	{
		Release();
	}

	void Release()
	{
		if (Context.IsValid() && Database)
		{
			Context->ReleaseReader(Database, bPooled);
		}

		if (TemporaryConnection.IsValid())
		{
			TemporaryConnection->Close();
			TemporaryConnection.Reset();
		}

		Context.Reset();
		Database = nullptr;
		bPooled = false;
	}

	explicit operator bool() const
	{
		return Database != nullptr;
	}
};

uint32 FESQLDatabaseContextWriterRunnable::Run()
{
	Context->WriterThreadId.Store(FPlatformTLS::GetCurrentThreadId());

	while (true)
	{
		Context->RollbackAbandonedTransactions();

		TSharedPtr<FESQLWriterWorkItem, ESPMode::ThreadSafe> WorkItem;
		if (Context->WriterQueue.Dequeue(WorkItem))
		{
			if (WorkItem.IsValid())
			{
				ExecuteWorkItem(WorkItem);
			}
			continue;
		}

		if (Context->bStopping.Load())
		{
			break;
		}

		if (Context->WriterWake)
		{
			Context->WriterWake->Wait(WriterWakeWaitMs);
		}
	}

	Context->CancelPendingWriterWork();
	Context->RollbackAbandonedTransactions();
	while (!Context->TransactionStack.IsEmpty())
	{
		Context->RollbackTopTransaction();
	}

	return 0;
}

void FESQLDatabaseContextWriterRunnable::ExecuteWorkItem(const TSharedPtr<FESQLWriterWorkItem, ESPMode::ThreadSafe>& WorkItem) const
{
	FESQLWriterWorkResult WorkResult;
	WorkResult.Result = MakeSubsystemFailure(EESQLErrorCode::InvalidState, TEXT("Writer work item did not execute"));

	if (!Context->WriterDatabase.IsValid() || !Context->WriterDatabase->IsOpen())
	{
		WorkResult.Result = MakeSubsystemFailure(EESQLErrorCode::InvalidState, TEXT("Writer database is not open"));
	}
	else if (WorkItem->TicketState.IsValid() && WorkItem->TicketState->bCancelled.Load())
	{
		WorkResult.Result = MakeSubsystemFailure(EESQLErrorCode::Cancelled, TEXT("Async SQL ticket was cancelled"));
	}
	else if (WorkItem->Run)
	{
		WorkResult = WorkItem->Run(*Context);
	}

	if (WorkItem->BlockingResponse.IsValid())
	{
		WorkItem->BlockingResponse->Result = WorkResult.Result;
		WorkItem->BlockingResponse->TransactionHandle = WorkResult.TransactionHandle;
		WorkItem->BlockingResponse->bHasTransactionHandle = WorkResult.bHasTransactionHandle;
		WorkItem->BlockingResponse->Event->Trigger();
	}

	if (WorkItem->Completion)
	{
		WorkItem->Completion(WorkResult);
	}
}

FESQLReaderLease FESQLDatabaseContext::AcquireReaderLease(FESQLError& OutError)
{
	FESQLReaderLease Lease;
	Lease.Context = AsShared();

	if (bStopping.Load())
	{
		OutError = MakeSubsystemError(EESQLErrorCode::Cancelled, TEXT("Database context is stopping"));
		Lease.Context.Reset();
		return Lease;
	}

	if (ResolvedPath.IsEmpty() || Persistence == EESQLDatabasePersistence::Session)
	{
		OutError = MakeSubsystemError(EESQLErrorCode::WrongThread, TEXT("This database does not expose a file-backed reader connection"));
		Lease.Context.Reset();
		return Lease;
	}

	FScopeLock ReaderLock(&ReaderPoolCS);
	++ActiveReaders;

	if (AvailableReaders.Num() > 0)
	{
		Lease.Database = AvailableReaders.Pop(false);
		Lease.bPooled = true;
		return Lease;
	}

	if (OwnedReaderConnections.Num() < MaxReaderPoolSize)
	{
		const FESQLDatabaseOpenResult OpenResult = FESQLDatabase::OpenReadOnly(ResolvedPath);
		if (!OpenResult)
		{
			--ActiveReaders;
			OutError = OpenResult.GetError();
			Lease.Context.Reset();
			return Lease;
		}

		TSharedPtr<FESQLDatabase> ReaderConnection = OpenResult.GetValue();
		Lease.Database = ReaderConnection.Get();
		Lease.bPooled = true;
		OwnedReaderConnections.Add(MoveTemp(ReaderConnection));
		return Lease;
	}

	const FESQLDatabaseOpenResult OpenResult = FESQLDatabase::OpenReadOnly(ResolvedPath);
	if (!OpenResult)
	{
		--ActiveReaders;
		OutError = OpenResult.GetError();
		Lease.Context.Reset();
		return Lease;
	}

	Lease.TemporaryConnection = OpenResult.GetValue();
	Lease.Database = Lease.TemporaryConnection.Get();
	Lease.bPooled = false;
	return Lease;
}

void FESQLDatabaseContext::ReleaseReader(FESQLDatabase* Database, const bool bPooled)
{
	FScopeLock ReaderLock(&ReaderPoolCS);

	if (bPooled && Database)
	{
		Database->ResetThreadAffinity();
		if (!bStopping.Load())
		{
			AvailableReaders.Add(Database);
		}
	}

	ActiveReaders = FMath::Max(0, ActiveReaders - 1);
	if (bStopping.Load() && ActiveReaders == 0 && ReaderReturnedEvent)
	{
		ReaderReturnedEvent->Trigger();
	}
}

void FESQLDatabaseContext::CancelPendingWriterWork()
{
	TSharedPtr<FESQLWriterWorkItem, ESPMode::ThreadSafe> WorkItem;
	while (WriterQueue.Dequeue(WorkItem))
	{
		FESQLWriterWorkResult CancelledResult;
		CancelledResult.Result = MakeSubsystemFailure(EESQLErrorCode::Cancelled, TEXT("Database context is shutting down"));

		if (WorkItem->BlockingResponse.IsValid())
		{
			WorkItem->BlockingResponse->Result = CancelledResult.Result;
			WorkItem->BlockingResponse->Event->Trigger();
		}

		if (WorkItem->Completion)
		{
			WorkItem->Completion(CancelledResult);
		}
	}
}

bool FESQLDatabaseContext::RollbackTopTransaction()
{
	if (!WriterDatabase.IsValid() || TransactionStack.Num() == 0)
	{
		return false;
	}

	const FESQLActiveTransactionState State = TransactionStack.Last();
	FESQLQueryResult Result = FESQLQueryResult::Success();

	if (TransactionStack.Num() == 1)
	{
		Result = MakeSubsystemSuccessFromErrorResult(WriterDatabase->RollbackTransaction());
	}
	else
	{
		Result = WriterDatabase->Execute(FString::Printf(TEXT("ROLLBACK TO SAVEPOINT \"%s\""), *State.SavepointName));
		if (Result.bSuccess)
		{
			Result = WriterDatabase->Execute(FString::Printf(TEXT("RELEASE SAVEPOINT \"%s\""), *State.SavepointName));
		}
	}

	if (Result.bSuccess)
	{
		TransactionStack.Pop();
		return true;
	}

	UE_LOG(LogExtendedSQL, Warning, TEXT("Failed to rollback abandoned transaction on '%s': %s"), *LogicalName, *Result.ErrorMessage);
	return false;
}

void FESQLDatabaseContext::RollbackAbandonedTransactions()
{
	while (TransactionStack.Num() > 0)
	{
		const FESQLActiveTransactionState& State = TransactionStack.Last();
		if (State.Lifetime.IsValid())
		{
			break;
		}

		RollbackTopTransaction();
	}
}

void UESQLSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RefreshCachedNetMode();
	UE_LOG(LogExtendedSQL, Log, TEXT("UESQLSubsystem initialized (NetMode: %d)"), static_cast<int32>(CachedNetMode.GetValue()));
}

void UESQLSubsystem::Deinitialize()
{
	CloseAllDatabases();

	{
		FScopeLock TicketLock(&TicketsCS);
		TicketStates.Empty();
	}

	UE_LOG(LogExtendedSQL, Log, TEXT("UESQLSubsystem deinitialized"));
	Super::Deinitialize();
}

FESQLQueryResult UESQLSubsystem::OpenDatabase(
	const FString& DatabaseName,
	EESQLDatabaseScope Scope,
	EESQLDatabasePersistence Persistence,
	const FString& FileName)
{
	RefreshCachedNetMode();

	FString ResolvedDatabaseName;
	FString ResolvedPath;
	bool bReadOnly = false;
	const FESQLQueryResult ResolveResult = ResolveDatabaseOpenPath(
		DatabaseName,
		Scope,
		Persistence,
		FileName,
		ResolvedDatabaseName,
		ResolvedPath,
		bReadOnly);
	if (!ResolveResult.bSuccess)
	{
		return ResolveResult;
	}

	if (const TOptional<FESQLError> GateError = CheckScopeGate(Scope, false, ResolvedDatabaseName))
	{
		return FESQLQueryResult::Failure(GateError.GetValue());
	}

	if (FindDatabaseContext(ResolvedDatabaseName).IsValid())
	{
		return FESQLQueryResult::Success();
	}

	TSharedRef<FESQLDatabaseContext, ESPMode::ThreadSafe> Context = MakeShared<FESQLDatabaseContext, ESPMode::ThreadSafe>();
	Context->LogicalName = ResolvedDatabaseName;
	Context->ResolvedPath = ResolvedPath;
	Context->Scope = Scope;
	Context->Persistence = Persistence;
	Context->bReadOnly = bReadOnly;

	FESQLError OpenError;
	if (!Context->Initialize(OpenError))
	{
		return FESQLQueryResult::Failure(OpenError);
	}

	{
		FScopeLock ContextLock(&DatabaseContextsCS);
		if (DatabaseContexts.Contains(ResolvedDatabaseName))
		{
			Context->Shutdown();
			return FESQLQueryResult::Success();
		}

		if (!DatabaseContexts.Contains(ResolvedDatabaseName))
		{
			DatabaseContexts.Add(ResolvedDatabaseName, Context);
		}
	}

	return FESQLQueryResult::Success();
}

void UESQLSubsystem::CloseDatabase(const FString& DatabaseName)
{
	EESQLDatabaseScope DummyScope = EESQLDatabaseScope::Local;
	const FString ResolvedName = NormalizeLogicalDatabaseName(DatabaseName, DummyScope);

	TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context;
	{
		FScopeLock ContextLock(&DatabaseContextsCS);
		if (TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe>* Found = DatabaseContexts.Find(ResolvedName))
		{
			Context = *Found;
			DatabaseContexts.Remove(ResolvedName);
		}
	}

	if (Context.IsValid())
	{
		Context->Shutdown();
	}
}

void UESQLSubsystem::CloseAllDatabases()
{
	TArray<TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe>> Contexts;
	{
		FScopeLock ContextLock(&DatabaseContextsCS);
		DatabaseContexts.GenerateValueArray(Contexts);
		DatabaseContexts.Empty();
	}

	for (const TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe>& Context : Contexts)
	{
		if (Context.IsValid())
		{
			Context->Shutdown();
		}
	}
}

bool UESQLSubsystem::IsDatabaseOpen(const FString& DatabaseName) const
{
	return FindDatabaseContext(DatabaseName).IsValid();
}

TArray<FString> UESQLSubsystem::GetOpenDatabaseNames() const
{
	TArray<FString> Names;
	FScopeLock ContextLock(&DatabaseContextsCS);
	DatabaseContexts.GetKeys(Names);
	return Names;
}

FESQLQueryResult UESQLSubsystem::DeleteDatabase(
	const FString& DatabaseName,
	EESQLDatabaseScope Scope,
	const FString& FileName)
{
	RefreshCachedNetMode();

	FString ResolvedDatabaseName;
	FString ResolvedPath;
	bool bReadOnly = false;
	const FESQLQueryResult ResolveResult = ResolveDatabaseOpenPath(
		DatabaseName,
		Scope,
		EESQLDatabasePersistence::Persistent,
		FileName,
		ResolvedDatabaseName,
		ResolvedPath,
		bReadOnly);
	if (!ResolveResult.bSuccess)
	{
		return ResolveResult;
	}

	if (FindDatabaseContext(ResolvedDatabaseName).IsValid())
	{
		return MakeSubsystemFailure(
			EESQLErrorCode::InvalidState,
			FString::Printf(TEXT("Cannot delete database '%s' while it is open"), *ResolvedDatabaseName));
	}

	if (const TOptional<FESQLError> GateError = CheckScopeGate(Scope, true, ResolvedDatabaseName))
	{
		return FESQLQueryResult::Failure(GateError.GetValue());
	}

	if (bReadOnly)
	{
		return MakeSubsystemFailure(EESQLErrorCode::ReadonlyViolation, TEXT("Readonly databases cannot be deleted through the runtime subsystem"));
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	bool bDeleted = false;
	if (PlatformFile.FileExists(*ResolvedPath))
	{
		bDeleted = PlatformFile.DeleteFile(*ResolvedPath);
	}

	PlatformFile.DeleteFile(*(ResolvedPath + TEXT("-wal")));
	PlatformFile.DeleteFile(*(ResolvedPath + TEXT("-shm")));

	const FString ProjectRoot = FESQLUsqliteSerializer::GetProjectRootForDatabasePath(ResolvedPath);
	if (PlatformFile.DirectoryExists(*ProjectRoot))
	{
		bDeleted = PlatformFile.DeleteDirectoryRecursively(*ProjectRoot) || bDeleted;
	}

	return bDeleted
		? FESQLQueryResult::Success()
		: MakeSubsystemFailure(EESQLErrorCode::NotFound, FString::Printf(TEXT("Database file not found at: %s"), *ResolvedPath));
}

FString UESQLSubsystem::GetDatabaseFilePath(const FString& DatabaseName) const
{
	if (const TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context = FindDatabaseContext(DatabaseName))
	{
		return Context->ResolvedPath;
	}

	return FString();
}

bool UESQLSubsystem::IsSessionDatabase(const FString& DatabaseName) const
{
	if (const TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context = FindDatabaseContext(DatabaseName))
	{
		return Context->Persistence == EESQLDatabasePersistence::Session;
	}

	return false;
}

bool UESQLSubsystem::IsDedicatedServer() const
{
	return CachedNetMode == NM_DedicatedServer;
}

bool UESQLSubsystem::IsListenServer() const
{
	return CachedNetMode == NM_ListenServer;
}

bool UESQLSubsystem::HasServerAuthority() const
{
	return CachedNetMode != NM_Client;
}

FESQLQueryResult UESQLSubsystem::ExecuteSQL(const FString& DatabaseName, const FString& SQL)
{
	return IsLikelyReadOnlySql(SQL)
		? RunRawRead(DatabaseName, SQL, TArray<FESQLBindingValue>())
		: RunRawWrite(DatabaseName, SQL, TArray<FESQLBindingValue>());
}

FESQLQueryResult UESQLSubsystem::ExecuteSQLWithBindings(
	const FString& DatabaseName,
	const FString& SQL,
	const TArray<FString>& Bindings)
{
	return ExecuteSQLWithValues(DatabaseName, SQL, MakeBindingValues(Bindings));
}

FESQLQueryResult UESQLSubsystem::ExecuteSQLWithValues(
	const FString& DatabaseName,
	const FString& SQL,
	const TArray<FESQLBindingValue>& Bindings)
{
	return IsLikelyReadOnlySql(SQL)
		? RunRawRead(DatabaseName, SQL, Bindings)
		: RunRawWrite(DatabaseName, SQL, Bindings);
}

FESQLQueryResult UESQLSubsystem::QuerySQL(
	const FString& DatabaseName,
	const FString& SQL,
	const TArray<FESQLBindingValue>& Bindings)
{
	return RunRawRead(DatabaseName, SQL, Bindings);
}

FESQLTicket UESQLSubsystem::AsyncExecuteSQL(
	const FString& DatabaseName,
	const FString& SQL,
	const TArray<FESQLBindingValue>& Bindings,
	const FOnESQLQueryCompleteCallback& OnComplete)
{
	if (IsLikelyReadOnlySql(SQL))
	{
		return AsyncQuerySQL(DatabaseName, SQL, Bindings, OnComplete);
	}

	RefreshCachedNetMode();
	const TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context = FindDatabaseContext(DatabaseName);
	if (!Context.IsValid())
	{
		FinalizeAsyncTicket(FGuid(), OnComplete, MakeSubsystemFailure(EESQLErrorCode::NotFound, FString::Printf(TEXT("Database '%s' is not open"), *DatabaseName)));
		return FESQLTicket();
	}

	if (const TOptional<FESQLError> GateError = CheckScopeGate(Context->Scope, true, Context->LogicalName))
	{
		FinalizeAsyncTicket(FGuid(), OnComplete, FESQLQueryResult::Failure(GateError.GetValue()));
		return FESQLTicket();
	}

	const TSharedPtr<FESQLAsyncTicketState, ESPMode::ThreadSafe> TicketState = RegisterTicketState(Context->LogicalName);
	const FGuid TicketId = TicketState->Ticket.Id;
	TWeakObjectPtr<UESQLSubsystem> WeakThis(this);
	const FString SqlCopy = SQL;
	const TArray<FESQLBindingValue> BindingsCopy = Bindings;
	const FOnESQLQueryCompleteCallback Callback = OnComplete;

	TSharedRef<FESQLWriterWorkItem, ESPMode::ThreadSafe> WorkItem = MakeShared<FESQLWriterWorkItem, ESPMode::ThreadSafe>();
	WorkItem->TicketState = TicketState;
	WorkItem->Run = [SqlCopy, BindingsCopy](FESQLDatabaseContext& DatabaseContext)
	{
		FESQLWriterWorkResult WorkResult;
		WorkResult.Result = ExecuteDatabaseSql(*DatabaseContext.WriterDatabase, SqlCopy, BindingsCopy);
		return WorkResult;
	};
	WorkItem->Completion = [WeakThis, TicketId, Callback](const FESQLWriterWorkResult& WorkResult)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->FinalizeAsyncTicket(TicketId, Callback, WorkResult.Result);
		}
	};
	Context->EnqueueWriter(WorkItem);
	return TicketState->Ticket;
}

FESQLTicket UESQLSubsystem::AsyncQuerySQL(
	const FString& DatabaseName,
	const FString& SQL,
	const TArray<FESQLBindingValue>& Bindings,
	const FOnESQLQueryCompleteCallback& OnComplete)
{
	RefreshCachedNetMode();
	const TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context = FindDatabaseContext(DatabaseName);
	if (!Context.IsValid())
	{
		FinalizeAsyncTicket(FGuid(), OnComplete, MakeSubsystemFailure(EESQLErrorCode::NotFound, FString::Printf(TEXT("Database '%s' is not open"), *DatabaseName)));
		return FESQLTicket();
	}

	if (const TOptional<FESQLError> GateError = CheckScopeGate(Context->Scope, false, Context->LogicalName))
	{
		FinalizeAsyncTicket(FGuid(), OnComplete, FESQLQueryResult::Failure(GateError.GetValue()));
		return FESQLTicket();
	}

	const TSharedPtr<FESQLAsyncTicketState, ESPMode::ThreadSafe> TicketState = RegisterTicketState(Context->LogicalName);
	const FGuid TicketId = TicketState->Ticket.Id;
	const FString SqlCopy = SQL;
	const TArray<FESQLBindingValue> BindingsCopy = Bindings;
	const FOnESQLQueryCompleteCallback Callback = OnComplete;
	TWeakObjectPtr<UESQLSubsystem> WeakThis(this);

	if (Context->Persistence == EESQLDatabasePersistence::Session || Context->ResolvedPath.IsEmpty())
	{
		TSharedRef<FESQLWriterWorkItem, ESPMode::ThreadSafe> WorkItem = MakeShared<FESQLWriterWorkItem, ESPMode::ThreadSafe>();
		WorkItem->TicketState = TicketState;
		WorkItem->Run = [SqlCopy, BindingsCopy](FESQLDatabaseContext& DatabaseContext)
		{
			FESQLWriterWorkResult WorkResult;
			WorkResult.Result = ExecuteDatabaseSql(*DatabaseContext.WriterDatabase, SqlCopy, BindingsCopy);
			return WorkResult;
		};
		WorkItem->Completion = [WeakThis, TicketId, Callback](const FESQLWriterWorkResult& WorkResult)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->FinalizeAsyncTicket(TicketId, Callback, WorkResult.Result);
			}
		};
		Context->EnqueueWriter(WorkItem);
		return TicketState->Ticket;
	}

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Context, TicketState, TicketId, SqlCopy, BindingsCopy, Callback, WeakThis]() mutable
	{
		if (TicketState->bCancelled.Load())
		{
			if (WeakThis.IsValid())
			{
				WeakThis->FinalizeAsyncTicket(TicketId, Callback, MakeSubsystemFailure(EESQLErrorCode::Cancelled, TEXT("Async SQL ticket was cancelled")));
			}
			return;
		}

		FESQLError ReaderError;
		FESQLReaderLease ReaderLease = Context->AcquireReaderLease(ReaderError);
		if (!ReaderLease)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->FinalizeAsyncTicket(TicketId, Callback, MakeSubsystemFailure(ReaderError));
			}
			return;
		}

		const FESQLQueryResult Result = ExecuteDatabaseSql(*ReaderLease.Database, SqlCopy, BindingsCopy);
		if (WeakThis.IsValid())
		{
			WeakThis->FinalizeAsyncTicket(TicketId, Callback, Result);
		}
	});

	return TicketState->Ticket;
}

bool UESQLSubsystem::CancelTicket(const FESQLTicket& Ticket)
{
	if (!Ticket.IsValid())
	{
		return false;
	}

	FScopeLock TicketLock(&TicketsCS);
	if (TWeakPtr<FESQLAsyncTicketState, ESPMode::ThreadSafe>* Found = TicketStates.Find(Ticket.Id))
	{
		if (const TSharedPtr<FESQLAsyncTicketState, ESPMode::ThreadSafe> TicketState = Found->Pin())
		{
			TicketState->bCancelled.Store(true);
			return true;
		}
	}

	return false;
}

FESQLQueryResult UESQLSubsystem::QueryTable(UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec)
{
	TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context;
	const FESQLQueryResult ContextResult = EnsureTableContext(GetGameInstance(), TableAsset, false, Context);
	if (!ContextResult.bSuccess)
	{
		return ContextResult;
	}

	FString SQL;
	TArray<FESQLBindingValue> Bindings;
	const FESQLQueryResult BuildResult = TableAsset->BuildQueryRowsSQL(QuerySpec, SQL, Bindings);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	return Context->Persistence == EESQLDatabasePersistence::Session || Context->ResolvedPath.IsEmpty()
		? Context->RunWriterSync([SQL, Bindings](FESQLDatabaseContext& DatabaseContext)
		{
			FESQLWriterWorkResult WorkResult;
			WorkResult.Result = ExecuteDatabaseSql(*DatabaseContext.WriterDatabase, SQL, Bindings);
			return WorkResult;
		})
		: [&Context, SQL, Bindings]()
		{
			FESQLError ReaderError;
			FESQLReaderLease ReaderLease = Context->AcquireReaderLease(ReaderError);
			if (!ReaderLease)
			{
				return MakeSubsystemFailure(ReaderError);
			}

			return ExecuteDatabaseSql(*ReaderLease.Database, SQL, Bindings);
		}();
}

FESQLQueryResult UESQLSubsystem::QuerySQLRows(UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec)
{
	return QueryTable(TableAsset, QuerySpec);
}

void UESQLSubsystem::AsyncQueryTable(UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec, TFunction<void(const FESQLQueryResult&)> OnComplete)
{
	if (!OnComplete)
	{
		return;
	}

	TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context;
	const FESQLQueryResult ContextResult = EnsureTableContext(GetGameInstance(), TableAsset, false, Context);
	if (!ContextResult.bSuccess)
	{
		OnComplete(ContextResult);
		return;
	}

	FString SQL;
	TArray<FESQLBindingValue> Bindings;
	const FESQLQueryResult BuildResult = TableAsset->BuildQueryRowsSQL(QuerySpec, SQL, Bindings);
	if (!BuildResult.bSuccess)
	{
		OnComplete(BuildResult);
		return;
	}

	if (Context->Persistence == EESQLDatabasePersistence::Session || Context->ResolvedPath.IsEmpty())
	{
		TSharedRef<FESQLWriterWorkItem, ESPMode::ThreadSafe> WorkItem = MakeShared<FESQLWriterWorkItem, ESPMode::ThreadSafe>();
		WorkItem->Run = [SQL, Bindings](FESQLDatabaseContext& DatabaseContext)
		{
			FESQLWriterWorkResult WorkResult;
			WorkResult.Result = ExecuteDatabaseSql(*DatabaseContext.WriterDatabase, SQL, Bindings);
			return WorkResult;
		};
		WorkItem->Completion = [OnComplete = MoveTemp(OnComplete)](const FESQLWriterWorkResult& WorkResult) mutable
		{
			AsyncTask(ENamedThreads::GameThread, [OnComplete = MoveTemp(OnComplete), Result = WorkResult.Result]() mutable
			{
				OnComplete(Result);
			});
		};
		Context->EnqueueWriter(WorkItem);
		return;
	}

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Context, SQL = MoveTemp(SQL), Bindings = MoveTemp(Bindings), OnComplete = MoveTemp(OnComplete)]() mutable
	{
		FESQLError ReaderError;
		FESQLReaderLease ReaderLease = Context->AcquireReaderLease(ReaderError);
		const FESQLQueryResult Result = ReaderLease
			? ExecuteDatabaseSql(*ReaderLease.Database, SQL, Bindings)
			: MakeSubsystemFailure(ReaderError);

		AsyncTask(ENamedThreads::GameThread, [OnComplete = MoveTemp(OnComplete), Result]() mutable
		{
			OnComplete(Result);
		});
	});
}

FESQLQueryResult UESQLSubsystem::DeleteTableRow(UESQLTableAsset* TableAsset, const FString& RowId)
{
	TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context;
	const FESQLQueryResult ContextResult = EnsureTableContext(GetGameInstance(), TableAsset, true, Context);
	if (!ContextResult.bSuccess)
	{
		return ContextResult;
	}

	if (RowId.TrimStartAndEnd().IsEmpty())
	{
		return MakeSubsystemFailure(EESQLErrorCode::InvalidArgument, TEXT("RowId cannot be empty"));
	}

	const FString SQL = FString::Printf(
		TEXT("DELETE FROM \"%s\" WHERE \"%s\"=?1"),
		*TableAsset->TableName,
		*TableAsset->PrimaryKeyColumn);

	return Context->RunWriterSync([SQL, RowId](FESQLDatabaseContext& DatabaseContext)
	{
		FESQLWriterWorkResult WorkResult;
		WorkResult.Result = DatabaseContext.WriterDatabase->Execute(SQL, TArray<FString>{ RowId });
		return WorkResult;
	});
}

FESQLQueryResult UESQLSubsystem::CountTableRows(UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec, int64& OutCount)
{
	OutCount = 0;

	TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context;
	const FESQLQueryResult ContextResult = EnsureTableContext(GetGameInstance(), TableAsset, false, Context);
	if (!ContextResult.bSuccess)
	{
		return ContextResult;
	}

	FString SQL;
	TArray<FESQLBindingValue> Bindings;
	const FESQLQueryResult BuildResult = TableAsset->BuildQuerySQL(TEXT("SELECT COUNT(*) AS RowCount"), QuerySpec, false, false, SQL, Bindings);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	const FESQLQueryResult QueryResult = Context->Persistence == EESQLDatabasePersistence::Session || Context->ResolvedPath.IsEmpty()
		? Context->RunWriterSync([SQL, Bindings](FESQLDatabaseContext& DatabaseContext)
		{
			FESQLWriterWorkResult WorkResult;
			WorkResult.Result = ExecuteDatabaseSql(*DatabaseContext.WriterDatabase, SQL, Bindings);
			return WorkResult;
		})
		: [&Context, SQL, Bindings]()
		{
			FESQLError ReaderError;
			FESQLReaderLease ReaderLease = Context->AcquireReaderLease(ReaderError);
			if (!ReaderLease)
			{
				return MakeSubsystemFailure(ReaderError);
			}

			return ExecuteDatabaseSql(*ReaderLease.Database, SQL, Bindings);
		}();

	if (!QueryResult.bSuccess)
	{
		return QueryResult;
	}

	const FESQLRow* Row = QueryResult.GetFirstRow();
	if (!Row || !Row->TryGetInt64(TEXT("RowCount"), OutCount))
	{
		return MakeSubsystemFailure(EESQLErrorCode::InvalidState, TEXT("Failed to parse row count result"));
	}

	return FESQLQueryResult::Success();
}

FESQLQueryResult UESQLSubsystem::CountSQLRows(UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec, int64& OutCount)
{
	return CountTableRows(TableAsset, QuerySpec, OutCount);
}

FESQLQueryResult UESQLSubsystem::CountSQLRowsByField(UESQLTableAsset* TableAsset, FName FieldName, EESQLFilterOperation Operation, const FESQLBindingValue& Value, int64& OutCount)
{
	return CountTableRows(TableAsset, BuildSingleFilterQuerySpec(FieldName, Operation, Value), OutCount);
}

FESQLQueryResult UESQLSubsystem::LoadRowIntoStruct(
	UObject* WorldContextObject,
	UESQLTableAsset* TableAsset,
	const FString& RowId,
	void* OutStructData,
	const UScriptStruct* StructType)
{
	TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context;
	const FESQLQueryResult ContextResult = EnsureTableContext(WorldContextObject, TableAsset, false, Context);
	if (!ContextResult.bSuccess)
	{
		return ContextResult;
	}

	FString SQL;
	TArray<FString> Bindings;
	const FESQLQueryResult BuildResult = TableAsset->BuildGetRowByIdSQL(RowId, SQL, Bindings);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	const FESQLQueryResult QueryResult = Context->Persistence == EESQLDatabasePersistence::Session || Context->ResolvedPath.IsEmpty()
		? Context->RunWriterSync([SQL, Bindings](FESQLDatabaseContext& DatabaseContext)
		{
			FESQLWriterWorkResult WorkResult;
			WorkResult.Result = DatabaseContext.WriterDatabase->Execute(SQL, Bindings);
			return WorkResult;
		})
		: [&Context, SQL, Bindings]()
		{
			FESQLError ReaderError;
			FESQLReaderLease ReaderLease = Context->AcquireReaderLease(ReaderError);
			if (!ReaderLease)
			{
				return MakeSubsystemFailure(ReaderError);
			}

			return ReaderLease.Database->Execute(SQL, Bindings);
		}();

	if (!QueryResult.bSuccess)
	{
		return QueryResult;
	}

	return PopulateQueryResultIntoStruct(TableAsset, QueryResult, OutStructData, StructType);
}

FESQLQueryResult UESQLSubsystem::LoadRowsIntoStructArray(
	UObject* WorldContextObject,
	UESQLTableAsset* TableAsset,
	const FESQLQuerySpec& QuerySpec,
	void* OutArrayData,
	const FArrayProperty* ArrayProperty)
{
	const FESQLQueryResult QueryResult = QueryTable(TableAsset, QuerySpec);
	if (!QueryResult.bSuccess)
	{
		return QueryResult;
	}

	return PopulateQueryResultIntoStructArray(TableAsset, QueryResult, OutArrayData, ArrayProperty);
}

FESQLQueryResult UESQLSubsystem::SaveRowFromStruct(
	UObject* WorldContextObject,
	UESQLTableAsset* TableAsset,
	const void* StructData,
	const UScriptStruct* StructType,
	FString* OutResolvedRowId,
	const FString& RowIdOverride)
{
	TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context;
	const FESQLQueryResult ContextResult = EnsureTableContext(WorldContextObject, TableAsset, true, Context);
	if (!ContextResult.bSuccess)
	{
		return ContextResult;
	}

	FString SQL;
	TArray<FESQLBindingValue> Bindings;
	FString ResolvedRowId;
	const FESQLQueryResult BuildResult = TableAsset->BuildSaveRowSQL(StructData, StructType, SQL, Bindings, ResolvedRowId, RowIdOverride);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	const FESQLQueryResult SaveResult = Context->RunWriterSync([SQL, Bindings](FESQLDatabaseContext& DatabaseContext)
	{
		FESQLWriterWorkResult WorkResult;
		WorkResult.Result = ExecuteDatabaseSql(*DatabaseContext.WriterDatabase, SQL, Bindings);
		return WorkResult;
	});

	if (SaveResult.bSuccess && OutResolvedRowId)
	{
		*OutResolvedRowId = ResolvedRowId;
	}

	return SaveResult;
}

void UESQLSubsystem::AsyncSaveRowFromStruct(
	UObject* WorldContextObject,
	UESQLTableAsset* TableAsset,
	const void* StructData,
	const UScriptStruct* StructType,
	TFunction<void(const FESQLQueryResult&, const FString&)> OnComplete,
	const FString& RowIdOverride)
{
	if (!OnComplete)
	{
		return;
	}

	TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context;
	const FESQLQueryResult ContextResult = EnsureTableContext(WorldContextObject, TableAsset, true, Context);
	if (!ContextResult.bSuccess)
	{
		OnComplete(ContextResult, FString());
		return;
	}

	FString SQL;
	TArray<FESQLBindingValue> Bindings;
	FString ResolvedRowId;
	const FESQLQueryResult BuildResult = TableAsset->BuildSaveRowSQL(StructData, StructType, SQL, Bindings, ResolvedRowId, RowIdOverride);
	if (!BuildResult.bSuccess)
	{
		OnComplete(BuildResult, FString());
		return;
	}

	TSharedRef<FESQLWriterWorkItem, ESPMode::ThreadSafe> WorkItem = MakeShared<FESQLWriterWorkItem, ESPMode::ThreadSafe>();
	WorkItem->Run = [SQL, Bindings](FESQLDatabaseContext& DatabaseContext)
	{
		FESQLWriterWorkResult WorkResult;
		WorkResult.Result = ExecuteDatabaseSql(*DatabaseContext.WriterDatabase, SQL, Bindings);
		return WorkResult;
	};
	WorkItem->Completion = [OnComplete = MoveTemp(OnComplete), ResolvedRowId = MoveTemp(ResolvedRowId)](const FESQLWriterWorkResult& WorkResult) mutable
	{
		AsyncTask(ENamedThreads::GameThread, [OnComplete = MoveTemp(OnComplete), Result = WorkResult.Result, ResolvedRowId]() mutable
		{
			OnComplete(Result, Result.bSuccess ? ResolvedRowId : FString());
		});
	};
	Context->EnqueueWriter(WorkItem);
}

FESQLQueryResult UESQLSubsystem::PopulateQueryResultIntoStruct(
	UESQLTableAsset* TableAsset,
	const FESQLQueryResult& QueryResult,
	void* OutStructData,
	const UScriptStruct* StructType) const
{
	if (!TableAsset)
	{
		return MakeSubsystemFailure(EESQLErrorCode::InvalidArgument, TEXT("TableAsset is null"));
	}

	return TableAsset->PopulateQueryResultIntoStruct(QueryResult, OutStructData, StructType);
}

FESQLQueryResult UESQLSubsystem::PopulateQueryResultIntoStructArray(
	UESQLTableAsset* TableAsset,
	const FESQLQueryResult& QueryResult,
	void* OutArrayData,
	const FArrayProperty* ArrayProperty) const
{
	if (!TableAsset)
	{
		return MakeSubsystemFailure(EESQLErrorCode::InvalidArgument, TEXT("TableAsset is null"));
	}

	return TableAsset->PopulateQueryResultIntoStructArray(QueryResult, OutArrayData, ArrayProperty);
}

DEFINE_FUNCTION(UESQLSubsystem::execPopulateSQLRowFromResult)
{
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_STRUCT(FESQLQueryResult, QueryResult);

	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FProperty* StructProperty = Stack.MostRecentProperty;
	void* StructData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FStructProperty* StructProp = CastField<FStructProperty>(StructProperty);
	const FESQLQueryResult Result = (!StructProp || !StructData || !StructProp->Struct)
		? FESQLQueryResult::Failure(TEXT("Populate SQL Row From Result requires a valid output struct pin"))
		: P_THIS->PopulateQueryResultIntoStruct(TableAsset, QueryResult, StructData, StructProp->Struct);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLSubsystem::execPopulateSQLRowsFromResult)
{
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_STRUCT(FESQLQueryResult, QueryResult);

	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	FProperty* ArrayProperty = Stack.MostRecentProperty;
	void* ArrayData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(ArrayProperty);
	const FESQLQueryResult Result = (!ArrayProp || !ArrayData)
		? FESQLQueryResult::Failure(TEXT("Populate SQL Rows From Result requires a valid output array pin"))
		: P_THIS->PopulateQueryResultIntoStructArray(TableAsset, QueryResult, ArrayData, ArrayProp);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLSubsystem::execLoadSQLRowById)
{
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_PROPERTY(FStrProperty, RowId);

	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FProperty* StructProperty = Stack.MostRecentProperty;
	void* StructData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FStructProperty* StructProp = CastField<FStructProperty>(StructProperty);
	const FESQLQueryResult Result = (!StructProp || !StructData || !StructProp->Struct)
		? FESQLQueryResult::Failure(TEXT("Load SQL Row By Id requires a valid output struct pin"))
		: P_THIS->LoadRowIntoStruct(P_THIS->GetGameInstance(), TableAsset, RowId, StructData, StructProp->Struct);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLSubsystem::execLoadSQLRow)
{
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_STRUCT(FESQLId, SqlId);

	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FProperty* StructProperty = Stack.MostRecentProperty;
	void* StructData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FStructProperty* StructProp = CastField<FStructProperty>(StructProperty);
	const FESQLQueryResult Result = (!StructProp || !StructData || !StructProp->Struct)
		? FESQLQueryResult::Failure(TEXT("Load SQL Row requires a valid output struct pin"))
		: P_THIS->LoadRowIntoStruct(P_THIS->GetGameInstance(), TableAsset, SqlId.Value, StructData, StructProp->Struct);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLSubsystem::execSaveSQLRow)
{
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_PROPERTY_REF(FStrProperty, OutResolvedRowId);
	P_GET_PROPERTY(FStrProperty, RowIdOverride);

	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FProperty* StructProperty = Stack.MostRecentProperty;
	void* StructData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FStructProperty* StructProp = CastField<FStructProperty>(StructProperty);
	const FESQLQueryResult Result = (!StructProp || !StructData || !StructProp->Struct)
		? FESQLQueryResult::Failure(TEXT("Save SQL Row requires a valid input struct pin"))
		: P_THIS->SaveRowFromStruct(P_THIS->GetGameInstance(), TableAsset, StructData, StructProp->Struct, &OutResolvedRowId, RowIdOverride);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLSubsystem::execLoadSQLRows)
{
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_PROPERTY(FIntProperty, MaxRows);

	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	FProperty* ArrayProperty = Stack.MostRecentProperty;
	void* ArrayData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(ArrayProperty);
	FESQLQuerySpec QuerySpec;
	QuerySpec.Limit = MaxRows > 0 ? MaxRows : 0;
	const FESQLQueryResult Result = (!ArrayProp || !ArrayData)
		? FESQLQueryResult::Failure(TEXT("Load SQL Rows requires a valid output array pin"))
		: P_THIS->LoadRowsIntoStructArray(P_THIS->GetGameInstance(), TableAsset, QuerySpec, ArrayData, ArrayProp);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLSubsystem::execFindSQLRows)
{
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_STRUCT(FESQLQuerySpec, QuerySpec);

	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	FProperty* ArrayProperty = Stack.MostRecentProperty;
	void* ArrayData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(ArrayProperty);
	const FESQLQueryResult Result = (!ArrayProp || !ArrayData)
		? FESQLQueryResult::Failure(TEXT("Find SQL Rows requires a valid output array pin"))
		: P_THIS->LoadRowsIntoStructArray(P_THIS->GetGameInstance(), TableAsset, QuerySpec, ArrayData, ArrayProp);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLSubsystem::execFindSQLRowsByField)
{
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_PROPERTY(FNameProperty, FieldName);
	P_GET_ENUM(EESQLFilterOperation, OperationValue);
	P_GET_STRUCT(FESQLBindingValue, Value);

	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	FProperty* ArrayProperty = Stack.MostRecentProperty;
	void* ArrayData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(ArrayProperty);
	const FESQLQueryResult Result = (!ArrayProp || !ArrayData)
		? FESQLQueryResult::Failure(TEXT("Find SQL Rows By Field requires a valid output array pin"))
		: P_THIS->LoadRowsIntoStructArray(P_THIS->GetGameInstance(), TableAsset, BuildSingleFilterQuerySpec(FieldName, static_cast<EESQLFilterOperation>(OperationValue), Value), ArrayData, ArrayProp);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLSubsystem::execFindFirstSQLRowByField)
{
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_PROPERTY(FNameProperty, FieldName);
	P_GET_ENUM(EESQLFilterOperation, OperationValue);
	P_GET_STRUCT(FESQLBindingValue, Value);

	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FProperty* StructProperty = Stack.MostRecentProperty;
	void* StructData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FStructProperty* StructProp = CastField<FStructProperty>(StructProperty);
	FESQLQuerySpec QuerySpec = BuildSingleFilterQuerySpec(FieldName, static_cast<EESQLFilterOperation>(OperationValue), Value);
	QuerySpec.Limit = 1;
	const FESQLQueryResult QueryResult = (!StructProp || !StructData || !StructProp->Struct)
		? FESQLQueryResult::Failure(TEXT("Find First SQL Row By Field requires a valid output struct pin"))
		: P_THIS->QueryTable(TableAsset, QuerySpec);
	const FESQLQueryResult Result = !QueryResult.bSuccess
		? QueryResult
		: (QueryResult.Rows.Num() == 0
			? FESQLQueryResult::Failure(TEXT("No rows matched query"))
			: P_THIS->PopulateQueryResultIntoStruct(TableAsset, QueryResult, StructData, StructProp->Struct));
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLSubsystem::execLoadSQLPage)
{
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_STRUCT(FESQLQuerySpec, BaseQuerySpec);
	P_GET_PROPERTY(FIntProperty, PageIndex);
	P_GET_PROPERTY(FIntProperty, PageSize);

	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	FProperty* ArrayProperty = Stack.MostRecentProperty;
	void* ArrayData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(ArrayProperty);
	FESQLQuerySpec QuerySpec;
	FString Error;
	const FESQLQueryResult Result = (!ArrayProp || !ArrayData)
		? FESQLQueryResult::Failure(TEXT("Load SQL Page requires a valid output array pin"))
		: (!BuildPagedQuerySpec(BaseQuerySpec, PageIndex, PageSize, QuerySpec, Error)
			? FESQLQueryResult::Failure(Error)
			: P_THIS->LoadRowsIntoStructArray(P_THIS->GetGameInstance(), TableAsset, QuerySpec, ArrayData, ArrayProp));
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLSubsystem::execLoadSQLPageByField)
{
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_PROPERTY(FNameProperty, FieldName);
	P_GET_ENUM(EESQLFilterOperation, OperationValue);
	P_GET_STRUCT(FESQLBindingValue, Value);
	P_GET_PROPERTY(FIntProperty, PageIndex);
	P_GET_PROPERTY(FIntProperty, PageSize);

	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	FProperty* ArrayProperty = Stack.MostRecentProperty;
	void* ArrayData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(ArrayProperty);
	FESQLQuerySpec QuerySpec;
	FString Error;
	const FESQLQueryResult Result = (!ArrayProp || !ArrayData)
		? FESQLQueryResult::Failure(TEXT("Load SQL Page By Field requires a valid output array pin"))
		: (!BuildPagedQuerySpec(BuildSingleFilterQuerySpec(FieldName, static_cast<EESQLFilterOperation>(OperationValue), Value), PageIndex, PageSize, QuerySpec, Error)
			? FESQLQueryResult::Failure(Error)
			: P_THIS->LoadRowsIntoStructArray(P_THIS->GetGameInstance(), TableAsset, QuerySpec, ArrayData, ArrayProp));
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

FESQLQueryResult UESQLSubsystem::BeginTransaction(const FString& DatabaseName, FESQLTransactionHandle& OutHandle)
{
	OutHandle.Reset();
	RefreshCachedNetMode();

	const TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context = FindDatabaseContext(DatabaseName);
	if (!Context.IsValid())
	{
		return MakeSubsystemFailure(EESQLErrorCode::NotFound, FString::Printf(TEXT("Database '%s' is not open"), *DatabaseName));
	}

	if (const TOptional<FESQLError> GateError = CheckScopeGate(Context->Scope, true, Context->LogicalName))
	{
		return FESQLQueryResult::Failure(GateError.GetValue());
	}

	return Context->RunWriterSync([](FESQLDatabaseContext& DatabaseContext)
	{
		FESQLWriterWorkResult WorkResult;
		const int64 TransactionId = DatabaseContext.NextTransactionId++;

		if (DatabaseContext.TransactionStack.Num() == 0)
		{
			const FESQLErrorResult BeginResult = DatabaseContext.WriterDatabase->BeginTransaction();
			if (!BeginResult.bSuccess)
			{
				WorkResult.Result = FESQLQueryResult::Failure(BeginResult.Error);
				return WorkResult;
			}
		}
		else
		{
			const FString SavepointName = FString::Printf(TEXT("sp_%lld"), static_cast<long long>(TransactionId));
			const FESQLQueryResult SavepointResult = DatabaseContext.WriterDatabase->Execute(
				FString::Printf(TEXT("SAVEPOINT \"%s\""), *SavepointName));
			if (!SavepointResult.bSuccess)
			{
				WorkResult.Result = SavepointResult;
				return WorkResult;
			}
		}

		const TSharedPtr<FESQLTransactionLifetime, ESPMode::ThreadSafe> Lifetime = MakeShared<FESQLTransactionLifetime, ESPMode::ThreadSafe>();
		FESQLActiveTransactionState& TransactionState = DatabaseContext.TransactionStack.AddDefaulted_GetRef();
		TransactionState.Id = TransactionId;
		TransactionState.SavepointName = DatabaseContext.TransactionStack.Num() > 1
			? FString::Printf(TEXT("sp_%lld"), static_cast<long long>(TransactionId))
			: FString();
		TransactionState.Lifetime = Lifetime;

		WorkResult.TransactionHandle.DatabaseName = FName(*DatabaseContext.LogicalName);
		WorkResult.TransactionHandle.Id = TransactionId;
		WorkResult.TransactionHandle.OwningThreadId = static_cast<int32>(DatabaseContext.WriterThreadId.Load());
		WorkResult.TransactionHandle.Lifetime = Lifetime;
		WorkResult.bHasTransactionHandle = true;
		WorkResult.Result = FESQLQueryResult::Success();
		return WorkResult;
	}, &OutHandle);
}

FESQLQueryResult UESQLSubsystem::CommitTransaction(FESQLTransactionHandle& Handle)
{
	if (!Handle.IsValid())
	{
		return MakeSubsystemFailure(EESQLErrorCode::InvalidArgument, TEXT("Transaction handle is invalid"));
	}

	const FString DatabaseName = Handle.DatabaseName.ToString();
	const int64 TransactionId = Handle.Id;
	const TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context = FindDatabaseContext(DatabaseName);
	if (!Context.IsValid())
	{
		return MakeSubsystemFailure(EESQLErrorCode::NotFound, FString::Printf(TEXT("Database '%s' is not open"), *DatabaseName));
	}

	const FESQLQueryResult Result = Context->RunWriterSync([TransactionId](FESQLDatabaseContext& DatabaseContext)
	{
		FESQLWriterWorkResult WorkResult;
		if (DatabaseContext.TransactionStack.Num() == 0 || DatabaseContext.TransactionStack.Last().Id != TransactionId)
		{
			WorkResult.Result = MakeSubsystemFailure(EESQLErrorCode::TransactionFailed, TEXT("Transaction handle is not the active top-of-stack transaction"));
			return WorkResult;
		}

		const FESQLActiveTransactionState TransactionState = DatabaseContext.TransactionStack.Last();
		if (DatabaseContext.TransactionStack.Num() == 1)
		{
			WorkResult.Result = MakeSubsystemSuccessFromErrorResult(DatabaseContext.WriterDatabase->CommitTransaction());
		}
		else
		{
			WorkResult.Result = DatabaseContext.WriterDatabase->Execute(
				FString::Printf(TEXT("RELEASE SAVEPOINT \"%s\""), *TransactionState.SavepointName));
		}

		if (WorkResult.Result.bSuccess)
		{
			DatabaseContext.TransactionStack.Pop();
		}

		return WorkResult;
	});

	if (Result.bSuccess)
	{
		Handle.Reset();
	}

	return Result;
}

FESQLQueryResult UESQLSubsystem::RollbackTransaction(FESQLTransactionHandle& Handle)
{
	if (!Handle.IsValid())
	{
		return MakeSubsystemFailure(EESQLErrorCode::InvalidArgument, TEXT("Transaction handle is invalid"));
	}

	const FString DatabaseName = Handle.DatabaseName.ToString();
	const int64 TransactionId = Handle.Id;
	const TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context = FindDatabaseContext(DatabaseName);
	if (!Context.IsValid())
	{
		return MakeSubsystemFailure(EESQLErrorCode::NotFound, FString::Printf(TEXT("Database '%s' is not open"), *DatabaseName));
	}

	const FESQLQueryResult Result = Context->RunWriterSync([TransactionId](FESQLDatabaseContext& DatabaseContext)
	{
		FESQLWriterWorkResult WorkResult;
		if (DatabaseContext.TransactionStack.Num() == 0 || DatabaseContext.TransactionStack.Last().Id != TransactionId)
		{
			WorkResult.Result = MakeSubsystemFailure(EESQLErrorCode::TransactionFailed, TEXT("Transaction handle is not the active top-of-stack transaction"));
			return WorkResult;
		}

		if (DatabaseContext.RollbackTopTransaction())
		{
			WorkResult.Result = FESQLQueryResult::Success();
		}
		else
		{
			WorkResult.Result = MakeSubsystemFailure(EESQLErrorCode::TransactionFailed, TEXT("Failed to rollback transaction"));
		}

		return WorkResult;
	});

	if (Result.bSuccess)
	{
		Handle.Reset();
	}

	return Result;
}

FESQLQueryResult UESQLSubsystem::ApplyMigrations(const FString& DatabaseName, const FESQLMigrationSet& MigrationSet)
{
	RefreshCachedNetMode();
	const TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context = FindDatabaseContext(DatabaseName);
	if (!Context.IsValid())
	{
		return MakeSubsystemFailure(EESQLErrorCode::NotFound, FString::Printf(TEXT("Database '%s' is not open"), *DatabaseName));
	}

	if (const TOptional<FESQLError> GateError = CheckScopeGate(Context->Scope, true, Context->LogicalName))
	{
		return FESQLQueryResult::Failure(GateError.GetValue());
	}

	if (MigrationSet.IsEmpty())
	{
		return FESQLQueryResult::Success();
	}

	TArray<FESQLMigrationStep> Steps = MigrationSet.Steps;
	Steps.Sort([](const FESQLMigrationStep& A, const FESQLMigrationStep& B)
	{
		return A.Sequence < B.Sequence;
	});

	return Context->RunWriterSync([Steps = MoveTemp(Steps)](FESQLDatabaseContext& DatabaseContext)
	{
		FESQLWriterWorkResult WorkResult;
		FESQLDatabase& Database = *DatabaseContext.WriterDatabase;

		FESQLQueryResult Result = Database.Execute(
			TEXT("CREATE TABLE IF NOT EXISTS \"_esql_schema_version\" (\"Sequence\" INTEGER NOT NULL, \"Migration\" TEXT NOT NULL)"));
		if (!Result.bSuccess)
		{
			WorkResult.Result = Result;
			return WorkResult;
		}

		Result = Database.Execute(TEXT("SELECT MAX(\"Sequence\") AS Sequence FROM \"_esql_schema_version\""));
		if (!Result.bSuccess)
		{
			WorkResult.Result = Result;
			return WorkResult;
		}

		int64 CurrentSequence = 0;
		if (const FESQLRow* Row = Result.GetFirstRow())
		{
			Row->TryGetInt64(TEXT("Sequence"), CurrentSequence);
		}

		const FESQLErrorResult BeginResult = Database.BeginTransaction();
		if (!BeginResult.bSuccess)
		{
			WorkResult.Result = FESQLQueryResult::Failure(BeginResult.Error);
			return WorkResult;
		}

		for (const FESQLMigrationStep& Step : Steps)
		{
			if (Step.Sequence <= CurrentSequence)
			{
				continue;
			}

			if (Step.Sequence <= 0)
			{
				Database.RollbackTransaction();
				WorkResult.Result = MakeSubsystemFailure(EESQLErrorCode::MigrationFailed, TEXT("Migration sequences must be positive"));
				return WorkResult;
			}

			for (const FString& Statement : ParseMigrationStatements(Step.Sql))
			{
				Result = Database.Execute(Statement);
				if (!Result.bSuccess)
				{
					Database.RollbackTransaction();
					WorkResult.Result = Result;
					WorkResult.Result.Error.Code = EESQLErrorCode::MigrationFailed;
					return WorkResult;
				}
			}

			Result = Database.Execute(
				TEXT("INSERT INTO \"_esql_schema_version\" (\"Sequence\", \"Migration\") VALUES (?1, ?2)"),
				{
					FESQLBindingValue::FromInteger(Step.Sequence),
					FESQLBindingValue::FromText(Step.Name.IsEmpty() ? FString::Printf(TEXT("migration_%d"), Step.Sequence) : Step.Name)
				});
			if (!Result.bSuccess)
			{
				Database.RollbackTransaction();
				WorkResult.Result = Result;
				WorkResult.Result.Error.Code = EESQLErrorCode::MigrationFailed;
				return WorkResult;
			}
		}

		const FESQLErrorResult CommitResult = Database.CommitTransaction();
		WorkResult.Result = CommitResult.bSuccess
			? FESQLQueryResult::Success()
			: FESQLQueryResult::Failure(CommitResult.Error);
		return WorkResult;
	});
}

FESQLQueryResult UESQLSubsystem::BackupTo(const FString& DatabaseName, const FString& DestAbsolutePath)
{
	RefreshCachedNetMode();
	const TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context = FindDatabaseContext(DatabaseName);
	if (!Context.IsValid())
	{
		return MakeSubsystemFailure(EESQLErrorCode::NotFound, FString::Printf(TEXT("Database '%s' is not open"), *DatabaseName));
	}

	if (const TOptional<FESQLError> GateError = CheckScopeGate(Context->Scope, false, Context->LogicalName))
	{
		return FESQLQueryResult::Failure(GateError.GetValue());
	}

	return Context->RunWriterSync([DestAbsolutePath](FESQLDatabaseContext& DatabaseContext)
	{
		FESQLWriterWorkResult WorkResult;
		WorkResult.Result = MakeSubsystemSuccessFromErrorResult(DatabaseContext.WriterDatabase->BackupToFile(DestAbsolutePath));
		return WorkResult;
	});
}

FESQLQueryResult UESQLSubsystem::SaveSnapshot(
	const FString& DatabaseName,
	const FString& SlotName,
	const FString& DisplayName)
{
	const FString SnapshotPath = ResolveSnapshotPath(DatabaseName, SlotName);
	const FESQLQueryResult BackupResult = BackupTo(DatabaseName, SnapshotPath);
	if (!BackupResult.bSuccess)
	{
		return BackupResult;
	}

	TSharedPtr<FJsonObject> MetaJson = MakeShared<FJsonObject>();
	MetaJson->SetStringField(TEXT("SlotName"), SlotName);
	MetaJson->SetStringField(TEXT("DisplayName"), DisplayName.IsEmpty() ? SlotName : DisplayName);
	MetaJson->SetStringField(TEXT("Timestamp"), FDateTime::UtcNow().ToIso8601());
	MetaJson->SetStringField(TEXT("DatabaseName"), DatabaseName);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	MetaJson->SetNumberField(TEXT("FileSizeBytes"), PlatformFile.FileSize(*SnapshotPath));

	FString MetaString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MetaString);
	FJsonSerializer::Serialize(MetaJson.ToSharedRef(), Writer);
	FFileHelper::SaveStringToFile(MetaString, *ResolveSnapshotMetaPath(DatabaseName, SlotName));

	return FESQLQueryResult::Success();
}

FESQLQueryResult UESQLSubsystem::LoadSnapshot(const FString& DatabaseName, const FString& SlotName)
{
	const TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context = FindDatabaseContext(DatabaseName);
	if (!Context.IsValid())
	{
		return MakeSubsystemFailure(EESQLErrorCode::NotFound, FString::Printf(TEXT("Database '%s' is not open"), *DatabaseName));
	}

	const FString SnapshotPath = ResolveSnapshotPath(DatabaseName, SlotName);
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*SnapshotPath))
	{
		return MakeSubsystemFailure(EESQLErrorCode::NotFound, FString::Printf(TEXT("Snapshot '%s' does not exist"), *SlotName));
	}

	return Context->RunWriterSync([SnapshotPath](FESQLDatabaseContext& DatabaseContext)
	{
		FESQLWriterWorkResult WorkResult;
		WorkResult.Result = MakeSubsystemSuccessFromErrorResult(DatabaseContext.WriterDatabase->RestoreFromFile(SnapshotPath));
		return WorkResult;
	});
}

FESQLQueryResult UESQLSubsystem::DeleteSnapshot(const FString& DatabaseName, const FString& SlotName)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.DeleteFile(*ResolveSnapshotPath(DatabaseName, SlotName));
	PlatformFile.DeleteFile(*ResolveSnapshotMetaPath(DatabaseName, SlotName));
	return FESQLQueryResult::Success();
}

TArray<FESQLSnapshotInfo> UESQLSubsystem::GetAllSnapshots(const FString& DatabaseName)
{
	TArray<FESQLSnapshotInfo> Snapshots;
	const FString SnapshotDir = FPaths::Combine(UESQLSettings::ResolveDatabaseDirectoryPath(), TEXT("Snapshots"), DatabaseName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*SnapshotDir))
	{
		return Snapshots;
	}

	TArray<FString> MetaFiles;
	PlatformFile.FindFilesRecursively(MetaFiles, *SnapshotDir, TEXT(".meta.json"));

	for (const FString& MetaPath : MetaFiles)
	{
		FString MetaString;
		if (!FFileHelper::LoadFileToString(MetaString, *MetaPath))
		{
			continue;
		}

		TSharedPtr<FJsonObject> MetaJson;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MetaString);
		if (!FJsonSerializer::Deserialize(Reader, MetaJson) || !MetaJson.IsValid())
		{
			continue;
		}

		FESQLSnapshotInfo Info;
		Info.SlotName = MetaJson->GetStringField(TEXT("SlotName"));
		Info.DisplayName = MetaJson->GetStringField(TEXT("DisplayName"));
		Info.FileSizeBytes = static_cast<int64>(MetaJson->GetNumberField(TEXT("FileSizeBytes")));
		FDateTime::ParseIso8601(*MetaJson->GetStringField(TEXT("Timestamp")), Info.Timestamp);
		Info.FilePath = ResolveSnapshotPath(DatabaseName, Info.SlotName);
		Snapshots.Add(MoveTemp(Info));
	}

	return Snapshots;
}

bool UESQLSubsystem::DoesSnapshotExist(const FString& DatabaseName, const FString& SlotName)
{
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*ResolveSnapshotPath(DatabaseName, SlotName));
}

void UESQLSubsystem::RefreshCachedNetMode()
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UWorld* World = GameInstance->GetWorld())
		{
			CachedNetMode = World->GetNetMode();
		}
	}
}

FString UESQLSubsystem::NormalizeLogicalDatabaseName(const FString& DatabaseName, EESQLDatabaseScope& InOutScope) const
{
	FString NormalizedName = DatabaseName.TrimStartAndEnd();

	auto ConsumePrefix = [&NormalizedName, &InOutScope](const TCHAR* Prefix, const EESQLDatabaseScope ScopeValue)
	{
		if (NormalizedName.StartsWith(Prefix, ESearchCase::IgnoreCase))
		{
			NormalizedName.RightChopInline(FCString::Strlen(Prefix), EAllowShrinking::No);
			NormalizedName.TrimStartAndEndInline();
			InOutScope = ScopeValue;
		}
	};

	ConsumePrefix(TEXT("local:"), EESQLDatabaseScope::Local);
	ConsumePrefix(TEXT("shared:"), EESQLDatabaseScope::Shared);
	ConsumePrefix(TEXT("readonly:"), EESQLDatabaseScope::Readonly);

	return NormalizedName;
}

TOptional<FESQLError> UESQLSubsystem::CheckScopeGate(const EESQLDatabaseScope Scope, const bool bWriteOperation, const FString& DatabaseName) const
{
	if (Scope == EESQLDatabaseScope::Shared && CachedNetMode == NM_Client)
	{
		return MakeSubsystemError(
			EESQLErrorCode::AuthorityViolation,
			FString::Printf(TEXT("Shared database '%s' is authority-only and cannot be opened or queried on clients"), *DatabaseName));
	}

	if (Scope == EESQLDatabaseScope::Readonly && bWriteOperation)
	{
		return MakeSubsystemError(
			EESQLErrorCode::ReadonlyViolation,
			FString::Printf(TEXT("Readonly database '%s' does not permit write operations"), *DatabaseName));
	}

	return TOptional<FESQLError>();
}

FESQLQueryResult UESQLSubsystem::ResolveDatabaseOpenPath(
	const FString& DatabaseName,
	EESQLDatabaseScope Scope,
	const EESQLDatabasePersistence Persistence,
	const FString& FileName,
	FString& OutResolvedDatabaseName,
	FString& OutResolvedPath,
	bool& OutReadOnly) const
{
	OutReadOnly = false;
	OutResolvedPath.Reset();
	OutResolvedDatabaseName = NormalizeLogicalDatabaseName(DatabaseName, Scope);

	if (OutResolvedDatabaseName.IsEmpty())
	{
		return MakeSubsystemFailure(EESQLErrorCode::InvalidArgument, TEXT("DatabaseName cannot be empty"));
	}

	if (Persistence == EESQLDatabasePersistence::Session)
	{
		if (Scope == EESQLDatabaseScope::Readonly)
		{
			return MakeSubsystemFailure(EESQLErrorCode::InvalidArgument, TEXT("Readonly databases cannot use session persistence"));
		}

		OutReadOnly = false;
		return FESQLQueryResult::Success();
	}

	FString ActualFileName = FileName.TrimStartAndEnd();
	if (ActualFileName.IsEmpty())
	{
		ActualFileName = OutResolvedDatabaseName + TEXT(".db");
	}

	if (!FESQLUsqliteSerializer::IsProjectRootPath(ActualFileName) && !ActualFileName.EndsWith(TEXT(".db"), ESearchCase::IgnoreCase))
	{
		ActualFileName += TEXT(".db");
	}

	if (FESQLUsqliteSerializer::IsProjectRootPath(ActualFileName))
	{
		if (Scope == EESQLDatabaseScope::Readonly)
		{
			return MakeSubsystemFailure(EESQLErrorCode::InvalidArgument, TEXT("Readonly databases cannot open .usqlite project roots directly"));
		}

#if WITH_EDITOR
		FString ProjectRoot;
		if (FPaths::IsRelative(ActualFileName) && !ActualFileName.Contains(TEXT("/")) && !ActualFileName.Contains(TEXT("\\")))
		{
			ProjectRoot = FESQLUsqliteSerializer::GetProjectRootForDatabasePath(
				FESQLPathResolver::ResolveDatabasePath(FPaths::GetBaseFilename(ActualFileName) + TEXT(".db"), Scope));
		}
		else
		{
			ProjectRoot = FESQLPathResolver::ResolveAbsolutePath(ActualFileName);
		}

		FString Error;
		if (!FESQLUsqliteBuilder::BuildProjectRoot(ProjectRoot, FString(), OutResolvedPath, Error))
		{
			return MakeSubsystemFailure(EESQLErrorCode::MigrationFailed, Error);
		}

		return FESQLQueryResult::Success();
#else
		return MakeSubsystemFailure(EESQLErrorCode::InvalidState, TEXT("Opening .usqlite project roots is only supported in editor builds"));
#endif
	}

	if (Scope == EESQLDatabaseScope::Readonly)
	{
		const FESQLResolvedPath ResolvedInfo = FESQLPathResolver::ResolveDatabasePathInfo(ActualFileName, Scope);
		if (!ResolvedInfo.IsValid())
		{
			return MakeSubsystemFailure(
				EESQLErrorCode::NotFound,
				FString::Printf(TEXT("Readonly database '%s' could not be resolved from packaged content"), *OutResolvedDatabaseName));
		}

		OutResolvedPath = ResolvedInfo.AbsolutePath;
		OutReadOnly = true;
		return FESQLQueryResult::Success();
	}

	OutResolvedPath = FESQLPathResolver::ResolveDatabasePath(ActualFileName, Scope);

#if WITH_EDITOR
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString ProjectRoot = FESQLUsqliteSerializer::GetProjectRootForDatabasePath(OutResolvedPath);
	const FString ProjectManifestPath = FPaths::Combine(ProjectRoot, TEXT("project.json"));
	if (PlatformFile.DirectoryExists(*ProjectRoot) && PlatformFile.FileExists(*ProjectManifestPath))
	{
		FString Error;
		FString BuiltPath;
		if (!FESQLUsqliteBuilder::BuildProjectRoot(ProjectRoot, FString(), BuiltPath, Error))
		{
			return MakeSubsystemFailure(EESQLErrorCode::MigrationFailed, Error);
		}

		OutResolvedPath = BuiltPath;
	}
#endif

	return FESQLQueryResult::Success();
}

TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> UESQLSubsystem::FindDatabaseContext(const FString& DatabaseName) const
{
	EESQLDatabaseScope DummyScope = EESQLDatabaseScope::Local;
	const FString ResolvedName = NormalizeLogicalDatabaseName(DatabaseName, DummyScope);

	FScopeLock ContextLock(&DatabaseContextsCS);
	if (const TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe>* Found = DatabaseContexts.Find(ResolvedName))
	{
		return *Found;
	}

	return nullptr;
}

TSharedPtr<FESQLAsyncTicketState, ESPMode::ThreadSafe> UESQLSubsystem::RegisterTicketState(const FString& DatabaseName)
{
	TSharedRef<FESQLAsyncTicketState, ESPMode::ThreadSafe> TicketState = MakeShared<FESQLAsyncTicketState, ESPMode::ThreadSafe>();
	TicketState->Ticket.Id = FGuid::NewGuid();
	TicketState->Ticket.DatabaseName = FName(*DatabaseName);

	FScopeLock TicketLock(&TicketsCS);
	TicketStates.Add(TicketState->Ticket.Id, TicketState);
	return TicketState;
}

void UESQLSubsystem::RemoveTicketState(const FGuid& TicketId)
{
	if (!TicketId.IsValid())
	{
		return;
	}

	FScopeLock TicketLock(&TicketsCS);
	TicketStates.Remove(TicketId);
}

void UESQLSubsystem::FinalizeAsyncTicket(
	const FGuid& TicketId,
	const FOnESQLQueryCompleteCallback& Callback,
	const FESQLQueryResult& Result)
{
	TWeakObjectPtr<UESQLSubsystem> WeakThis(this);
	AsyncTask(ENamedThreads::GameThread, [WeakThis, TicketId, Callback, Result]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->RemoveTicketState(TicketId);
		}

		Callback.ExecuteIfBound(Result);

		if (WeakThis.IsValid())
		{
			if (Result.bSuccess)
			{
				WeakThis->OnQueryComplete.Broadcast(Result);
			}
			else
			{
				WeakThis->OnQueryError.Broadcast(Result);
			}
		}
	});
}

FESQLQueryResult UESQLSubsystem::RunRawRead(const FString& DatabaseName, const FString& SQL, const TArray<FESQLBindingValue>& Bindings)
{
	RefreshCachedNetMode();
	const TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context = FindDatabaseContext(DatabaseName);
	if (!Context.IsValid())
	{
		return MakeSubsystemFailure(EESQLErrorCode::NotFound, FString::Printf(TEXT("Database '%s' is not open"), *DatabaseName));
	}

	if (const TOptional<FESQLError> GateError = CheckScopeGate(Context->Scope, false, Context->LogicalName))
	{
		return FESQLQueryResult::Failure(GateError.GetValue());
	}

	if (Context->Persistence == EESQLDatabasePersistence::Session || Context->ResolvedPath.IsEmpty())
	{
		return Context->RunWriterSync([SQL, Bindings](FESQLDatabaseContext& DatabaseContext)
		{
			FESQLWriterWorkResult WorkResult;
			WorkResult.Result = ExecuteDatabaseSql(*DatabaseContext.WriterDatabase, SQL, Bindings);
			return WorkResult;
		});
	}

	FESQLError ReaderError;
	FESQLReaderLease ReaderLease = Context->AcquireReaderLease(ReaderError);
	if (!ReaderLease)
	{
		return MakeSubsystemFailure(ReaderError);
	}

	return ExecuteDatabaseSql(*ReaderLease.Database, SQL, Bindings);
}

FESQLQueryResult UESQLSubsystem::RunRawWrite(const FString& DatabaseName, const FString& SQL, const TArray<FESQLBindingValue>& Bindings)
{
	RefreshCachedNetMode();
	const TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> Context = FindDatabaseContext(DatabaseName);
	if (!Context.IsValid())
	{
		return MakeSubsystemFailure(EESQLErrorCode::NotFound, FString::Printf(TEXT("Database '%s' is not open"), *DatabaseName));
	}

	if (const TOptional<FESQLError> GateError = CheckScopeGate(Context->Scope, true, Context->LogicalName))
	{
		return FESQLQueryResult::Failure(GateError.GetValue());
	}

	return Context->RunWriterSync([SQL, Bindings](FESQLDatabaseContext& DatabaseContext)
	{
		FESQLWriterWorkResult WorkResult;
		WorkResult.Result = ExecuteDatabaseSql(*DatabaseContext.WriterDatabase, SQL, Bindings);
		return WorkResult;
	});
}

FESQLQueryResult UESQLSubsystem::EnsureTableContext(
	UObject* /*WorldContextObject*/,
	UESQLTableAsset* TableAsset,
	const bool bNeedsWrite,
	TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe>& OutContext)
{
	OutContext.Reset();

	if (!TableAsset)
	{
		return MakeSubsystemFailure(EESQLErrorCode::InvalidArgument, TEXT("TableAsset is null"));
	}

	const FESQLTableSchema Schema = TableAsset->GetSchemaDescriptor();
	if (!Schema.IsValid())
	{
		return MakeSubsystemFailure(
			EESQLErrorCode::InvalidArgument,
			FString::Printf(TEXT("Table asset '%s' has incomplete schema metadata"), *GetNameSafe(TableAsset)));
	}

	TArray<FESQLStructValidator::FFieldResult> ValidationResults;
	if (!TableAsset->ValidateStruct(ValidationResults))
	{
		const FESQLStructValidator::FFieldResult* InvalidField = ValidationResults.FindByPredicate([](const FESQLStructValidator::FFieldResult& Field)
		{
			return !Field.bIsValid;
		});

		return MakeSubsystemFailure(
			EESQLErrorCode::InvalidArgument,
			InvalidField
				? FString::Printf(TEXT("Table asset '%s' has unsupported field '%s': %s"), *GetNameSafe(TableAsset), *InvalidField->FieldName, *InvalidField->ErrorReason)
				: FString::Printf(TEXT("Table asset '%s' has an invalid row struct"), *GetNameSafe(TableAsset)));
	}

	const FESQLQueryResult OpenResult = OpenDatabase(Schema.DatabaseName, Schema.Scope, Schema.Persistence);
	if (!OpenResult.bSuccess)
	{
		return OpenResult;
	}

	OutContext = FindDatabaseContext(Schema.DatabaseName);
	if (!OutContext.IsValid())
	{
		return MakeSubsystemFailure(EESQLErrorCode::NotFound, TEXT("Failed to resolve database context for table asset"));
	}

	if (const TOptional<FESQLError> GateError = CheckScopeGate(OutContext->Scope, bNeedsWrite, OutContext->LogicalName))
	{
		return FESQLQueryResult::Failure(GateError.GetValue());
	}

	if (!OutContext->bReadOnly)
	{
		const FESQLQueryResult PrepareResult = EnsureTablePrepared(TableAsset, OutContext);
		if (!PrepareResult.bSuccess)
		{
			return PrepareResult;
		}
	}

	return FESQLQueryResult::Success();
}

FESQLQueryResult UESQLSubsystem::EnsureTablePrepared(
	UESQLTableAsset* TableAsset,
	const TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe>& Context)
{
	if (!TableAsset || !Context.IsValid())
	{
		return MakeSubsystemFailure(EESQLErrorCode::InvalidArgument, TEXT("Table preparation requires a valid table asset and database context"));
	}

	const FString PreparedKey = MakePreparedTableKey(TableAsset);
	if (Context->IsTablePrepared(PreparedKey))
	{
		return FESQLQueryResult::Success();
	}

	return Context->RunWriterSync([WeakTableAsset = TWeakObjectPtr<UESQLTableAsset>(TableAsset), PreparedKey](FESQLDatabaseContext& DatabaseContext)
	{
		FESQLWriterWorkResult WorkResult;
		UESQLTableAsset* StrongTableAsset = WeakTableAsset.Get();
		if (!StrongTableAsset)
		{
			WorkResult.Result = MakeSubsystemFailure(EESQLErrorCode::InvalidState, TEXT("SQL Table Asset is no longer valid"));
			return WorkResult;
		}

		FString Error;
		if (!StrongTableAsset->EnsureTableExists(DatabaseContext.WriterDatabase, Error))
		{
			WorkResult.Result = MakeSubsystemFailure(EESQLErrorCode::MigrationFailed, Error);
			return WorkResult;
		}

		DatabaseContext.MarkTablePrepared(PreparedKey);
		WorkResult.Result = FESQLQueryResult::Success();
		return WorkResult;
	});
}

FString UESQLSubsystem::ResolveSnapshotPath(const FString& DatabaseName, const FString& SlotName) const
{
	return FPaths::Combine(UESQLSettings::ResolveDatabaseDirectoryPath(), TEXT("Snapshots"), DatabaseName, SlotName + TEXT(".db"));
}

FString UESQLSubsystem::ResolveSnapshotMetaPath(const FString& DatabaseName, const FString& SlotName) const
{
	return FPaths::Combine(UESQLSettings::ResolveDatabaseDirectoryPath(), TEXT("Snapshots"), DatabaseName, SlotName + TEXT(".meta.json"));
}

bool UESQLSubsystem::IsLikelyReadOnlySql(const FString& SQL)
{
	const FString TrimmedSql = SQL.TrimStartAndEnd();
	if (TrimmedSql.IsEmpty())
	{
		return false;
	}

	const FString UpperSql = TrimmedSql.Left(16).ToUpper();
	return UpperSql.StartsWith(TEXT("SELECT"))
		|| UpperSql.StartsWith(TEXT("PRAGMA"))
		|| UpperSql.StartsWith(TEXT("WITH"))
		|| UpperSql.StartsWith(TEXT("EXPLAIN"))
		|| UpperSql.StartsWith(TEXT("VALUES"));
}