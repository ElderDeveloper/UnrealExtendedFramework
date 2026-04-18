// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESQLTableAsset.h"
#include "ESQLStructValidator.h"
#include "PlayerData/ESQLPlayerDBComponent.h"
#include "Shared/ESQLPropertySerializer.h"
#include "Shared/ESQLSettings.h"
#include "Subsystem/ESQLSubsystem.h"
#include "UnrealExtendedSQL.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerState.h"
#include "Internationalization/Text.h"
#include "Misc/FileHelper.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

THIRD_PARTY_INCLUDES_START
#include "sqlite3.h"
THIRD_PARTY_INCLUDES_END

namespace
{
const FProperty* FindSQLTablePropertyByColumnName(const UScriptStruct* StructType, const FString& ColumnName)
{
	if (!StructType || ColumnName.IsEmpty())
	{
		return nullptr;
	}

	for (TFieldIterator<FProperty> It(StructType); It; ++It)
	{
		const FProperty* Property = *It;
		if (Property && Property->GetName() == ColumnName)
		{
			return Property;
		}
	}

	return nullptr;
}

FText MakeSQLDisplayTextFromColumnValue(const FProperty* Property, const FString& ColumnValue, const FString& FallbackValue)
{
	if (ColumnValue.IsEmpty())
	{
		return FText::FromString(FallbackValue);
	}

	if (CastField<const FTextProperty>(Property))
	{
		return FTextStringHelper::CreateFromBuffer(*ColumnValue);
	}

	return FText::FromString(ColumnValue);
}
}

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


// ── Runtime API ──────────────────────────────────────────────────────────────


FESQLQueryResult UESQLTableAsset::Initialize(UObject* WorldContextObject)
{
	if (bInitialized && CachedDatabase && CachedDatabase->IsOpen())
	{
		return FESQLQueryResult::Success();
	}

	if (!RowStruct)
	{
		return FESQLQueryResult::Failure(TEXT("RowStruct is not set on this SQL Table Asset"));
	}

	// Validate struct
	TArray<FESQLStructValidator::FFieldResult> ValidationResults;
	if (!FESQLStructValidator::Validate(RowStruct, ValidationResults))
	{
		return FESQLQueryResult::Failure(TEXT("RowStruct has unsupported field types. Check validation results."));
	}

	// Get subsystem
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UESQLSubsystem* Subsystem = GI ? GI->GetSubsystem<UESQLSubsystem>() : nullptr;

	if (!Subsystem)
	{
		return FESQLQueryResult::Failure(TEXT("Failed to get UESQLSubsystem"));
	}

	// Open database via subsystem
	if (!Subsystem->IsDatabaseOpen(DatabaseName))
	{
		FESQLQueryResult OpenResult = Subsystem->OpenDatabase(DatabaseName, Scope, Persistence);
		if (!OpenResult.bSuccess)
		{
			return OpenResult;
		}
	}

	// Resolve the table name
	if (TableName.IsEmpty())
	{
		TableName = RowStruct->GetName();
	}

	// Get internal database handle for schema operations
	// We access it through Execute — the subsystem manages the connection
	// Build CREATE TABLE with PrimaryKey + struct columns
	TMap<FString, FString> ColumnMap = FESQLStructValidator::BuildColumnMap(RowStruct);

	// Add PrimaryKeyColumn
	TMap<FString, FString> FullColumns;
	FullColumns.Add(PrimaryKeyColumn, TEXT("TEXT PRIMARY KEY"));
	for (const auto& Pair : ColumnMap)
	{
		FullColumns.Add(Pair.Key, Pair.Value);
	}

	// Create table if not exists
	FESQLQueryResult CreateResult = Subsystem->CreateTable(
		DatabaseName,
		TableName,
		FullColumns,
		TEXT(""),  // PK already included in column definition
		true       // IF NOT EXISTS
	);

	if (!CreateResult.bSuccess)
	{
		return CreateResult;
	}

	// Cache the database handle for direct operations
	// We need to reach into the subsystem's internals for SyncSchema
	// For now, open a direct connection to the same file
	const FString FilePath = UESQLSettings::ResolveDatabaseFilePath(DatabaseName);

	FString Error;
	CachedDatabase = FESQLDatabase::Open(FilePath, Error);
	if (!CachedDatabase)
	{
		UE_LOG(LogExtendedSQL, Warning, TEXT("SQL Table Asset: Could not open direct connection for SyncSchema: %s"), *Error);
		// Table was still created via subsystem, so we're partially initialized
	}
	else
	{
		// Run schema migration
		FESQLQueryResult SyncResult = SyncSchema(CachedDatabase);
		if (!SyncResult.bSuccess)
		{
			UE_LOG(LogExtendedSQL, Warning, TEXT("SQL Table Asset: Schema sync warning: %s"), *SyncResult.ErrorMessage);
		}
	}

	bInitialized = true;
	UE_LOG(LogExtendedSQL, Log, TEXT("SQL Table Asset '%s' initialized (DB: %s, Table: %s)"),
		*GetName(), *DatabaseName, *TableName);

	return FESQLQueryResult::Success();
}

bool UESQLTableAsset::IsInitialized() const
{
	return bInitialized;
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

FESQLQueryResult UESQLTableAsset::BuildGetAllRowsSQL(int32 MaxRows, FString& OutSQL) const
{
	if (TableName.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("TableName is not configured"));
	}

	OutSQL = FString::Printf(TEXT("SELECT * FROM \"%s\""), *TableName);
	if (MaxRows > 0)
	{
		OutSQL += FString::Printf(TEXT(" LIMIT %d"), MaxRows);
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

FESQLQueryResult UESQLTableAsset::ResolvePlayerDatabaseConnection(UESQLPlayerDBComponent* PlayerDBComponent, TSharedPtr<FESQLDatabase>& OutDatabase)
{
	OutDatabase.Reset();

	if (!PlayerDBComponent)
	{
		return FESQLQueryResult::Failure(TEXT("PlayerDBComponent is null"));
	}

	UWorld* World = PlayerDBComponent->GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UESQLSubsystem* Subsystem = GI ? GI->GetSubsystem<UESQLSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return FESQLQueryResult::Failure(TEXT("Failed to get UESQLSubsystem"));
	}

	APlayerState* PlayerState = PlayerDBComponent->GetOwningPlayerState();
	APlayerController* PlayerController = PlayerState ? PlayerState->GetPlayerController() : nullptr;
	if (!PlayerController)
	{
		return FESQLQueryResult::Failure(TEXT("Failed to resolve player controller from PlayerDBComponent"));
	}

	const FString PlayerId = PlayerDBComponent->GetPlayerId();
	if (PlayerId.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("Failed to resolve player identity from PlayerDBComponent"));
	}

	const FString ResolvedDatabaseName = !DatabaseName.IsEmpty() ? DatabaseName : PlayerDBComponent->PlayerDatabaseName;
	if (ResolvedDatabaseName.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("DatabaseName is not configured for player-scoped access"));
	}

	if (!Subsystem->IsPlayerDatabaseOpen(ResolvedDatabaseName, PlayerController))
	{
		const FESQLQueryResult OpenResult = Subsystem->OpenPlayerDatabase(ResolvedDatabaseName, PlayerController);
		if (!OpenResult.bSuccess)
		{
			return OpenResult;
		}
	}

	return ResolvePlayerDatabaseConnection(PlayerDBComponent, PlayerId, OutDatabase);
}

FESQLQueryResult UESQLTableAsset::ResolvePlayerDatabaseConnection(UObject* WorldContextObject, const FString& PlayerId, TSharedPtr<FESQLDatabase>& OutDatabase)
{
	OutDatabase.Reset();

	if (!WorldContextObject)
	{
		return FESQLQueryResult::Failure(TEXT("WorldContextObject is null"));
	}

	const FString TrimmedPlayerId = PlayerId.TrimStartAndEnd();
	if (TrimmedPlayerId.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("PlayerId cannot be empty"));
	}

	UWorld* World = WorldContextObject->GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UESQLSubsystem* Subsystem = GI ? GI->GetSubsystem<UESQLSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return FESQLQueryResult::Failure(TEXT("Failed to get UESQLSubsystem"));
	}

	if (!Subsystem->HasServerAuthority())
	{
		return FESQLQueryResult::Failure(TEXT("Player-scoped table access is server-only"));
	}

	FString ResolvedDatabaseName = DatabaseName;
	ResolvedDatabaseName.TrimStartAndEndInline();
	if (ResolvedDatabaseName.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("DatabaseName is not configured for player-scoped access"));
	}

	FString Error;
	OutDatabase = FESQLDatabase::Open(UESQLSettings::ResolvePlayerDatabaseFilePath(ResolvedDatabaseName, TrimmedPlayerId), Error);
	if (!OutDatabase)
	{
		return FESQLQueryResult::Failure(Error);
	}

	FString EnsureError;
	if (!EnsureTableExists(OutDatabase, EnsureError))
	{
		return FESQLQueryResult::Failure(EnsureError);
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

FESQLQueryResult UESQLTableAsset::LoadRowIntoStruct(
	UObject* WorldContextObject,
	const FString& RowId,
	void* OutStructData,
	const UScriptStruct* StructType)
{
	const FESQLQueryResult ValidationResult = ValidateTypedStructAccess(StructType);
	if (!ValidationResult.bSuccess)
	{
		return ValidationResult;
	}

	if (!OutStructData)
	{
		return FESQLQueryResult::Failure(TEXT("OutStructData is null"));
	}

	const FESQLQueryResult QueryResult = GetRowById(WorldContextObject, RowId);
	if (!QueryResult.bSuccess)
	{
		return QueryResult;
	}

	if (QueryResult.Rows.Num() == 0)
	{
		return FESQLQueryResult::Failure(FString::Printf(
			TEXT("No row found in table '%s' for id '%s'"),
			*TableName,
			*RowId));
	}

	return PopulateStructFromRow(QueryResult.Rows[0], OutStructData, StructType);
}

FESQLQueryResult UESQLTableAsset::LoadRowsIntoStructArray(
	UObject* WorldContextObject,
	void* OutArrayData,
	const FArrayProperty* ArrayProperty,
	int32 MaxRows)
{
	FESQLQuerySpec QuerySpec;
	QuerySpec.Limit = MaxRows > 0 ? MaxRows : 0;
	return QueryRowsIntoStructArray(WorldContextObject, QuerySpec, OutArrayData, ArrayProperty);
}

FESQLQueryResult UESQLTableAsset::LoadPageIntoStructArray(
	UObject* WorldContextObject,
	const FESQLQuerySpec& BaseQuerySpec,
	int32 PageIndex,
	int32 PageSize,
	void* OutArrayData,
	const FArrayProperty* ArrayProperty)
{
	if (PageIndex < 0)
	{
		return FESQLQueryResult::Failure(TEXT("PageIndex cannot be negative"));
	}

	if (PageSize <= 0)
	{
		return FESQLQueryResult::Failure(TEXT("PageSize must be greater than zero"));
	}

	const int64 RequestedOffset = static_cast<int64>(PageIndex) * static_cast<int64>(PageSize);
	if (RequestedOffset > MAX_int32)
	{
		return FESQLQueryResult::Failure(TEXT("Requested page offset exceeds supported range"));
	}

	FESQLQuerySpec PagedQuerySpec = BaseQuerySpec;
	PagedQuerySpec.Limit = PageSize;
	PagedQuerySpec.Offset = static_cast<int32>(RequestedOffset);
	return QueryRowsIntoStructArray(WorldContextObject, PagedQuerySpec, OutArrayData, ArrayProperty);
}

FESQLQueryResult UESQLTableAsset::LoadRowIntoStruct(
	UObject* WorldContextObject,
	const FESQLId& SqlId,
	void* OutStructData,
	const UScriptStruct* StructType)
{
	return LoadRowIntoStruct(WorldContextObject, SqlId.Value, OutStructData, StructType);
}

FESQLQueryResult UESQLTableAsset::LoadPlayerRowIntoStruct(
	UESQLPlayerDBComponent* PlayerDBComponent,
	const FString& RowId,
	void* OutStructData,
	const UScriptStruct* StructType)
{
	const FESQLQueryResult ValidationResult = ValidateTypedStructAccess(StructType);
	if (!ValidationResult.bSuccess)
	{
		return ValidationResult;
	}

	if (!OutStructData)
	{
		return FESQLQueryResult::Failure(TEXT("OutStructData is null"));
	}

	TSharedPtr<FESQLDatabase> PlayerDatabase;
	const FESQLQueryResult ResolveResult = ResolvePlayerDatabaseConnection(PlayerDBComponent, PlayerDatabase);
	if (!ResolveResult.bSuccess)
	{
		return ResolveResult;
	}

	FString SQL;
	TArray<FString> Bindings;
	const FESQLQueryResult BuildResult = BuildGetRowByIdSQL(RowId, SQL, Bindings);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	const FESQLQueryResult QueryResult = PlayerDatabase->Execute(SQL, Bindings);
	if (!QueryResult.bSuccess)
	{
		return QueryResult;
	}

	if (QueryResult.Rows.Num() == 0)
	{
		return FESQLQueryResult::Failure(FString::Printf(
			TEXT("No row found in table '%s' for id '%s'"),
			*TableName,
			*RowId));
	}

	return PopulateStructFromRow(QueryResult.Rows[0], OutStructData, StructType);
}

FESQLQueryResult UESQLTableAsset::LoadPlayerRowIntoStruct(
	UObject* WorldContextObject,
	const FString& PlayerId,
	const FString& RowId,
	void* OutStructData,
	const UScriptStruct* StructType)
{
	const FESQLQueryResult ValidationResult = ValidateTypedStructAccess(StructType);
	if (!ValidationResult.bSuccess)
	{
		return ValidationResult;
	}

	if (!OutStructData)
	{
		return FESQLQueryResult::Failure(TEXT("OutStructData is null"));
	}

	TSharedPtr<FESQLDatabase> PlayerDatabase;
	const FESQLQueryResult ResolveResult = ResolvePlayerDatabaseConnection(WorldContextObject, PlayerId, PlayerDatabase);
	if (!ResolveResult.bSuccess)
	{
		return ResolveResult;
	}

	FString SQL;
	TArray<FString> Bindings;
	const FESQLQueryResult BuildResult = BuildGetRowByIdSQL(RowId, SQL, Bindings);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	const FESQLQueryResult QueryResult = PlayerDatabase->Execute(SQL, Bindings);
	if (!QueryResult.bSuccess)
	{
		return QueryResult;
	}

	if (QueryResult.Rows.Num() == 0)
	{
		return FESQLQueryResult::Failure(FString::Printf(
			TEXT("No row found in table '%s' for id '%s'"),
			*TableName,
			*RowId));
	}

	return PopulateStructFromRow(QueryResult.Rows[0], OutStructData, StructType);
}

FESQLQueryResult UESQLTableAsset::LoadPlayerRowIntoStruct(
	UESQLPlayerDBComponent* PlayerDBComponent,
	const FESQLId& SqlId,
	void* OutStructData,
	const UScriptStruct* StructType)
{
	return LoadPlayerRowIntoStruct(PlayerDBComponent, SqlId.Value, OutStructData, StructType);
}

FESQLQueryResult UESQLTableAsset::LoadPlayerRowIntoStruct(
	UObject* WorldContextObject,
	const FString& PlayerId,
	const FESQLId& SqlId,
	void* OutStructData,
	const UScriptStruct* StructType)
{
	return LoadPlayerRowIntoStruct(WorldContextObject, PlayerId, SqlId.Value, OutStructData, StructType);
}

FESQLQueryResult UESQLTableAsset::LoadPlayerRowsIntoStructArray(
	UESQLPlayerDBComponent* PlayerDBComponent,
	void* OutArrayData,
	const FArrayProperty* ArrayProperty,
	int32 MaxRows)
{
	FESQLQuerySpec QuerySpec;
	QuerySpec.Limit = MaxRows > 0 ? MaxRows : 0;
	return QueryPlayerRowsIntoStructArray(PlayerDBComponent, QuerySpec, OutArrayData, ArrayProperty);
}

FESQLQueryResult UESQLTableAsset::LoadPlayerPageIntoStructArray(
	UESQLPlayerDBComponent* PlayerDBComponent,
	const FESQLQuerySpec& BaseQuerySpec,
	int32 PageIndex,
	int32 PageSize,
	void* OutArrayData,
	const FArrayProperty* ArrayProperty)
{
	if (PageIndex < 0)
	{
		return FESQLQueryResult::Failure(TEXT("PageIndex cannot be negative"));
	}

	if (PageSize <= 0)
	{
		return FESQLQueryResult::Failure(TEXT("PageSize must be greater than zero"));
	}

	const int64 RequestedOffset = static_cast<int64>(PageIndex) * static_cast<int64>(PageSize);
	if (RequestedOffset > MAX_int32)
	{
		return FESQLQueryResult::Failure(TEXT("Requested page offset exceeds supported range"));
	}

	FESQLQuerySpec PagedQuerySpec = BaseQuerySpec;
	PagedQuerySpec.Limit = PageSize;
	PagedQuerySpec.Offset = static_cast<int32>(RequestedOffset);
	return QueryPlayerRowsIntoStructArray(PlayerDBComponent, PagedQuerySpec, OutArrayData, ArrayProperty);
}

FESQLQueryResult UESQLTableAsset::LoadPlayerPageIntoStructArray(
	UObject* WorldContextObject,
	const FString& PlayerId,
	const FESQLQuerySpec& BaseQuerySpec,
	int32 PageIndex,
	int32 PageSize,
	void* OutArrayData,
	const FArrayProperty* ArrayProperty)
{
	if (PageIndex < 0)
	{
		return FESQLQueryResult::Failure(TEXT("PageIndex cannot be negative"));
	}

	if (PageSize <= 0)
	{
		return FESQLQueryResult::Failure(TEXT("PageSize must be greater than zero"));
	}

	const int64 RequestedOffset = static_cast<int64>(PageIndex) * static_cast<int64>(PageSize);
	if (RequestedOffset > MAX_int32)
	{
		return FESQLQueryResult::Failure(TEXT("Requested page offset exceeds supported range"));
	}

	FESQLQuerySpec PagedQuerySpec = BaseQuerySpec;
	PagedQuerySpec.Limit = PageSize;
	PagedQuerySpec.Offset = static_cast<int32>(RequestedOffset);
	return QueryPlayerRowsIntoStructArray(WorldContextObject, PlayerId, PagedQuerySpec, OutArrayData, ArrayProperty);
}

FText UESQLTableAsset::ResolveRowDisplayText(
	UObject* WorldContextObject,
	const FString& RowId,
	const FString& LabelColumnOverride,
	FString* OutError)
{
	if (RowId.IsEmpty())
	{
		if (OutError)
		{
			*OutError = TEXT("RowId cannot be empty");
		}

		return FText::GetEmpty();
	}

	const FESQLQueryResult QueryResult = GetRowById(WorldContextObject, RowId);
	if (!QueryResult.bSuccess)
	{
		if (OutError)
		{
			*OutError = QueryResult.ErrorMessage;
		}

		return FText::FromString(RowId);
	}

	if (QueryResult.Rows.Num() == 0)
	{
		if (OutError)
		{
			*OutError = FString::Printf(
				TEXT("No row found in table '%s' for id '%s'"),
				*TableName,
				*RowId);
		}

		return FText::FromString(RowId);
	}

	const FString LabelColumn = GetEffectiveLabelColumn(LabelColumnOverride);
	if (LabelColumn.IsEmpty() || LabelColumn == PrimaryKeyColumn)
	{
		if (OutError)
		{
			OutError->Reset();
		}

		return FText::FromString(RowId);
	}

	const FESQLRow& Row = QueryResult.Rows[0];
	if (Row.IsNull(LabelColumn))
	{
		if (OutError)
		{
			OutError->Reset();
		}

		return FText::FromString(RowId);
	}

	const FString* LabelValue = Row.FindValue(LabelColumn);
	if (!LabelValue)
	{
		if (OutError)
		{
			*OutError = FString::Printf(
				TEXT("Label column '%s' was not found in row '%s' for table '%s'"),
				*LabelColumn,
				*RowId,
				*TableName);
		}

		return FText::FromString(RowId);
	}

	if (OutError)
	{
		OutError->Reset();
	}

	return MakeSQLDisplayTextFromColumnValue(
		FindSQLTablePropertyByColumnName(RowStruct, LabelColumn),
		*LabelValue,
		RowId);
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

FESQLQueryResult UESQLTableAsset::QueryRows(UObject* WorldContextObject, const FESQLQuerySpec& QuerySpec)
{
	const FESQLQueryResult InitResult = Initialize(WorldContextObject);
	if (!InitResult.bSuccess)
	{
		return InitResult;
	}

	if (!CachedDatabase || !CachedDatabase->IsOpen())
	{
		return FESQLQueryResult::Failure(TEXT("Database connection is not available"));
	}

	FString SQL;
	TArray<FESQLBindingValue> Bindings;
	const FESQLQueryResult BuildResult = BuildQueryRowsSQL(QuerySpec, SQL, Bindings);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	return Bindings.Num() > 0
		? CachedDatabase->Execute(SQL, Bindings)
		: CachedDatabase->Execute(SQL);
}

FESQLQueryResult UESQLTableAsset::QueryRowsIntoStructArray(
	UObject* WorldContextObject,
	const FESQLQuerySpec& QuerySpec,
	void* OutArrayData,
	const FArrayProperty* ArrayProperty)
{
	const FESQLQueryResult QueryResult = QueryRows(WorldContextObject, QuerySpec);
	if (!QueryResult.bSuccess)
	{
		return QueryResult;
	}

	const FESQLQueryResult PopulateResult = PopulateStructArrayFromRows(QueryResult.Rows, OutArrayData, ArrayProperty);
	if (!PopulateResult.bSuccess)
	{
		return PopulateResult;
	}

	return QueryResult;
}

void UESQLTableAsset::AsyncLoadRowResult(
	UObject* WorldContextObject,
	const FString& RowId,
	TFunction<void(const FESQLQueryResult&)> OnComplete)
{
	if (!OnComplete)
	{
		return;
	}

	if (RowId.IsEmpty())
	{
		OnComplete(FESQLQueryResult::Failure(TEXT("RowId cannot be empty")));
		return;
	}

	const FESQLQueryResult InitResult = Initialize(WorldContextObject);
	if (!InitResult.bSuccess)
	{
		OnComplete(InitResult);
		return;
	}

	if (!CachedDatabase || !CachedDatabase->IsOpen())
	{
		OnComplete(FESQLQueryResult::Failure(TEXT("Database connection is not available")));
		return;
	}

	FString SQL;
	TArray<FString> Bindings;
	const FESQLQueryResult BuildResult = BuildGetRowByIdSQL(RowId, SQL, Bindings);
	if (!BuildResult.bSuccess)
	{
		OnComplete(BuildResult);
		return;
	}

	TSharedPtr<FESQLDatabase> Database = CachedDatabase;
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Database, SQL = MoveTemp(SQL), Bindings = MoveTemp(Bindings), OnComplete = MoveTemp(OnComplete)]() mutable
	{
		const FESQLQueryResult QueryResult = Database->Execute(SQL, Bindings);

		AsyncTask(ENamedThreads::GameThread, [QueryResult, OnComplete = MoveTemp(OnComplete)]() mutable
		{
			OnComplete(QueryResult);
		});
	});
}

void UESQLTableAsset::AsyncLoadRowsResult(
	UObject* WorldContextObject,
	TFunction<void(const FESQLQueryResult&)> OnComplete,
	int32 MaxRows)
{
	if (!OnComplete)
	{
		return;
	}

	const FESQLQueryResult InitResult = Initialize(WorldContextObject);
	if (!InitResult.bSuccess)
	{
		OnComplete(InitResult);
		return;
	}

	if (!CachedDatabase || !CachedDatabase->IsOpen())
	{
		OnComplete(FESQLQueryResult::Failure(TEXT("Database connection is not available")));
		return;
	}

	FString SQL;
	const FESQLQueryResult BuildResult = BuildGetAllRowsSQL(MaxRows, SQL);
	if (!BuildResult.bSuccess)
	{
		OnComplete(BuildResult);
		return;
	}

	TSharedPtr<FESQLDatabase> Database = CachedDatabase;
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Database, SQL = MoveTemp(SQL), OnComplete = MoveTemp(OnComplete)]() mutable
	{
		const FESQLQueryResult QueryResult = Database->Execute(SQL);

		AsyncTask(ENamedThreads::GameThread, [QueryResult, OnComplete = MoveTemp(OnComplete)]() mutable
		{
			OnComplete(QueryResult);
		});
	});
}

void UESQLTableAsset::AsyncQueryRowsResult(
	UObject* WorldContextObject,
	const FESQLQuerySpec& QuerySpec,
	TFunction<void(const FESQLQueryResult&)> OnComplete)
{
	if (!OnComplete)
	{
		return;
	}

	const FESQLQueryResult InitResult = Initialize(WorldContextObject);
	if (!InitResult.bSuccess)
	{
		OnComplete(InitResult);
		return;
	}

	if (!CachedDatabase || !CachedDatabase->IsOpen())
	{
		OnComplete(FESQLQueryResult::Failure(TEXT("Database connection is not available")));
		return;
	}

	FString SQL;
	TArray<FESQLBindingValue> Bindings;
	const FESQLQueryResult BuildResult = BuildQueryRowsSQL(QuerySpec, SQL, Bindings);
	if (!BuildResult.bSuccess)
	{
		OnComplete(BuildResult);
		return;
	}

	TSharedPtr<FESQLDatabase> Database = CachedDatabase;
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Database, SQL = MoveTemp(SQL), Bindings = MoveTemp(Bindings), OnComplete = MoveTemp(OnComplete)]() mutable
	{
		const FESQLQueryResult QueryResult = Bindings.Num() > 0
			? Database->Execute(SQL, Bindings)
			: Database->Execute(SQL);

		AsyncTask(ENamedThreads::GameThread, [QueryResult, OnComplete = MoveTemp(OnComplete)]() mutable
		{
			OnComplete(QueryResult);
		});
	});
}

void UESQLTableAsset::AsyncSaveRowFromStruct(
	UObject* WorldContextObject,
	const void* StructData,
	const UScriptStruct* StructType,
	TFunction<void(const FESQLQueryResult&, const FString&)> OnComplete,
	const FString& RowIdOverride)
{
	if (!OnComplete)
	{
		return;
	}

	const FESQLQueryResult ValidationResult = ValidateTypedStructAccess(StructType);
	if (!ValidationResult.bSuccess)
	{
		OnComplete(ValidationResult, FString());
		return;
	}

	const FESQLQueryResult InitResult = Initialize(WorldContextObject);
	if (!InitResult.bSuccess)
	{
		OnComplete(InitResult, FString());
		return;
	}

	if (!CachedDatabase || !CachedDatabase->IsOpen())
	{
		OnComplete(FESQLQueryResult::Failure(TEXT("Database connection is not available")), FString());
		return;
	}

	FString SQL;
	TArray<FESQLBindingValue> Bindings;
	FString ResolvedRowId;
	const FESQLQueryResult BuildResult = BuildSaveRowSQL(StructData, StructType, SQL, Bindings, ResolvedRowId, RowIdOverride);
	if (!BuildResult.bSuccess)
	{
		OnComplete(BuildResult, FString());
		return;
	}

	TSharedPtr<FESQLDatabase> Database = CachedDatabase;
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Database, SQL = MoveTemp(SQL), Bindings = MoveTemp(Bindings), ResolvedRowId = MoveTemp(ResolvedRowId), OnComplete = MoveTemp(OnComplete)]() mutable
	{
		const FESQLQueryResult SaveResult = Database->Execute(SQL, Bindings);

		AsyncTask(ENamedThreads::GameThread, [SaveResult, ResolvedRowId = MoveTemp(ResolvedRowId), OnComplete = MoveTemp(OnComplete)]() mutable
		{
			OnComplete(SaveResult, SaveResult.bSuccess ? ResolvedRowId : FString());
		});
	});
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

FESQLQueryResult UESQLTableAsset::QueryPlayerRows(UESQLPlayerDBComponent* PlayerDBComponent, const FESQLQuerySpec& QuerySpec)
{
	TSharedPtr<FESQLDatabase> PlayerDatabase;
	const FESQLQueryResult ResolveResult = ResolvePlayerDatabaseConnection(PlayerDBComponent, PlayerDatabase);
	if (!ResolveResult.bSuccess)
	{
		return ResolveResult;
	}

	FString SQL;
	TArray<FESQLBindingValue> Bindings;
	const FESQLQueryResult BuildResult = BuildQueryRowsSQL(QuerySpec, SQL, Bindings);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	return Bindings.Num() > 0
		? PlayerDatabase->Execute(SQL, Bindings)
		: PlayerDatabase->Execute(SQL);
}

FESQLQueryResult UESQLTableAsset::QueryPlayerRowsIntoStructArray(
	UESQLPlayerDBComponent* PlayerDBComponent,
	const FESQLQuerySpec& QuerySpec,
	void* OutArrayData,
	const FArrayProperty* ArrayProperty)
{
	const FESQLQueryResult QueryResult = QueryPlayerRows(PlayerDBComponent, QuerySpec);
	if (!QueryResult.bSuccess)
	{
		return QueryResult;
	}

	const FESQLQueryResult PopulateResult = PopulateStructArrayFromRows(QueryResult.Rows, OutArrayData, ArrayProperty);
	if (!PopulateResult.bSuccess)
	{
		return PopulateResult;
	}

	return QueryResult;
}

FESQLQueryResult UESQLTableAsset::QueryPlayerRowsIntoStructArray(
	UObject* WorldContextObject,
	const FString& PlayerId,
	const FESQLQuerySpec& QuerySpec,
	void* OutArrayData,
	const FArrayProperty* ArrayProperty)
{
	const FESQLQueryResult QueryResult = QueryPlayerRows(WorldContextObject, PlayerId, QuerySpec);
	if (!QueryResult.bSuccess)
	{
		return QueryResult;
	}

	const FESQLQueryResult PopulateResult = PopulateStructArrayFromRows(QueryResult.Rows, OutArrayData, ArrayProperty);
	if (!PopulateResult.bSuccess)
	{
		return PopulateResult;
	}

	return QueryResult;
}

FESQLQueryResult UESQLTableAsset::QueryPlayerRows(UObject* WorldContextObject, const FString& PlayerId, const FESQLQuerySpec& QuerySpec)
{
	TSharedPtr<FESQLDatabase> PlayerDatabase;
	const FESQLQueryResult ResolveResult = ResolvePlayerDatabaseConnection(WorldContextObject, PlayerId, PlayerDatabase);
	if (!ResolveResult.bSuccess)
	{
		return ResolveResult;
	}

	FString SQL;
	TArray<FESQLBindingValue> Bindings;
	const FESQLQueryResult BuildResult = BuildQueryRowsSQL(QuerySpec, SQL, Bindings);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	return Bindings.Num() > 0
		? PlayerDatabase->Execute(SQL, Bindings)
		: PlayerDatabase->Execute(SQL);
}

FESQLQueryResult UESQLTableAsset::CountPlayerRows(UESQLPlayerDBComponent* PlayerDBComponent, const FESQLQuerySpec& QuerySpec, int64& OutCount)
{
	OutCount = 0;

	TSharedPtr<FESQLDatabase> PlayerDatabase;
	const FESQLQueryResult ResolveResult = ResolvePlayerDatabaseConnection(PlayerDBComponent, PlayerDatabase);
	if (!ResolveResult.bSuccess)
	{
		return ResolveResult;
	}

	FString SQL;
	TArray<FESQLBindingValue> Bindings;
	const FESQLQueryResult BuildResult = BuildQuerySQL(TEXT("SELECT COUNT(*) AS RowCount"), QuerySpec, false, false, SQL, Bindings);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	const FESQLQueryResult QueryResult = Bindings.Num() > 0
		? PlayerDatabase->Execute(SQL, Bindings)
		: PlayerDatabase->Execute(SQL);
	if (!QueryResult.bSuccess)
	{
		return QueryResult;
	}

	const FESQLRow* Row = QueryResult.GetFirstRow();
	if (!Row)
	{
		return FESQLQueryResult::Failure(TEXT("Count query returned no rows"));
	}

	if (!Row->TryGetInt64(TEXT("RowCount"), OutCount))
	{
		return FESQLQueryResult::Failure(TEXT("Count query did not return a valid RowCount value"));
	}

	return QueryResult;
}

FESQLQueryResult UESQLTableAsset::CountPlayerRows(UObject* WorldContextObject, const FString& PlayerId, const FESQLQuerySpec& QuerySpec, int64& OutCount)
{
	OutCount = 0;

	TSharedPtr<FESQLDatabase> PlayerDatabase;
	const FESQLQueryResult ResolveResult = ResolvePlayerDatabaseConnection(WorldContextObject, PlayerId, PlayerDatabase);
	if (!ResolveResult.bSuccess)
	{
		return ResolveResult;
	}

	FString SQL;
	TArray<FESQLBindingValue> Bindings;
	const FESQLQueryResult BuildResult = BuildQuerySQL(TEXT("SELECT COUNT(*) AS RowCount"), QuerySpec, false, false, SQL, Bindings);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	const FESQLQueryResult QueryResult = Bindings.Num() > 0
		? PlayerDatabase->Execute(SQL, Bindings)
		: PlayerDatabase->Execute(SQL);
	if (!QueryResult.bSuccess)
	{
		return QueryResult;
	}

	const FESQLRow* Row = QueryResult.GetFirstRow();
	if (!Row)
	{
		return FESQLQueryResult::Failure(TEXT("Count query returned no rows"));
	}

	if (!Row->TryGetInt64(TEXT("RowCount"), OutCount))
	{
		return FESQLQueryResult::Failure(TEXT("Count query did not return a valid RowCount value"));
	}

	return QueryResult;
}

FESQLQueryResult UESQLTableAsset::CountRows(UObject* WorldContextObject, const FESQLQuerySpec& QuerySpec, int64& OutCount)
{
	OutCount = 0;

	const FESQLQueryResult InitResult = Initialize(WorldContextObject);
	if (!InitResult.bSuccess)
	{
		return InitResult;
	}

	if (!CachedDatabase || !CachedDatabase->IsOpen())
	{
		return FESQLQueryResult::Failure(TEXT("Database connection is not available"));
	}

	FString SQL;
	TArray<FESQLBindingValue> Bindings;
	const FESQLQueryResult BuildResult = BuildQuerySQL(TEXT("SELECT COUNT(*) AS RowCount"), QuerySpec, false, false, SQL, Bindings);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	const FESQLQueryResult QueryResult = Bindings.Num() > 0
		? CachedDatabase->Execute(SQL, Bindings)
		: CachedDatabase->Execute(SQL);
	if (!QueryResult.bSuccess)
	{
		return QueryResult;
	}

	const FESQLRow* Row = QueryResult.GetFirstRow();
	if (!Row)
	{
		return FESQLQueryResult::Failure(TEXT("Count query returned no rows"));
	}

	if (!Row->TryGetInt64(TEXT("RowCount"), OutCount))
	{
		return FESQLQueryResult::Failure(TEXT("Count query did not return a valid RowCount value"));
	}

	return QueryResult;
}

bool UESQLTableAsset::TryCountPlayerRows(UESQLPlayerDBComponent* PlayerDBComponent, const FESQLQuerySpec& QuerySpec, int64& OutCount, FString* OutError)
{
	const FESQLQueryResult Result = CountPlayerRows(PlayerDBComponent, QuerySpec, OutCount);
	if (!Result.bSuccess)
	{
		OutCount = 0;
		if (OutError)
		{
			*OutError = Result.ErrorMessage;
		}

		return false;
	}

	if (OutError)
	{
		OutError->Reset();
	}

	return true;
}

bool UESQLTableAsset::TryCountPlayerRows(UObject* WorldContextObject, const FString& PlayerId, const FESQLQuerySpec& QuerySpec, int64& OutCount, FString* OutError)
{
	const FESQLQueryResult Result = CountPlayerRows(WorldContextObject, PlayerId, QuerySpec, OutCount);
	if (!Result.bSuccess)
	{
		OutCount = 0;
		if (OutError)
		{
			*OutError = Result.ErrorMessage;
		}

		return false;
	}

	if (OutError)
	{
		OutError->Reset();
	}

	return true;
}

bool UESQLTableAsset::TryCountRows(UObject* WorldContextObject, const FESQLQuerySpec& QuerySpec, int64& OutCount, FString* OutError)
{
	const FESQLQueryResult Result = CountRows(WorldContextObject, QuerySpec, OutCount);
	if (!Result.bSuccess)
	{
		OutCount = 0;
		if (OutError)
		{
			*OutError = Result.ErrorMessage;
		}

		return false;
	}

	if (OutError)
	{
		OutError->Reset();
	}

	return true;
}

FText UESQLTableAsset::ResolveRowDisplayText(
	UObject* WorldContextObject,
	const FESQLId& SqlId,
	FString* OutError)
{
	return ResolveRowDisplayText(WorldContextObject, SqlId.Value, SqlId.LabelColumn, OutError);
}

bool UESQLTableAsset::DoesRowExist(UObject* WorldContextObject, const FString& RowId, FString* OutError)
{
	const FESQLQueryResult QueryResult = GetRowById(WorldContextObject, RowId);
	if (!QueryResult.bSuccess)
	{
		if (OutError)
		{
			*OutError = QueryResult.ErrorMessage;
		}

		return false;
	}

	if (OutError)
	{
		OutError->Reset();
	}

	return QueryResult.Rows.Num() > 0;
}

bool UESQLTableAsset::DoesRowExist(UObject* WorldContextObject, const FESQLId& SqlId, FString* OutError)
{
	return DoesRowExist(WorldContextObject, SqlId.Value, OutError);
}

FESQLQueryResult UESQLTableAsset::DeleteRowById(UObject* WorldContextObject, const FString& RowId)
{
	if (RowId.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("RowId cannot be empty"));
	}

	const FESQLQueryResult InitResult = Initialize(WorldContextObject);
	if (!InitResult.bSuccess)
	{
		return InitResult;
	}

	if (!CachedDatabase || !CachedDatabase->IsOpen())
	{
		return FESQLQueryResult::Failure(TEXT("Database connection is not available"));
	}

	if (PrimaryKeyColumn.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("PrimaryKeyColumn is not configured"));
	}

	const FString SQL = FString::Printf(
		TEXT("DELETE FROM \"%s\" WHERE \"%s\"=?1"),
		*TableName,
		*PrimaryKeyColumn);

	TArray<FString> Bindings;
	Bindings.Add(RowId);
	return CachedDatabase->Execute(SQL, Bindings);
}

FESQLQueryResult UESQLTableAsset::DeleteRowById(UObject* WorldContextObject, const FESQLId& SqlId)
{
	return DeleteRowById(WorldContextObject, SqlId.Value);
}

FESQLQueryResult UESQLTableAsset::DeletePlayerRowById(UESQLPlayerDBComponent* PlayerDBComponent, const FString& RowId)
{
	if (RowId.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("RowId cannot be empty"));
	}

	TSharedPtr<FESQLDatabase> PlayerDatabase;
	const FESQLQueryResult ResolveResult = ResolvePlayerDatabaseConnection(PlayerDBComponent, PlayerDatabase);
	if (!ResolveResult.bSuccess)
	{
		return ResolveResult;
	}

	if (PrimaryKeyColumn.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("PrimaryKeyColumn is not configured"));
	}

	const FString SQL = FString::Printf(
		TEXT("DELETE FROM \"%s\" WHERE \"%s\"=?1"),
		*TableName,
		*PrimaryKeyColumn);

	TArray<FString> Bindings;
	Bindings.Add(RowId);
	return PlayerDatabase->Execute(SQL, Bindings);
}

FESQLQueryResult UESQLTableAsset::DeletePlayerRowById(UObject* WorldContextObject, const FString& PlayerId, const FString& RowId)
{
	if (RowId.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("RowId cannot be empty"));
	}

	TSharedPtr<FESQLDatabase> PlayerDatabase;
	const FESQLQueryResult ResolveResult = ResolvePlayerDatabaseConnection(WorldContextObject, PlayerId, PlayerDatabase);
	if (!ResolveResult.bSuccess)
	{
		return ResolveResult;
	}

	if (PrimaryKeyColumn.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("PrimaryKeyColumn is not configured"));
	}

	const FString SQL = FString::Printf(
		TEXT("DELETE FROM \"%s\" WHERE \"%s\"=?1"),
		*TableName,
		*PrimaryKeyColumn);

	TArray<FString> Bindings;
	Bindings.Add(RowId);
	return PlayerDatabase->Execute(SQL, Bindings);
}

FESQLQueryResult UESQLTableAsset::DeletePlayerRowById(UESQLPlayerDBComponent* PlayerDBComponent, const FESQLId& SqlId)
{
	return DeletePlayerRowById(PlayerDBComponent, SqlId.Value);
}

FESQLQueryResult UESQLTableAsset::DeletePlayerRowById(UObject* WorldContextObject, const FString& PlayerId, const FESQLId& SqlId)
{
	return DeletePlayerRowById(WorldContextObject, PlayerId, SqlId.Value);
}

FESQLQueryResult UESQLTableAsset::SaveRowFromStruct(
	UObject* WorldContextObject,
	const void* StructData,
	const UScriptStruct* StructType,
	FString* OutResolvedRowId,
	const FString& RowIdOverride)
{
	const FESQLQueryResult InitResult = Initialize(WorldContextObject);
	if (!InitResult.bSuccess)
	{
		return InitResult;
	}

	if (!CachedDatabase || !CachedDatabase->IsOpen())
	{
		return FESQLQueryResult::Failure(TEXT("Database connection is not available"));
	}

	FString SQL;
	TArray<FESQLBindingValue> Bindings;
	FString ResolvedRowId;
	const FESQLQueryResult BuildResult = BuildSaveRowSQL(StructData, StructType, SQL, Bindings, ResolvedRowId, RowIdOverride);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	FESQLQueryResult SaveResult = CachedDatabase->Execute(SQL, Bindings);
	if (SaveResult.bSuccess && OutResolvedRowId)
	{
		*OutResolvedRowId = ResolvedRowId;
	}

	return SaveResult;
}

FESQLQueryResult UESQLTableAsset::SavePlayerRowFromStruct(
	UESQLPlayerDBComponent* PlayerDBComponent,
	const void* StructData,
	const UScriptStruct* StructType,
	FString* OutResolvedRowId,
	const FString& RowIdOverride)
{
	TSharedPtr<FESQLDatabase> PlayerDatabase;
	const FESQLQueryResult ResolveResult = ResolvePlayerDatabaseConnection(PlayerDBComponent, PlayerDatabase);
	if (!ResolveResult.bSuccess)
	{
		return ResolveResult;
	}

	FString SQL;
	TArray<FESQLBindingValue> Bindings;
	FString ResolvedRowId;
	const FESQLQueryResult BuildResult = BuildSaveRowSQL(StructData, StructType, SQL, Bindings, ResolvedRowId, RowIdOverride);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	FESQLQueryResult SaveResult = PlayerDatabase->Execute(SQL, Bindings);
	if (SaveResult.bSuccess && OutResolvedRowId)
	{
		*OutResolvedRowId = ResolvedRowId;
	}

	return SaveResult;
}

FESQLQueryResult UESQLTableAsset::SavePlayerRowFromStruct(
	UObject* WorldContextObject,
	const FString& PlayerId,
	const void* StructData,
	const UScriptStruct* StructType,
	FString* OutResolvedRowId,
	const FString& RowIdOverride)
{
	TSharedPtr<FESQLDatabase> PlayerDatabase;
	const FESQLQueryResult ResolveResult = ResolvePlayerDatabaseConnection(WorldContextObject, PlayerId, PlayerDatabase);
	if (!ResolveResult.bSuccess)
	{
		return ResolveResult;
	}

	FString SQL;
	TArray<FESQLBindingValue> Bindings;
	FString ResolvedRowId;
	const FESQLQueryResult BuildResult = BuildSaveRowSQL(StructData, StructType, SQL, Bindings, ResolvedRowId, RowIdOverride);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	FESQLQueryResult SaveResult = PlayerDatabase->Execute(SQL, Bindings);
	if (SaveResult.bSuccess && OutResolvedRowId)
	{
		*OutResolvedRowId = ResolvedRowId;
	}

	return SaveResult;
}


// ── InsertRowFromStruct (CustomThunk) ────────────────────────────────────────

DEFINE_FUNCTION(UESQLTableAsset::execInsertRowFromStruct)
{
	// Step the struct parameter from the Blueprint VM stack
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* StructData = Stack.MostRecentPropertyAddress;
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_FINISH;

	FESQLQueryResult Result;

	if (!StructData || !StructProp || !StructProp->Struct)
	{
		Result = FESQLQueryResult::Failure(TEXT("Invalid struct data passed to InsertRowFromStruct"));
	}
	else
	{
		UESQLTableAsset* Self = (UESQLTableAsset*)Context;

		if (!Self->bInitialized)
		{
			Result = FESQLQueryResult::Failure(TEXT("SQL Table Asset not initialized. Call Initialize() first."));
		}
		else
		{
			// Extract struct fields to column values
			TMap<FString, FESQLBindingValue> ColumnValues;
			const UScriptStruct* StructType = StructProp->Struct;
			Result = Self->BuildStructColumnBindings(StructData, StructType, ColumnValues);
			if (!Result.bSuccess)
			{
				*(FESQLQueryResult*)RESULT_PARAM = Result;
				return;
			}

			// Generate a RowName if PrimaryKeyColumn is "RowName" and not in struct
			if (Self->PrimaryKeyColumn == TEXT("RowName") && !ColumnValues.Contains(TEXT("RowName")))
			{
				ColumnValues.Add(TEXT("RowName"), FESQLBindingValue::FromText(FGuid::NewGuid().ToString()));
			}

			// Insert via cached database
			if (Self->CachedDatabase && Self->CachedDatabase->IsOpen())
			{
				// Build INSERT SQL
				FString ColList, PlaceholderList;
				TArray<FESQLBindingValue> Bindings;
				int32 Idx = 1;

				for (const auto& Pair : ColumnValues)
				{
					if (Idx > 1) { ColList += TEXT(", "); PlaceholderList += TEXT(", "); }
					ColList += FString::Printf(TEXT("\"%s\""), *Pair.Key);
					PlaceholderList += FString::Printf(TEXT("?%d"), Idx);
					Bindings.Add(Pair.Value);
					++Idx;
				}

				FString SQL = FString::Printf(TEXT("INSERT INTO \"%s\" (%s) VALUES (%s)"),
					*Self->TableName, *ColList, *PlaceholderList);

				Result = Self->CachedDatabase->Execute(SQL, Bindings);
			}
			else
			{
				Result = FESQLQueryResult::Failure(TEXT("Database connection is not available"));
			}
		}
	}

	*(FESQLQueryResult*)RESULT_PARAM = Result;
}


// ── GetAllRows ───────────────────────────────────────────────────────────────

FESQLQueryResult UESQLTableAsset::GetAllRows(int32 MaxRows)
{
	if (!bInitialized || !CachedDatabase || !CachedDatabase->IsOpen())
	{
		return FESQLQueryResult::Failure(TEXT("SQL Table Asset not initialized"));
	}

	FString SQL;
	const FESQLQueryResult BuildResult = BuildGetAllRowsSQL(MaxRows, SQL);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	return CachedDatabase->Execute(SQL);
}

FESQLQueryResult UESQLTableAsset::GetRowById(UObject* WorldContextObject, FESQLId SqlId)
{
	return GetRowById(WorldContextObject, SqlId.Value);
}

FESQLQueryResult UESQLTableAsset::GetRowById(UObject* WorldContextObject, const FString& RowId)
{
	if (RowId.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("RowId cannot be empty"));
	}

	const FESQLQueryResult InitResult = Initialize(WorldContextObject);
	if (!InitResult.bSuccess)
	{
		return InitResult;
	}

	if (!CachedDatabase || !CachedDatabase->IsOpen())
	{
		return FESQLQueryResult::Failure(TEXT("Database connection is not available"));
	}

	FString SQL;
	TArray<FString> Bindings;
	const FESQLQueryResult BuildResult = BuildGetRowByIdSQL(RowId, SQL, Bindings);
	if (!BuildResult.bSuccess)
	{
		return BuildResult;
	}

	return CachedDatabase->Execute(SQL, Bindings);
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

int32 UESQLTableAsset::GetRowCount()
{
	if (!bInitialized || !CachedDatabase || !CachedDatabase->IsOpen())
	{
		return 0;
	}

	FESQLQueryResult Result = CachedDatabase->Execute(
		FString::Printf(TEXT("SELECT COUNT(*) AS RowCount FROM \"%s\""), *TableName));

	if (const FESQLRow* Row = Result.GetFirstRow())
	{
		int64 Count = 0;
		if (Row->TryGetInt64(TEXT("RowCount"), Count))
		{
			return static_cast<int32>(Count);
		}
	}

	return 0;
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

			Database->BeginTransaction();

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

			Database->CommitTransaction();

			UE_LOG(LogExtendedSQL, Log, TEXT("SyncSchema: Recreated table '%s' (dropped %d columns via migration)"),
				*TableName, ColumnsToDrop.Num());
		}
	}

	return FESQLQueryResult::Success();
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
	TSharedPtr<FESQLDatabase> Db = InDatabase ? InDatabase : CachedDatabase;
	if (!Db || !Db->IsOpen())
	{
		OutError = TEXT("Database not open");
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
	TSharedPtr<FESQLDatabase> Db = InDatabase ? InDatabase : CachedDatabase;
	if (!Db || !Db->IsOpen())
	{
		OutError = TEXT("Database not open");
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
	Db->BeginTransaction();

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
			Db->RollbackTransaction();
			OutError = FString::Printf(TEXT("CSV import failed at row %d: %s"), i, *InsertResult.ErrorMessage);
			return false;
		}
	}

	Db->CommitTransaction();
	return true;
}


// ── VCS Pipeline (.sqldump) ──────────────────────────────────────────────────

#include "VCSPipeline/ESQLDumpPipeline.h"

bool UESQLTableAsset::ExportToSQLDump(const FString& DumpFilePath, FString& OutError, TSharedPtr<FESQLDatabase> InDatabase) const
{
	TSharedPtr<FESQLDatabase> Db = InDatabase ? InDatabase : CachedDatabase;
	if (!Db || !Db->IsOpen())
	{
		OutError = TEXT("Database not open");
		return false;
	}

	return FESQLDumpPipeline::ExportDatabaseToDump(Db, DumpFilePath, OutError);
}

bool UESQLTableAsset::ImportFromSQLDump(const FString& DumpFilePath, FString& OutError, TSharedPtr<FESQLDatabase> InDatabase)
{
	TSharedPtr<FESQLDatabase> Db = InDatabase ? InDatabase : CachedDatabase;
	if (!Db || !Db->IsOpen())
	{
		OutError = TEXT("Database not open");
		return false;
	}

	return FESQLDumpPipeline::ImportDumpToDatabase(Db, DumpFilePath, OutError);
}

bool UESQLTableAsset::SyncDatabaseAndDump(FString& OutError)
{
	const FString DbPath = UESQLSettings::ResolveDatabaseFilePath(DatabaseName);
	const FString DumpPath = FESQLDumpPipeline::GetDumpPathForDatabase(DbPath);

	return FESQLDumpPipeline::SyncDatabaseAndDump(DbPath, DumpPath, OutError);
}

FString UESQLTableAsset::GetSQLDumpPath() const
{
	const FString DbPath = UESQLSettings::ResolveDatabaseFilePath(DatabaseName);
	return FESQLDumpPipeline::GetDumpPathForDatabase(DbPath);
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
