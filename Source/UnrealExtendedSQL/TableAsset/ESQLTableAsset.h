// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "UObject/Object.h"
#include "Shared/ESQLId.h"
#include "Shared/ESQLTypes.h"
#include "Core/ESQLDatabase.h"
#include "TableAsset/ESQLStructValidator.h"
#include "ESQLTableAsset.generated.h"

class FArrayProperty;


/**
 * Schema-owning typed table contract for ExtendedSQL.
 *
 * This asset owns table identity, schema sync, primary-key behavior,
 * label behavior, and editor-facing authored-data metadata.
 *
 * Runtime SQL routing lives on UESQLSubsystem. The asset exposes metadata,
 * schema description, and authoring helpers only.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDSQL_API UESQLTableAsset : public UObject
{
	GENERATED_BODY()
	friend class UESQLSubsystem;
#if WITH_EDITOR
	friend class FESQLTableEditorToolkit;
#endif

public:

	// ── Asset Metadata ───────────────────────────────────────────────────

	/** The struct that defines the row schema.
	    Set once during asset creation. The editor auto-migrates the
	    DB schema if the struct changes (add/remove columns). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SQL Table")
	const UScriptStruct* RowStruct = nullptr;

	/** The database this table lives in (logical name, same as OpenDatabase).
	    Defaults to "EditorData". Select from existing databases or type a new name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table", meta = (GetOptions = "GetDatabaseNameOptions"))
	FString DatabaseName = TEXT("EditorData");

	/** The SQL table name inside the database.
	    Defaults to the struct name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table")
	FString TableName;

	/** Which column serves as the PRIMARY KEY in SQLite.
	    This column is managed by the system — it does NOT need to exist
	    in the RowStruct. It is auto-added as the first column.
	    Defaults to "RowName" (TEXT, auto-generated if left empty on insert).
	    Query results always include this column alongside the struct fields. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table")
	FString PrimaryKeyColumn = TEXT("RowName");

	/** Default column to display as a human-readable label in FESQLId picker dropdowns.
	    Individual FESQLId properties can override this per property.
	    The picker shows "<DefaultLabelColumn value> (<PrimaryKeyColumn value>)".
	    If empty, only the PrimaryKeyColumn value is shown.
	    Example: Set to "display_name" so the picker shows "Iron Sword (weapon_42)". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table",
		meta = (DisplayName = "Default Label Column", GetOptions = "GetLabelColumnOptions"))
	FString DefaultLabelColumn;

	/** Database scope — controls file path and authority. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table")
	EESQLDatabaseScope Scope = EESQLDatabaseScope::Local;

	/** Persistence mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table")
	EESQLDatabasePersistence Persistence = EESQLDatabasePersistence::Persistent;


	/** Returns list of existing database names for the dropdown (GetOptions). */
	UFUNCTION()
	TArray<FString> GetDatabaseNameOptions() const;

	/** Returns picker column options derived from the primary key and row struct fields. */
	UFUNCTION()
	TArray<FString> GetLabelColumnOptions() const;

	/** Build the normalized schema descriptor consumed by subsystem runtime routing. */
	FESQLTableSchema GetSchemaDescriptor() const;


	// ── Editor Support (code only, no UI — Phase 8) ──────────────────────

	/** Returns column definitions derived from the row struct.
	    Includes PrimaryKeyColumn as first entry. */
	TArray<FESQLColumn> GetColumnDefinitions() const;

	/** Resolve an authored Blueprint field name to the actual SQL column name. */
	FString ResolveColumnName(const FString& ColumnName) const;

	/** Resolve the effective label column for runtime lookups. */
	FString GetEffectiveLabelColumn(const FString& LabelColumnOverride) const;

	/** Populate a single struct instance from a previously captured raw SQL result. C++ only. */
	FESQLQueryResult PopulateQueryResultIntoStruct(
		const FESQLQueryResult& QueryResult,
		void* OutStructData,
		const UScriptStruct* StructType) const;

	/** Populate an array of struct instances from a previously captured raw SQL result. C++ only. */
	FESQLQueryResult PopulateQueryResultIntoStructArray(
		const FESQLQueryResult& QueryResult,
		void* OutArrayData,
		const FArrayProperty* ArrayProperty) const;

	/** Validate the row struct for SQLite compatibility. */
	bool ValidateStruct(TArray<FESQLStructValidator::FFieldResult>& OutResults) const;

	/** Ensure the table exists in the given database (CREATE TABLE IF NOT EXISTS).
	    Called by authoring/runtime preparation paths before editing or querying rows. */
	bool EnsureTableExists(TSharedPtr<FESQLDatabase> InDatabase, FString& OutError);

	/** Export all rows to a CSV file.
	    @param InDatabase Optional database to use. If null, opens the table database on demand. */
	bool ExportToCSV(const FString& OutputFilePath, FString& OutError, TSharedPtr<FESQLDatabase> InDatabase = nullptr) const;

	/** Import rows from a CSV file (UPSERT logic).
	    @param InDatabase Optional database to use. If null, opens the table database on demand. */
	bool ImportFromCSV(const FString& InputFilePath, FString& OutError, TSharedPtr<FESQLDatabase> InDatabase = nullptr);

	// ── `.usqlite` Source Of Truth ───────────────────────────────────────

	/** Export the current database state to a `.usqlite/` project.
	    @param ProjectRoot Optional target directory. If empty, uses the sibling `<Database>.usqlite/` path.
	    @param InDatabase Optional database to use. If null, opens the table database on demand. */
	bool ExportToUSqlite(const FString& ProjectRoot, FString& OutError, TSharedPtr<FESQLDatabase> InDatabase = nullptr) const;

	/** Ensure the canonical `.db` file is rebuilt from the sibling `.usqlite/` project when present,
	    or create the initial `.usqlite/` project from the current `.db` when only the database exists. */
	bool SyncDatabaseAndProject(FString& OutError);

	/** Get the `.usqlite/` project path for this asset's database. */
	FString GetUSqliteProjectPath() const;

#if WITH_EDITOR
	/** Gather authoring-facing asset validation errors for the details panel and editor validators. */
	void GetValidationErrors(TArray<FText>& OutErrors) const;

	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	FESQLQueryResult ValidateTypedStructAccess(const UScriptStruct* StructType) const;
	FESQLQueryResult BuildStructColumnBindings(const void* StructData, const UScriptStruct* StructType, TMap<FString, FESQLBindingValue>& OutColumnValues) const;
	FESQLQueryResult PopulateStructFromRow(const FESQLRow& Row, void* OutStructData, const UScriptStruct* StructType) const;
	FESQLQueryResult PopulateStructArrayFromRows(const TArray<FESQLRow>& Rows, void* OutArrayData, const FArrayProperty* ArrayProperty) const;
	bool ResolveAuthoringDatabase(
		TSharedPtr<FESQLDatabase> InDatabase,
		TSharedPtr<FESQLDatabase>& OutDatabase,
		TSharedPtr<FESQLDatabase>& OutOwnedDatabase,
		FString& OutError) const;
	FESQLQueryResult BuildGetRowByIdSQL(const FString& RowId, FString& OutSQL, TArray<FString>& OutBindings) const;
	FESQLQueryResult BuildSaveRowSQL(const void* StructData, const UScriptStruct* StructType, FString& OutSQL, TArray<FESQLBindingValue>& OutBindings, FString& OutResolvedRowId, const FString& RowIdOverride) const;
	bool TryResolveQueryColumn(const FString& FieldName, FString& OutColumnName, FString* OutError = nullptr) const;
	FESQLQueryResult BuildQuerySQL(const FString& SelectClause, const FESQLQuerySpec& QuerySpec, bool bIncludeSorting, bool bIncludePaging, FString& OutSQL, TArray<FESQLBindingValue>& OutBindings) const;
	FESQLQueryResult BuildQueryRowsSQL(const FESQLQuerySpec& QuerySpec, FString& OutSQL, TArray<FESQLBindingValue>& OutBindings) const;

	/** Compare struct fields vs DB columns and apply migrations for authoring flows. */
	FESQLQueryResult SyncSchema(TSharedPtr<FESQLDatabase> Database);
};
