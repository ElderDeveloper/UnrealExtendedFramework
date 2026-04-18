// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESQLTypes.h"

// Forward declaration — avoid including sqlite3.h in public header
struct sqlite3_stmt;

/**
 * RAII wrapper around a compiled sqlite3_stmt* handle.
 * Bind parameters, step through result rows, read column values.
 *
 * NOT a UObject — this is a pure C++ utility class used internally
 * by FESQLDatabase. Not exposed to Blueprints.
 *
 * Lifetime: owned by FESQLDatabase (via TSharedPtr in the statement cache)
 * or created on-the-fly for one-shot queries.
 */
class UNREALEXTENDEDSQL_API FESQLStatement
{
public:

	~FESQLStatement();

	// Non-copyable, movable
	FESQLStatement(const FESQLStatement&) = delete;
	FESQLStatement& operator=(const FESQLStatement&) = delete;
	FESQLStatement(FESQLStatement&& Other) noexcept;
	FESQLStatement& operator=(FESQLStatement&& Other) noexcept;


	// ── Binding (1-indexed, matching SQLite ?1, ?2, etc.) ────────────────

	bool BindInt(int32 Index, int64 Value);
	bool BindFloat(int32 Index, double Value);
	bool BindText(int32 Index, const FString& Value);
	bool BindBlob(int32 Index, const TArray<uint8>& Data);
	bool BindNull(int32 Index);

	/** Bind all values from a string array as TEXT.
	    Bindings[0] → ?1, Bindings[1] → ?2, etc. */
	bool BindStringArray(const TArray<FString>& Bindings);

	/** Clear all bindings. */
	bool ClearBindings();


	// ── Execution ────────────────────────────────────────────────────────

	/** Step one row. Returns true while there are rows to read (SQLITE_ROW).
	    Returns false when done (SQLITE_DONE) or on error. */
	bool Step();

	/** True if the last Step() returned SQLITE_DONE. */
	bool IsDone() const;

	/** Reset the statement for reuse with new bindings. */
	void Reset();


	// ── Column Readers (0-indexed) ───────────────────────────────────────

	int64 GetColumnInt(int32 Index) const;
	double GetColumnFloat(int32 Index) const;
	FString GetColumnText(int32 Index) const;
	TArray<uint8> GetColumnBlob(int32 Index) const;
	EESQLColumnType GetColumnType(int32 Index) const;
	int32 GetColumnCount() const;
	FString GetColumnName(int32 Index) const;


	// ── State ────────────────────────────────────────────────────────────

	/** Returns true if this statement has a valid compiled handle. */
	bool IsValid() const;

	/** Returns the original SQL string. */
	FString GetSQL() const;

private:

	friend class FESQLDatabase;

	/** Private constructor — only FESQLDatabase can create statements. */
	FESQLStatement();

	sqlite3_stmt* StmtHandle = nullptr;
	bool bDone = false;
};
