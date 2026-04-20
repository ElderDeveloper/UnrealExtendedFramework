// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESQLTableAsset.h"
#include "ESQLStructValidator.h"
#include "Private/USqlite/ESQLUsqliteSerializer.h"
#include "Shared/ESQLPropertySerializer.h"
#include "Shared/ESQLSettings.h"
#include "Private/ThirdParty/ESQLVendorSqlite.h"
#include "UnrealExtendedSQL.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

// ── Database Name Options (for dropdown in Details panel) ────────────────────

TArray<FString> UESQLTableAsset::GetDatabaseNameOptions() const
{
	TArray<FString> Options;

	// Always include the current value and common defaults
	Options.Add(TEXT("EditorData"));
	if (!DatabaseName.IsEmpty() && DatabaseName != TEXT("EditorData"))
	{
		Options.AddUnique(DatabaseName);
	}

	// Scan existing databases in the configured database directory.
	const FString DbDir = UESQLSettings::ResolveDatabaseDirectoryPath();
	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *FPaths::Combine(DbDir, TEXT("*.db")), true, false);

	for (const FString& File : FoundFiles)
	{
		// Strip the .db extension to get the logical name
		FString Name = FPaths::GetBaseFilename(File);
		Options.AddUnique(Name);
	}

	Options.Sort();
	return Options;
}

TArray<FString> UESQLTableAsset::GetLabelColumnOptions() const
{
	TArray<FString> Options;

	if (!PrimaryKeyColumn.IsEmpty())
	{
		Options.Add(PrimaryKeyColumn);
	}

	if (!RowStruct)
	{
		return Options;
	}

	for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
	{
		if (FESQLStructValidator::MapPropertyToSQLiteType(*It).IsEmpty())
		{
			continue;
		}

		Options.AddUnique((*It)->GetAuthoredName());
	}

	return Options;
}
FESQLTableSchema UESQLTableAsset::GetSchemaDescriptor() const
{
	FESQLTableSchema Schema;
	Schema.DatabaseName = DatabaseName;
	Schema.TableName = TableName.IsEmpty() && RowStruct ? RowStruct->GetName() : TableName;
	Schema.PrimaryKeyColumn = PrimaryKeyColumn;
	Schema.LabelColumn = GetEffectiveLabelColumn(TEXT(""));
	Schema.Scope = Scope;
	Schema.Persistence = Persistence;
	Schema.RowStruct = RowStruct;

	Schema.Columns.Add({ PrimaryKeyColumn, TEXT("TEXT"), true, false, FString() });

	if (RowStruct)
	{
		for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
		{
			FProperty* Property = *It;
			const FString SQLiteType = FESQLStructValidator::MapPropertyToSQLiteType(Property);
			if (SQLiteType.IsEmpty())
			{
				continue;
			}

			Schema.Columns.Add({ Property->GetName(), SQLiteType, false, true, FString() });
		}
	}

	return Schema;
}

FESQLQueryResult UESQLTableAsset::ValidateTypedStructAccess(const UScriptStruct* StructType) const
{
	if (!StructType)
	{
		return FESQLQueryResult::Failure(TEXT("StructType is null"));
	}

	if (!RowStruct)
	{
		return FESQLQueryResult::Failure(TEXT("RowStruct is not set on this SQL Table Asset"));
	}

	if (StructType != RowStruct)
	{
		return FESQLQueryResult::Failure(FString::Printf(
			TEXT("Struct type mismatch for table '%s': expected '%s' but received '%s'"),
			*GetName(),
			*RowStruct->GetName(),
			*StructType->GetName()));
	}

	return FESQLQueryResult::Success();
}

FESQLQueryResult UESQLTableAsset::BuildStructColumnBindings(
	const void* StructData,
	const UScriptStruct* StructType,
	TMap<FString, FESQLBindingValue>& OutColumnValues) const
{
	OutColumnValues.Reset();

	if (!StructData)
	{
		return FESQLQueryResult::Failure(TEXT("StructData is null"));
	}

	const FESQLQueryResult ValidationResult = ValidateTypedStructAccess(StructType);
	if (!ValidationResult.bSuccess)
	{
		return ValidationResult;
	}

	for (TFieldIterator<FProperty> It(StructType); It; ++It)
	{
		FProperty* Property = *It;
		FESQLBindingValue BindingValue;

		if (FESQLPropertySerializer::SerializePropertyToBindingValue(Property, StructData, BindingValue))
		{
			OutColumnValues.Add(Property->GetName(), MoveTemp(BindingValue));
		}
	}

	return FESQLQueryResult::Success();
}
FESQLQueryResult UESQLTableAsset::BuildGetRowByIdSQL(const FString& RowId, FString& OutSQL, TArray<FString>& OutBindings) const
{
	OutBindings.Reset();

	if (RowId.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("RowId cannot be empty"));
	}

	if (TableName.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("TableName is not configured"));
	}

	if (PrimaryKeyColumn.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("PrimaryKeyColumn is not configured"));
	}

	OutSQL = FString::Printf(
		TEXT("SELECT * FROM \"%s\" WHERE \"%s\"=?1 LIMIT 1"),
		*TableName,
		*PrimaryKeyColumn);
	OutBindings.Add(RowId);
	return FESQLQueryResult::Success();
}

FESQLQueryResult UESQLTableAsset::BuildSaveRowSQL(
	const void* StructData,
	const UScriptStruct* StructType,
	FString& OutSQL,
	TArray<FESQLBindingValue>& OutBindings,
	FString& OutResolvedRowId,
	const FString& RowIdOverride) const
{
	const FESQLQueryResult ValidationResult = ValidateTypedStructAccess(StructType);
	if (!ValidationResult.bSuccess)
	{
		return ValidationResult;
	}

	TMap<FString, FESQLBindingValue> ColumnValues;
	const FESQLQueryResult BindingsResult = BuildStructColumnBindings(StructData, StructType, ColumnValues);
	if (!BindingsResult.bSuccess)
	{
		return BindingsResult;
	}

	const FString ConflictColumn = ResolveColumnName(PrimaryKeyColumn);
	if (ConflictColumn.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("PrimaryKeyColumn is not configured"));
	}

	OutResolvedRowId = RowIdOverride;
	if (OutResolvedRowId.IsEmpty())
	{
		if (const FESQLBindingValue* ExistingPrimaryKey = ColumnValues.Find(ConflictColumn))
		{
			if (!ExistingPrimaryKey->bIsNull)
			{
				OutResolvedRowId = ExistingPrimaryKey->TextValue;
			}
		}
	}

	if (OutResolvedRowId.IsEmpty())
	{
		if (ConflictColumn == TEXT("RowName"))
		{
			OutResolvedRowId = FGuid::NewGuid().ToString();
		}
		else
		{
			return FESQLQueryResult::Failure(FString::Printf(
				TEXT("Cannot save row for table '%s': no value provided for primary key column '%s'"),
				*TableName,
				*ConflictColumn));
		}
	}

	ColumnValues.Add(ConflictColumn, FESQLBindingValue::FromText(OutResolvedRowId));

	FString ColumnList;
	FString PlaceholderList;
	FString UpdateList;
	OutBindings.Reset();
	int32 Index = 1;

	for (const auto& Pair : ColumnValues)
	{
		if (Index > 1)
		{
			ColumnList += TEXT(", ");
			PlaceholderList += TEXT(", ");
		}

		ColumnList += FString::Printf(TEXT("\"%s\""), *Pair.Key);
		PlaceholderList += FString::Printf(TEXT("?%d"), Index);

		if (Pair.Key != ConflictColumn)
		{
			if (!UpdateList.IsEmpty())
			{
				UpdateList += TEXT(", ");
			}

			UpdateList += FString::Printf(TEXT("\"%s\"=excluded.\"%s\""), *Pair.Key, *Pair.Key);
		}

		OutBindings.Add(Pair.Value);
		++Index;
	}

	if (UpdateList.IsEmpty())
	{
		OutSQL = FString::Printf(
			TEXT("INSERT INTO \"%s\" (%s) VALUES (%s) ON CONFLICT(\"%s\") DO NOTHING"),
			*TableName,
			*ColumnList,
			*PlaceholderList,
			*ConflictColumn);
	}
	else
	{
		OutSQL = FString::Printf(
			TEXT("INSERT INTO \"%s\" (%s) VALUES (%s) ON CONFLICT(\"%s\") DO UPDATE SET %s"),
			*TableName,
			*ColumnList,
			*PlaceholderList,
			*ConflictColumn,
			*UpdateList);
	}

	return FESQLQueryResult::Success();
}
FESQLQueryResult UESQLTableAsset::PopulateStructFromRow(
	const FESQLRow& Row,
	void* OutStructData,
	const UScriptStruct* StructType) const
{
	if (!OutStructData)
	{
		return FESQLQueryResult::Failure(TEXT("OutStructData is null"));
	}

	const FESQLQueryResult ValidationResult = ValidateTypedStructAccess(StructType);
	if (!ValidationResult.bSuccess)
	{
		return ValidationResult;
	}

	FStructOnScope LoadedRow(StructType);
	void* LoadedRowData = LoadedRow.GetStructMemory();
	if (!LoadedRowData)
	{
		return FESQLQueryResult::Failure(TEXT("Failed to allocate temporary struct memory"));
	}

	for (TFieldIterator<FProperty> It(StructType); It; ++It)
	{
		FProperty* Property = *It;
		const FString ColumnName = Property->GetName();
		const FString* ColumnValue = Row.FindValue(ColumnName);
		const bool bIsNull = Row.IsNull(ColumnName);

		if (!ColumnValue && !bIsNull)
		{
			continue;
		}

		const FString Value = ColumnValue ? *ColumnValue : FString();
		if (!FESQLPropertySerializer::DeserializePropertyFromString(Property, LoadedRowData, Value, bIsNull))
		{
			return FESQLQueryResult::Failure(FString::Printf(
				TEXT("Failed to deserialize column '%s' into struct '%s'"),
				*ColumnName,
				*StructType->GetName()));
		}
	}

	StructType->CopyScriptStruct(OutStructData, LoadedRowData);
	return FESQLQueryResult::Success();
}

FESQLQueryResult UESQLTableAsset::PopulateStructArrayFromRows(
	const TArray<FESQLRow>& Rows,
	void* OutArrayData,
	const FArrayProperty* ArrayProperty) const
{
	if (!OutArrayData)
	{
		return FESQLQueryResult::Failure(TEXT("OutArrayData is null"));
	}

	const FStructProperty* StructProperty = ArrayProperty ? CastField<FStructProperty>(ArrayProperty->Inner) : nullptr;
	if (!StructProperty || !StructProperty->Struct)
	{
		return FESQLQueryResult::Failure(TEXT("SQL row array operations require an array of structs"));
	}

	FScriptArrayHelper ArrayHelper(ArrayProperty, OutArrayData);
	ArrayHelper.Resize(0);
	ArrayHelper.Resize(Rows.Num());

	for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
	{
		const FESQLQueryResult PopulateResult = PopulateStructFromRow(Rows[RowIndex], ArrayHelper.GetRawPtr(RowIndex), StructProperty->Struct);
		if (!PopulateResult.bSuccess)
		{
			ArrayHelper.Resize(0);
			return PopulateResult;
		}
	}

	return FESQLQueryResult::Success();
}

bool UESQLTableAsset::TryResolveQueryColumn(const FString& FieldName, FString& OutColumnName, FString* OutError) const
{
	OutColumnName = ResolveColumnName(FieldName);
	if (OutColumnName.IsEmpty())
	{
		if (OutError)
		{
			*OutError = TEXT("Query field name cannot be empty");
		}

		return false;
	}

	if (OutColumnName == PrimaryKeyColumn)
	{
		if (OutError)
		{
			OutError->Reset();
		}

		return true;
	}

	if (!RowStruct)
	{
		if (OutError)
		{
			*OutError = TEXT("RowStruct is not set on this SQL Table Asset");
		}

		return false;
	}

	for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
	{
		if (const FProperty* Property = *It)
		{
			if (Property->GetName() == OutColumnName)
			{
				if (OutError)
				{
					OutError->Reset();
				}

				return true;
			}
		}
	}

	if (OutError)
	{
		*OutError = FString::Printf(
			TEXT("Unknown query field '%s' for table '%s'"),
			*FieldName,
			*TableName);
	}

	return false;
}

FESQLQueryResult UESQLTableAsset::BuildQuerySQL(
	const FString& SelectClause,
	const FESQLQuerySpec& QuerySpec,
	bool bIncludeSorting,
	bool bIncludePaging,
	FString& OutSQL,
	TArray<FESQLBindingValue>& OutBindings) const
{
	if (TableName.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("TableName is not configured"));
	}

	if (SelectClause.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("SelectClause cannot be empty"));
	}

	OutBindings.Reset();
	OutSQL = FString::Printf(TEXT("%s FROM \"%s\""), *SelectClause, *TableName);

	if (QuerySpec.Filters.Num() > 0)
	{
		OutSQL += TEXT(" WHERE ");

		bool bFirstFilter = true;
		for (const FESQLFieldFilter& Filter : QuerySpec.Filters)
		{
			FString ColumnName;
			FString ResolveError;
			if (!TryResolveQueryColumn(Filter.FieldName, ColumnName, &ResolveError))
			{
				return FESQLQueryResult::Failure(ResolveError);
			}

			if (!bFirstFilter)
			{
				OutSQL += TEXT(" AND ");
			}

			const int32 BindingStartIndex = OutBindings.Num() + 1;
			const FString QuotedColumn = FString::Printf(TEXT("\"%s\""), *ColumnName);

			if (Filter.Operation == EESQLFilterOperation::In)
			{
				if (Filter.Values.Num() == 0)
				{
					return FESQLQueryResult::Failure(FString::Printf(
						TEXT("IN filter for field '%s' requires at least one value"),
						*Filter.FieldName));
				}

				OutSQL += QuotedColumn + TEXT(" IN (");
				for (int32 ValueIndex = 0; ValueIndex < Filter.Values.Num(); ++ValueIndex)
				{
					if (ValueIndex > 0)
					{
						OutSQL += TEXT(", ");
					}

					OutSQL += FString::Printf(TEXT("?%d"), BindingStartIndex + ValueIndex);
					OutBindings.Add(Filter.Values[ValueIndex]);
				}
				OutSQL += TEXT(")");
			}
			else
			{
				if (Filter.Values.Num() != 1)
				{
					return FESQLQueryResult::Failure(FString::Printf(
						TEXT("Filter for field '%s' requires exactly one value"),
						*Filter.FieldName));
				}

				const FESQLBindingValue& BindingValue = Filter.Values[0];
				if (BindingValue.IsNull())
				{
					if (Filter.Operation == EESQLFilterOperation::Equal)
					{
						OutSQL += QuotedColumn + TEXT(" IS NULL");
					}
					else if (Filter.Operation == EESQLFilterOperation::NotEqual)
					{
						OutSQL += QuotedColumn + TEXT(" IS NOT NULL");
					}
					else
					{
						return FESQLQueryResult::Failure(FString::Printf(
							TEXT("Null filter values are only supported for Equal/NotEqual on field '%s'"),
							*Filter.FieldName));
					}
				}
				else
				{
					const TCHAR* OperatorToken = TEXT("=");
					switch (Filter.Operation)
					{
					case EESQLFilterOperation::Equal:
						OperatorToken = TEXT("=");
						break;
					case EESQLFilterOperation::NotEqual:
						OperatorToken = TEXT("<>");
						break;
					case EESQLFilterOperation::Less:
						OperatorToken = TEXT("<");
						break;
					case EESQLFilterOperation::LessOrEqual:
						OperatorToken = TEXT("<=");
						break;
					case EESQLFilterOperation::Greater:
						OperatorToken = TEXT(">");
						break;
					case EESQLFilterOperation::GreaterOrEqual:
						OperatorToken = TEXT(">=");
						break;
					case EESQLFilterOperation::Like:
						OperatorToken = TEXT("LIKE");
						break;
					case EESQLFilterOperation::In:
					default:
						return FESQLQueryResult::Failure(FString::Printf(
							TEXT("Unsupported single-value filter operation for field '%s'"),
							*Filter.FieldName));
					}

					OutSQL += FString::Printf(TEXT("%s %s ?%d"), *QuotedColumn, OperatorToken, BindingStartIndex);
					OutBindings.Add(BindingValue);
				}
			}

			bFirstFilter = false;
		}
	}

	if (bIncludeSorting && QuerySpec.SortRules.Num() > 0)
	{
		OutSQL += TEXT(" ORDER BY ");

		for (int32 SortIndex = 0; SortIndex < QuerySpec.SortRules.Num(); ++SortIndex)
		{
			const FESQLSortRule& SortRule = QuerySpec.SortRules[SortIndex];
			FString ColumnName;
			FString ResolveError;
			if (!TryResolveQueryColumn(SortRule.FieldName, ColumnName, &ResolveError))
			{
				return FESQLQueryResult::Failure(ResolveError);
			}

			if (SortIndex > 0)
			{
				OutSQL += TEXT(", ");
			}

			OutSQL += FString::Printf(
				TEXT("\"%s\" %s"),
				*ColumnName,
				SortRule.bAscending ? TEXT("ASC") : TEXT("DESC"));
		}
	}

	if (bIncludePaging)
	{
		if (QuerySpec.Limit > 0)
		{
			OutSQL += FString::Printf(TEXT(" LIMIT %d"), QuerySpec.Limit);
		}

		if (QuerySpec.Offset > 0)
		{
			if (QuerySpec.Limit <= 0)
			{
				OutSQL += TEXT(" LIMIT -1");
			}

			OutSQL += FString::Printf(TEXT(" OFFSET %d"), QuerySpec.Offset);
		}
	}

	return FESQLQueryResult::Success();
}

FESQLQueryResult UESQLTableAsset::BuildQueryRowsSQL(const FESQLQuerySpec& QuerySpec, FString& OutSQL, TArray<FESQLBindingValue>& OutBindings) const
{
	return BuildQuerySQL(TEXT("SELECT *"), QuerySpec, true, true, OutSQL, OutBindings);
}

FESQLQueryResult UESQLTableAsset::PopulateQueryResultIntoStruct(
	const FESQLQueryResult& QueryResult,
	void* OutStructData,
	const UScriptStruct* StructType) const
{
	const FESQLQueryResult ValidationResult = ValidateTypedStructAccess(StructType);
	if (!ValidationResult.bSuccess)
	{
		return ValidationResult;
	}

	if (!QueryResult.bSuccess)
	{
		return QueryResult;
	}

	const FESQLRow* Row = QueryResult.GetFirstRow();
	if (!Row)
	{
		return FESQLQueryResult::Failure(TEXT("Query result does not contain any rows"));
	}

	return PopulateStructFromRow(*Row, OutStructData, StructType);
}

FESQLQueryResult UESQLTableAsset::PopulateQueryResultIntoStructArray(
	const FESQLQueryResult& QueryResult,
	void* OutArrayData,
	const FArrayProperty* ArrayProperty) const
{
	if (!QueryResult.bSuccess)
	{
		return QueryResult;
	}

	return PopulateStructArrayFromRows(QueryResult.Rows, OutArrayData, ArrayProperty);
}

FString UESQLTableAsset::ResolveColumnName(const FString& ColumnName) const
{
	if (ColumnName.IsEmpty() || !RowStruct)
	{
		return ColumnName;
	}

	if (ColumnName == PrimaryKeyColumn)
	{
		return PrimaryKeyColumn;
	}

	for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property)
		{
			continue;
		}

		if (Property->GetName() == ColumnName || Property->GetAuthoredName() == ColumnName)
		{
			return Property->GetName();
		}
	}

	return ColumnName;
}

FString UESQLTableAsset::GetEffectiveLabelColumn(const FString& LabelColumnOverride) const
{
	const FString DesiredLabelColumn = !LabelColumnOverride.IsEmpty()
		? LabelColumnOverride
		: DefaultLabelColumn;

	if (DesiredLabelColumn.IsEmpty())
	{
		return PrimaryKeyColumn;
	}

	return ResolveColumnName(DesiredLabelColumn);
}


// ── Schema Migration ─────────────────────────────────────────────────────────

FESQLQueryResult UESQLTableAsset::SyncSchema(TSharedPtr<FESQLDatabase> Database)
{
	if (!Database || !Database->IsOpen())
	{
		return FESQLQueryResult::Failure(TEXT("Database not open for schema sync"));
	}

	if (!RowStruct)
	{
		return FESQLQueryResult::Failure(TEXT("RowStruct is null"));
	}

	// Get existing columns from the database
	FESQLQueryResult PragmaResult = Database->Execute(
		FString::Printf(TEXT("PRAGMA table_info(\"%s\")"), *TableName));

	if (!PragmaResult.bSuccess)
	{
		return PragmaResult;
	}

	// Build set of existing column names
	TSet<FString> ExistingColumns;
	for (const FESQLRow& Row : PragmaResult.Rows)
	{
		FString ColName;
		if (Row.TryGetString(TEXT("name"), ColName))
		{
			ExistingColumns.Add(ColName);
		}
	}

	// Build set of expected columns from struct
	TMap<FString, FString> ExpectedColumns;
	ExpectedColumns.Add(PrimaryKeyColumn, TEXT("TEXT"));  // PK always present

	for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
	{
		FString SQLType = FESQLStructValidator::MapPropertyToSQLiteType(*It);
		if (!SQLType.IsEmpty())
		{
			ExpectedColumns.Add((*It)->GetName(), SQLType);
		}
	}

	// ADD new columns
	for (const auto& Pair : ExpectedColumns)
	{
		if (!ExistingColumns.Contains(Pair.Key))
		{
			FString AlterSQL = FString::Printf(TEXT("ALTER TABLE \"%s\" ADD COLUMN \"%s\" %s"),
				*TableName, *Pair.Key, *Pair.Value);

			FESQLQueryResult AlterResult = Database->Execute(AlterSQL);
			if (AlterResult.bSuccess)
			{
				UE_LOG(LogExtendedSQL, Log, TEXT("SyncSchema: Added column '%s' (%s) to table '%s'"),
					*Pair.Key, *Pair.Value, *TableName);
			}
			else
			{
				UE_LOG(LogExtendedSQL, Warning, TEXT("SyncSchema: Failed to add column '%s': %s"),
					*Pair.Key, *AlterResult.ErrorMessage);
			}
		}
	}

	// DROP removed columns (only if they're not the PrimaryKeyColumn)
	TArray<FString> ColumnsToDrop;
	for (const FString& ExistingCol : ExistingColumns)
	{
		if (ExistingCol == PrimaryKeyColumn) continue;
		if (!ExpectedColumns.Contains(ExistingCol))
		{
			ColumnsToDrop.Add(ExistingCol);
		}
	}

	if (ColumnsToDrop.Num() > 0)
	{
		// Check SQLite version for DROP COLUMN support (3.35.0+)
		const int32 SQLiteVersion = sqlite3_libversion_number();
		const bool bSupportsDropColumn = SQLiteVersion >= 3035000;

		if (bSupportsDropColumn)
		{
			for (const FString& ColName : ColumnsToDrop)
			{
				FString DropSQL = FString::Printf(TEXT("ALTER TABLE \"%s\" DROP COLUMN \"%s\""),
					*TableName, *ColName);

				FESQLQueryResult DropResult = Database->Execute(DropSQL);
				if (DropResult.bSuccess)
				{
					UE_LOG(LogExtendedSQL, Log, TEXT("SyncSchema: Dropped column '%s' from table '%s'"),
						*ColName, *TableName);
				}
				else
				{
					UE_LOG(LogExtendedSQL, Warning, TEXT("SyncSchema: Failed to drop column '%s': %s"),
						*ColName, *DropResult.ErrorMessage);
				}
			}
		}
		else
		{
			// Recreate-table fallback for older SQLite
			UE_LOG(LogExtendedSQL, Log, TEXT("SyncSchema: SQLite %hs doesn't support DROP COLUMN. Using recreate-table approach."),
				sqlite3_libversion());

			// Build column list for the new table (only expected columns)
			FString ColumnList;
			bool bFirst = true;
			for (const auto& Pair : ExpectedColumns)
			{
				if (!bFirst) ColumnList += TEXT(", ");
				ColumnList += FString::Printf(TEXT("\"%s\""), *Pair.Key);
				bFirst = false;
			}

			FString ColumnDefs;
			bFirst = true;
			for (const auto& Pair : ExpectedColumns)
			{
				if (!bFirst) ColumnDefs += TEXT(", ");
				ColumnDefs += FString::Printf(TEXT("\"%s\" %s"), *Pair.Key, *Pair.Value);
				if (Pair.Key == PrimaryKeyColumn) ColumnDefs += TEXT(" PRIMARY KEY");
				bFirst = false;
			}

			const FString TempTable = TableName + TEXT("_migration_tmp");

			const FESQLErrorResult BeginResult = Database->BeginTransaction();
			if (!BeginResult.bSuccess)
			{
				return FESQLQueryResult::Failure(BeginResult.Error);
			}

			// CREATE new temp table
			Database->Execute(FString::Printf(TEXT("CREATE TABLE \"%s\" (%s)"), *TempTable, *ColumnDefs));

			// Copy data (only columns that exist in both old and new)
			FString CopyColumns;
			bFirst = true;
			for (const auto& Pair : ExpectedColumns)
			{
				if (ExistingColumns.Contains(Pair.Key))
				{
					if (!bFirst) CopyColumns += TEXT(", ");
					CopyColumns += FString::Printf(TEXT("\"%s\""), *Pair.Key);
					bFirst = false;
				}
			}

			if (!CopyColumns.IsEmpty())
			{
				Database->Execute(FString::Printf(TEXT("INSERT INTO \"%s\" (%s) SELECT %s FROM \"%s\""),
					*TempTable, *CopyColumns, *CopyColumns, *TableName));
			}

			// Drop old, rename new
			Database->Execute(FString::Printf(TEXT("DROP TABLE \"%s\""), *TableName));
			Database->Execute(FString::Printf(TEXT("ALTER TABLE \"%s\" RENAME TO \"%s\""), *TempTable, *TableName));

			const FESQLErrorResult CommitResult = Database->CommitTransaction();
			if (!CommitResult.bSuccess)
			{
				return FESQLQueryResult::Failure(CommitResult.Error);
			}

			UE_LOG(LogExtendedSQL, Log, TEXT("SyncSchema: Recreated table '%s' (dropped %d columns via migration)"),
				*TableName, ColumnsToDrop.Num());
		}
	}

	return FESQLQueryResult::Success();
}

bool UESQLTableAsset::ResolveAuthoringDatabase(
	TSharedPtr<FESQLDatabase> InDatabase,
	TSharedPtr<FESQLDatabase>& OutDatabase,
	TSharedPtr<FESQLDatabase>& OutOwnedDatabase,
	FString& OutError) const
{
	OutDatabase.Reset();
	OutOwnedDatabase.Reset();
	OutError.Reset();

	if (InDatabase && InDatabase->IsOpen())
	{
		OutDatabase = InDatabase;
		return true;
	}

	FString ResolvedDatabaseName = DatabaseName;
	ResolvedDatabaseName.TrimStartAndEndInline();
	if (ResolvedDatabaseName.IsEmpty())
	{
		OutError = TEXT("DatabaseName is not configured");
		return false;
	}

	const FString DatabasePath = UESQLSettings::ResolveDatabaseFilePath(ResolvedDatabaseName);
	const FESQLDatabaseOpenResult OpenResult = FESQLDatabase::Open(DatabasePath);
	if (!OpenResult)
	{
		OutError = OpenResult.GetErrorMessage();
		return false;
	}

	OutOwnedDatabase = OpenResult.GetValue();
	OutDatabase = OutOwnedDatabase;
	return true;
}


// ── Editor Support ───────────────────────────────────────────────────────────

TArray<FESQLColumn> UESQLTableAsset::GetColumnDefinitions() const
{
	TArray<FESQLColumn> Columns;

	// Primary key column first
	FESQLColumn PKCol;
	PKCol.Name = PrimaryKeyColumn;
	PKCol.Type = EESQLColumnType::Text;
	Columns.Add(PKCol);

	if (!RowStruct) return Columns;

	for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
	{
		FString SQLType = FESQLStructValidator::MapPropertyToSQLiteType(*It);
		if (SQLType.IsEmpty()) continue;

		FESQLColumn Col;
		Col.Name = (*It)->GetName();

		if (SQLType == TEXT("INTEGER")) Col.Type = EESQLColumnType::Integer;
		else if (SQLType == TEXT("REAL")) Col.Type = EESQLColumnType::Float;
		else if (SQLType == TEXT("TEXT")) Col.Type = EESQLColumnType::Text;
		else if (SQLType == TEXT("BLOB")) Col.Type = EESQLColumnType::Blob;
		else Col.Type = EESQLColumnType::Text;

		Columns.Add(Col);
	}

	return Columns;
}

bool UESQLTableAsset::ValidateStruct(TArray<FESQLStructValidator::FFieldResult>& OutResults) const
{
	if (!RowStruct) return false;
	return FESQLStructValidator::Validate(RowStruct, OutResults);
}

bool UESQLTableAsset::ExportToCSV(const FString& OutputFilePath, FString& OutError, TSharedPtr<FESQLDatabase> InDatabase) const
{
	TSharedPtr<FESQLDatabase> Db;
	TSharedPtr<FESQLDatabase> OwnedDatabase;
	if (!ResolveAuthoringDatabase(InDatabase, Db, OwnedDatabase, OutError))
	{
		return false;
	}

	FESQLQueryResult Result = Db->Execute(
		FString::Printf(TEXT("SELECT * FROM \"%s\""), *TableName));

	if (!Result.bSuccess)
	{
		OutError = Result.ErrorMessage;
		return false;
	}

	FString CSV;

	// Header row
	if (Result.ColumnDefinitions.Num() > 0)
	{
		for (int32 i = 0; i < Result.ColumnDefinitions.Num(); ++i)
		{
			if (i > 0) CSV += TEXT(",");
			CSV += Result.ColumnDefinitions[i].Name;
		}
		CSV += TEXT("\n");
	}

	// Data rows
	for (const FESQLRow& Row : Result.Rows)
	{
		for (int32 i = 0; i < Result.ColumnDefinitions.Num(); ++i)
		{
			if (i > 0) CSV += TEXT(",");
			const FString& ColName = Result.ColumnDefinitions[i].Name;
			FString Value = Row.Columns.FindRef(ColName);

			// Escape CSV: quote if contains comma, newline, or quote
			if (Value.Contains(TEXT(",")) || Value.Contains(TEXT("\n")) || Value.Contains(TEXT("\"")))
			{
				Value = Value.Replace(TEXT("\""), TEXT("\"\""));
				Value = FString::Printf(TEXT("\"%s\""), *Value);
			}

			CSV += Value;
		}
		CSV += TEXT("\n");
	}

	if (!FFileHelper::SaveStringToFile(CSV, *OutputFilePath))
	{
		OutError = FString::Printf(TEXT("Failed to write CSV to: %s"), *OutputFilePath);
		return false;
	}

	return true;
}

bool UESQLTableAsset::ImportFromCSV(const FString& InputFilePath, FString& OutError, TSharedPtr<FESQLDatabase> InDatabase)
{
	TSharedPtr<FESQLDatabase> Db;
	TSharedPtr<FESQLDatabase> OwnedDatabase;
	if (!ResolveAuthoringDatabase(InDatabase, Db, OwnedDatabase, OutError))
	{
		return false;
	}

	FString EnsureError;
	if (!EnsureTableExists(Db, EnsureError))
	{
		OutError = EnsureError;
		return false;
	}

	FString CSVContent;
	if (!FFileHelper::LoadFileToString(CSVContent, *InputFilePath))
	{
		OutError = FString::Printf(TEXT("Failed to read CSV from: %s"), *InputFilePath);
		return false;
	}

	// Parse CSV lines
	TArray<FString> Lines;
	CSVContent.ParseIntoArrayLines(Lines);

	if (Lines.Num() < 2) // Need at least header + 1 data row
	{
		OutError = TEXT("CSV file has no data rows");
		return false;
	}

	// Parse header
	TArray<FString> Headers;
	Lines[0].ParseIntoArray(Headers, TEXT(","), false);
	for (FString& Header : Headers)
	{
		Header.TrimStartAndEndInline();
		Header.RemoveFromStart(TEXT("\""));
		Header.RemoveFromEnd(TEXT("\""));
	}

	// Import data rows using UPSERT
	const FESQLErrorResult BeginResult = Db->BeginTransaction();
	if (!BeginResult.bSuccess)
	{
		OutError = BeginResult.GetErrorMessage();
		return false;
	}

	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		if (Lines[i].TrimStartAndEnd().IsEmpty()) continue;

		TArray<FString> Values;
		Lines[i].ParseIntoArray(Values, TEXT(","), false);

		TMap<FString, FString> ColumnValues;
		for (int32 j = 0; j < FMath::Min(Headers.Num(), Values.Num()); ++j)
		{
			FString Value = Values[j].TrimStartAndEnd();
			Value.RemoveFromStart(TEXT("\""));
			Value.RemoveFromEnd(TEXT("\""));
			ColumnValues.Add(Headers[j], Value);
		}

		if (ColumnValues.Num() == 0) continue;

		// Build UPSERT SQL
		FString ColList, PlaceholderList, UpdateList;
		TArray<FString> Bindings;
		int32 Idx = 1;

		for (const auto& Pair : ColumnValues)
		{
			if (Idx > 1) { ColList += TEXT(", "); PlaceholderList += TEXT(", "); }
			ColList += FString::Printf(TEXT("\"%s\""), *Pair.Key);
			PlaceholderList += FString::Printf(TEXT("?%d"), Idx);

			if (Pair.Key != PrimaryKeyColumn)
			{
				if (!UpdateList.IsEmpty()) UpdateList += TEXT(", ");
				UpdateList += FString::Printf(TEXT("\"%s\"=excluded.\"%s\""), *Pair.Key, *Pair.Key);
			}

			Bindings.Add(Pair.Value);
			++Idx;
		}

		FString SQL;
		if (UpdateList.IsEmpty())
		{
			SQL = FString::Printf(TEXT("INSERT OR IGNORE INTO \"%s\" (%s) VALUES (%s)"),
				*TableName, *ColList, *PlaceholderList);
		}
		else
		{
			SQL = FString::Printf(TEXT("INSERT INTO \"%s\" (%s) VALUES (%s) ON CONFLICT(\"%s\") DO UPDATE SET %s"),
				*TableName, *ColList, *PlaceholderList, *PrimaryKeyColumn, *UpdateList);
		}

		FESQLQueryResult InsertResult = Db->Execute(SQL, Bindings);
		if (!InsertResult.bSuccess)
		{
			const FESQLErrorResult RollbackResult = Db->RollbackTransaction();
			OutError = !RollbackResult.bSuccess
				? FString::Printf(TEXT("CSV import failed at row %d: %s (rollback error: %s)"), i, *InsertResult.ErrorMessage, *RollbackResult.GetErrorMessage())
				: FString::Printf(TEXT("CSV import failed at row %d: %s"), i, *InsertResult.ErrorMessage);
			return false;
		}
	}

	const FESQLErrorResult CommitResult = Db->CommitTransaction();
	if (!CommitResult.bSuccess)
	{
		OutError = CommitResult.GetErrorMessage();
		return false;
	}
	return true;
}

bool UESQLTableAsset::ExportToUSqlite(const FString& ProjectRoot, FString& OutError, TSharedPtr<FESQLDatabase> InDatabase) const
{
	TSharedPtr<FESQLDatabase> Db;
	TSharedPtr<FESQLDatabase> OwnedDatabase;
	if (!ResolveAuthoringDatabase(InDatabase, Db, OwnedDatabase, OutError))
	{
		return false;
	}

	const FString TargetProjectRoot = ProjectRoot.IsEmpty() ? GetUSqliteProjectPath() : ProjectRoot;
	return FESQLUsqliteSerializer::ExportDatabaseToProject(Db, TargetProjectRoot, OutError, this);
}

bool UESQLTableAsset::SyncDatabaseAndProject(FString& OutError)
{
	const FString DbPath = UESQLSettings::ResolveDatabaseFilePath(DatabaseName);
	return FESQLUsqliteSerializer::SyncDatabaseAndProject(DbPath, GetUSqliteProjectPath(), OutError, this);
}

FString UESQLTableAsset::GetUSqliteProjectPath() const
{
	const FString DbPath = UESQLSettings::ResolveDatabaseFilePath(DatabaseName);
	return FESQLUsqliteSerializer::GetProjectRootForDatabasePath(DbPath);
}
// ── Editor ───────────────────────────────────────────────────────────────────

bool UESQLTableAsset::EnsureTableExists(TSharedPtr<FESQLDatabase> InDatabase, FString& OutError)
{
	if (!InDatabase || !InDatabase->IsOpen())
	{
		OutError = TEXT("Database is not open");
		return false;
	}

	if (!RowStruct)
	{
		OutError = TEXT("RowStruct is not set");
		return false;
	}

	// Resolve table name from struct if not set
	if (TableName.IsEmpty())
	{
		TableName = RowStruct->GetName();
	}

	// Build CREATE TABLE IF NOT EXISTS from struct columns
	TMap<FString, FString> ColumnMap = FESQLStructValidator::BuildColumnMap(RowStruct);

	FString ColumnDefs;
	// Primary key first
	ColumnDefs += FString::Printf(TEXT("\"%s\" TEXT PRIMARY KEY"), *PrimaryKeyColumn);

	// Struct fields
	for (const auto& Pair : ColumnMap)
	{
		ColumnDefs += FString::Printf(TEXT(", \"%s\" %s"), *Pair.Key, *Pair.Value);
	}

	FString SQL = FString::Printf(TEXT("CREATE TABLE IF NOT EXISTS \"%s\" (%s)"),
		*TableName, *ColumnDefs);

	FESQLQueryResult Result = InDatabase->Execute(SQL);
	if (!Result.bSuccess)
	{
		OutError = Result.ErrorMessage;
		return false;
	}

	// Also run SyncSchema to add any missing columns
	FESQLQueryResult SyncResult = SyncSchema(InDatabase);
	if (!SyncResult.bSuccess)
	{
		UE_LOG(LogExtendedSQL, Warning, TEXT("EnsureTableExists: Schema sync warning: %s"), *SyncResult.ErrorMessage);
	}

	return true;
}

#if WITH_EDITOR
void UESQLTableAsset::GetValidationErrors(TArray<FText>& OutErrors) const
{
	OutErrors.Reset();

	const FString EffectiveTableName = TableName.IsEmpty() && RowStruct ? RowStruct->GetName() : TableName;

	if (!RowStruct)
	{
		OutErrors.Add(FText::FromString(TEXT("RowStruct must be set.")));
	}

	if (DatabaseName.TrimStartAndEnd().IsEmpty())
	{
		OutErrors.Add(FText::FromString(TEXT("DatabaseName must not be empty.")));
	}

	if (EffectiveTableName.TrimStartAndEnd().IsEmpty())
	{
		OutErrors.Add(FText::FromString(TEXT("TableName must not be empty.")));
	}

	if (PrimaryKeyColumn.TrimStartAndEnd().IsEmpty())
	{
		OutErrors.Add(FText::FromString(TEXT("PrimaryKeyColumn must not be empty.")));
	}

	if (RowStruct)
	{
		TArray<FESQLStructValidator::FFieldResult> Results;
		if (!FESQLStructValidator::Validate(RowStruct, Results))
		{
			for (const FESQLStructValidator::FFieldResult& Field : Results)
			{
				if (!Field.bIsValid)
				{
					OutErrors.Add(FText::FromString(FString::Printf(
						TEXT("RowStruct field '%s' (%s) is not supported: %s"),
						*Field.FieldName,
						*Field.UETypeName,
						*Field.ErrorReason)));
				}
			}
		}
	}

	const FString ProjectRoot = GetUSqliteProjectPath();
	const FString ProjectManifestPath = FPaths::Combine(ProjectRoot, TEXT("project.json"));
	if (IFileManager::Get().FileExists(*ProjectManifestPath))
	{
		FESQLUsqliteProject Project;
		FString ProjectError;
		if (!FESQLUsqliteSerializer::LoadProject(ProjectRoot, Project, ProjectError))
		{
			OutErrors.Add(FText::FromString(FString::Printf(TEXT(".usqlite project is invalid: %s"), *ProjectError)));
		}
		else if (!EffectiveTableName.TrimStartAndEnd().IsEmpty())
		{
			const FESQLUsqliteTableSchema* ProjectTable = Project.FindTable(EffectiveTableName);
			if (!ProjectTable)
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT(".usqlite project '%s' does not contain schema for table '%s'."),
					*Project.ProjectName,
					*EffectiveTableName)));
			}
			else
			{
				if (!PrimaryKeyColumn.IsEmpty() && !ProjectTable->PrimaryKeyColumn.IsEmpty() && ProjectTable->PrimaryKeyColumn != PrimaryKeyColumn)
				{
					OutErrors.Add(FText::FromString(FString::Printf(
						TEXT("Asset primary key '%s' does not match .usqlite primary key '%s'."),
						*PrimaryKeyColumn,
						*ProjectTable->PrimaryKeyColumn)));
				}

				if (RowStruct && !ProjectTable->RowStructPath.IsEmpty() && ProjectTable->RowStructPath != RowStruct->GetPathName())
				{
					OutErrors.Add(FText::FromString(FString::Printf(
						TEXT("Asset RowStruct '%s' does not match .usqlite rowStructPath '%s'."),
						*RowStruct->GetPathName(),
						*ProjectTable->RowStructPath)));
				}

				if (!DefaultLabelColumn.IsEmpty() && !ProjectTable->LabelColumn.IsEmpty() && ProjectTable->LabelColumn != DefaultLabelColumn)
				{
					OutErrors.Add(FText::FromString(FString::Printf(
						TEXT("Asset default label column '%s' does not match .usqlite label column '%s'."),
						*DefaultLabelColumn,
						*ProjectTable->LabelColumn)));
				}
			}
		}
	}
}

EDataValidationResult UESQLTableAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	TArray<FText> ValidationErrors;
	GetValidationErrors(ValidationErrors);
	for (const FText& ValidationError : ValidationErrors)
	{
		Context.AddError(ValidationError);
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

void UESQLTableAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = PropertyChangedEvent.GetPropertyName();

	if (PropName == GET_MEMBER_NAME_CHECKED(UESQLTableAsset, RowStruct))
	{
		// Re-validate struct when it changes
		TArray<FESQLStructValidator::FFieldResult> Results;
		if (RowStruct && !FESQLStructValidator::Validate(RowStruct, Results))
		{
			UE_LOG(LogExtendedSQL, Warning, TEXT("SQL Table Asset: RowStruct '%s' has unsupported fields"),
				*RowStruct->GetName());

			for (const auto& Field : Results)
			{
				if (!Field.bIsValid)
				{
					UE_LOG(LogExtendedSQL, Warning, TEXT("  ❌ %s (%s): %s"),
						*Field.FieldName, *Field.UETypeName, *Field.ErrorReason);
				}
			}
		}

		// Always sync TableName to the struct name when struct changes
		if (RowStruct)
		{
			const FString NewTableName = RowStruct->GetName();
			if (TableName != NewTableName)
			{
				UE_LOG(LogExtendedSQL, Log, TEXT("SQL Table Asset: TableName updated '%s' -> '%s'"),
					*TableName, *NewTableName);
				TableName = NewTableName;
			}
		}
	}
}
#endif
