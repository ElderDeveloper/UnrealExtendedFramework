// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Private/USqlite/ESQLUsqliteSerializer.h"

#include "Private/Paths/ESQLPathResolver.h"
#include "Private/USqlite/ESQLUsqliteBuilder.h"
#include "Private/USqlite/ESQLUsqliteHash.h"
#include "Private/USqlite/ESQLUsqliteValidator.h"
#include "Core/ESQLDatabase.h"
#include "Core/ESQLStatement.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TableAsset/ESQLTableAsset.h"

namespace
{
FString NormalizeForSave(const FString& InText, const bool bEnsureTrailingNewline = true)
{
	FString Result = InText;
	Result.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
	Result.ReplaceInline(TEXT("\r"), TEXT("\n"));

	if (bEnsureTrailingNewline && !Result.EndsWith(TEXT("\n")))
	{
		Result += TEXT("\n");
	}

	return Result;
}

FString NormalizePrettyJsonForSave(const FString& InText)
{
	const FString Normalized = NormalizeForSave(InText, false);
	FString Result;
	Result.Reserve(Normalized.Len() * 2);

	bool bAtLineStart = true;
	for (int32 Index = 0; Index < Normalized.Len(); ++Index)
	{
		const TCHAR Character = Normalized[Index];
		if (bAtLineStart && Character == TEXT('\t'))
		{
			Result += TEXT("  ");
			continue;
		}

		Result.AppendChar(Character);
		bAtLineStart = Character == TEXT('\n');
	}

	return NormalizeForSave(Result);
}

int64 GetUtf8Size(const FString& Text)
{
	FTCHARToUTF8 Utf8(*Text);
	return Utf8.Length();
}

FString CanonicalizeSQLiteType(const FString& InType)
{
	const FString UpperType = InType.TrimStartAndEnd().ToUpper();
	if (UpperType.Contains(TEXT("INT")))
	{
		return TEXT("INTEGER");
	}

	if (UpperType.Contains(TEXT("REAL")) || UpperType.Contains(TEXT("FLOA")) || UpperType.Contains(TEXT("DOUB")))
	{
		return TEXT("REAL");
	}

	if (UpperType.Contains(TEXT("BLOB")))
	{
		return TEXT("BLOB");
	}

	return TEXT("TEXT");
}

FString MakeMigrationFileName(const int32 Sequence, const FString& Slug)
{
	const FString SafeSlug = Slug.IsEmpty() ? TEXT("migration") : Slug;
	return FString::Printf(TEXT("%04d_%s.sql"), Sequence, *SafeSlug);
}

FString EnsureSqlHasTerminatingSemicolon(const FString& InSql)
{
	FString Result = InSql.TrimStartAndEnd();
	if (!Result.EndsWith(TEXT(";")))
	{
		Result += TEXT(";");
	}
	return Result;
}

FString MakeSchemaPath(const FString& TableName)
{
	return FString::Printf(TEXT("schema/%s.json"), *TableName);
}

FString MakeDataPath(const FString& TableName)
{
	return FString::Printf(TEXT("data/%s.ndjson"), *TableName);
}

bool JsonValueToBindingValue(const TSharedPtr<FJsonValue>& JsonValue, const FString& SQLiteType, FESQLBindingValue& OutValue)
{
	const FString CanonicalType = CanonicalizeSQLiteType(SQLiteType);

	if (!JsonValue.IsValid() || JsonValue->IsNull())
	{
		OutValue = FESQLBindingValue::Null();
		return true;
	}

	if (CanonicalType == TEXT("INTEGER"))
	{
		double NumberValue = 0.0;
		if (JsonValue->TryGetNumber(NumberValue))
		{
			OutValue = FESQLBindingValue::FromInteger(static_cast<int64>(NumberValue));
			return true;
		}

		FString StringValue;
		if (JsonValue->TryGetString(StringValue))
		{
			int64 IntValue = 0;
			if (ESQLTypeParsing::TryParseInt64(StringValue, IntValue))
			{
				OutValue = FESQLBindingValue::FromInteger(IntValue);
				return true;
			}
		}

		return false;
	}

	if (CanonicalType == TEXT("REAL"))
	{
		double NumberValue = 0.0;
		if (JsonValue->TryGetNumber(NumberValue))
		{
			OutValue = FESQLBindingValue::FromFloat(NumberValue);
			return true;
		}

		FString StringValue;
		if (JsonValue->TryGetString(StringValue))
		{
			double FloatValue = 0.0;
			if (ESQLTypeParsing::TryParseDouble(StringValue, FloatValue))
			{
				OutValue = FESQLBindingValue::FromFloat(FloatValue);
				return true;
			}
		}

		return false;
	}

	if (CanonicalType == TEXT("BLOB"))
	{
		const TSharedPtr<FJsonObject> BlobObject = JsonValue->AsObject();
		if (!BlobObject.IsValid() || !BlobObject->HasField(TEXT("$blob")))
		{
			return false;
		}

		FString Base64Value;
		if (!BlobObject->TryGetStringField(TEXT("$blob"), Base64Value))
		{
			return false;
		}

		TArray<uint8> BlobBytes;
		if (!FBase64::Decode(Base64Value, BlobBytes))
		{
			return false;
		}

		OutValue = FESQLBindingValue::FromBlob(MoveTemp(BlobBytes));
		return true;
	}

	FString StringValue;
	if (!JsonValue->TryGetString(StringValue))
	{
		double NumberValue = 0.0;
		if (JsonValue->TryGetNumber(NumberValue))
		{
			StringValue = FString::SanitizeFloat(NumberValue);
		}
		else if (JsonValue->Type == EJson::Boolean)
		{
			StringValue = JsonValue->AsBool() ? TEXT("true") : TEXT("false");
		}
		else
		{
			return false;
		}
	}

	OutValue = FESQLBindingValue::FromText(MoveTemp(StringValue));
	return true;
}

FString BuildProjectJson(const FESQLUsqliteProject& Project)
{
	TArray<FESQLUsqliteTableSchema> Tables = Project.Tables;
	Tables.Sort([](const FESQLUsqliteTableSchema& A, const FESQLUsqliteTableSchema& B)
	{
		return A.TableName < B.TableName;
	});

	TArray<FESQLUsqliteMigration> Migrations = Project.Migrations;
	Migrations.Sort([](const FESQLUsqliteMigration& A, const FESQLUsqliteMigration& B)
	{
		return (A.Sequence == B.Sequence) ? (A.RelativePath < B.RelativePath) : (A.Sequence < B.Sequence);
	});

	FString Output;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("formatVersion"), Project.FormatVersion);
	Writer->WriteValue(TEXT("projectName"), Project.ProjectName);
	Writer->WriteValue(TEXT("hashAlgorithm"), ESQLUsqliteHash::GetDefaultAlgorithm());

	Writer->WriteArrayStart(TEXT("tables"));
	for (const FESQLUsqliteTableSchema& Table : Tables)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("name"), Table.TableName);
		Writer->WriteValue(TEXT("schemaPath"), MakeSchemaPath(Table.TableName));
		Writer->WriteValue(TEXT("dataPath"), MakeDataPath(Table.TableName));
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteArrayStart(TEXT("migrations"));
	for (const FESQLUsqliteMigration& Migration : Migrations)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("sequence"), Migration.Sequence);
		Writer->WriteValue(TEXT("slug"), Migration.Slug);
		Writer->WriteValue(TEXT("path"), Migration.RelativePath);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteObjectEnd();
	Writer->Close();

	return NormalizePrettyJsonForSave(Output);
}

FString BuildSchemaJson(const FESQLUsqliteTableSchema& Table)
{
	FString Output;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("tableName"), Table.TableName);
	Writer->WriteValue(TEXT("primaryKey"), Table.PrimaryKeyColumn);
	Writer->WriteValue(TEXT("rowStructPath"), Table.RowStructPath);
	Writer->WriteValue(TEXT("labelColumn"), Table.LabelColumn);

	Writer->WriteArrayStart(TEXT("columns"));
	for (const FESQLUsqliteColumnSchema& Column : Table.Columns)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("name"), Column.Name);
		Writer->WriteValue(TEXT("type"), Column.SQLiteType);
		Writer->WriteValue(TEXT("nullable"), Column.bNullable);
		Writer->WriteValue(TEXT("primaryKey"), Column.bPrimaryKey);
		if (!Column.DefaultValue.IsEmpty())
		{
			Writer->WriteValue(TEXT("defaultValue"), Column.DefaultValue);
		}
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteObjectEnd();
	Writer->Close();

	return NormalizePrettyJsonForSave(Output);
}

template <typename PrintPolicy>
void WriteBindingValueJson(const TSharedRef<TJsonWriter<TCHAR, PrintPolicy>>& Writer, const FESQLBindingValue& Value)
{
	if (Value.IsNull())
	{
		Writer->WriteNull();
		return;
	}

	switch (Value.ValueType)
	{
	case EESQLColumnType::Integer:
		Writer->WriteValue(static_cast<double>(Value.IntegerValue));
		return;

	case EESQLColumnType::Float:
		Writer->WriteValue(Value.FloatValue);
		return;

	case EESQLColumnType::Blob:
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("$blob"), FBase64::Encode(Value.BlobValue));
			Writer->WriteObjectEnd();
			return;
		}

	case EESQLColumnType::Text:
	default:
		Writer->WriteValue(Value.TextValue);
		return;
	}
}

FString MakeSortKey(const FESQLBindingValue* Value)
{
	if (!Value || Value->IsNull())
	{
		return TEXT("~null");
	}

	switch (Value->ValueType)
	{
	case EESQLColumnType::Integer:
		return FString::Printf(TEXT("i:%020lld"), Value->IntegerValue);

	case EESQLColumnType::Float:
		return FString::Printf(TEXT("f:%024.12f"), Value->FloatValue);

	case EESQLColumnType::Blob:
		return TEXT("b:") + FBase64::Encode(Value->BlobValue);

	case EESQLColumnType::Text:
	default:
		return TEXT("t:") + Value->TextValue;
	}
}

FString BuildTableDataNdjson(const FESQLUsqliteTableSchema& Table, const TArray<FESQLUsqliteRow>& Rows)
{
	TArray<FESQLUsqliteRow> SortedRows = Rows;
	const FString SortColumn = Table.PrimaryKeyColumn.IsEmpty() && Table.Columns.Num() > 0 ? Table.Columns[0].Name : Table.PrimaryKeyColumn;

	SortedRows.Sort([&SortColumn](const FESQLUsqliteRow& A, const FESQLUsqliteRow& B)
	{
		const FESQLBindingValue* LeftValue = A.FindValue(SortColumn);
		const FESQLBindingValue* RightValue = B.FindValue(SortColumn);
		return MakeSortKey(LeftValue) < MakeSortKey(RightValue);
	});

	FString Output;
	for (const FESQLUsqliteRow& Row : SortedRows)
	{
		FString Line;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
		Writer->WriteObjectStart();
		for (const FESQLUsqliteColumnSchema& Column : Table.Columns)
		{
			const FESQLBindingValue* Value = Row.FindValue(Column.Name);
			Writer->WriteIdentifierPrefix(Column.Name);
			WriteBindingValueJson(Writer, Value ? *Value : FESQLBindingValue::Null());
		}
		Writer->WriteObjectEnd();
		Writer->Close();

		Output += NormalizeForSave(Line);
	}

	return Output;
}

FString BuildLockJson(const FESQLUsqliteLock& Lock)
{
	FString Output;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("hashAlgorithm"), Lock.HashAlgorithm);
	Writer->WriteValue(TEXT("projectHash"), Lock.ProjectHash);
	Writer->WriteValue(TEXT("highestMigrationSequence"), Lock.HighestMigrationSequence);

	Writer->WriteArrayStart(TEXT("files"));
	for (const FESQLUsqliteFileHash& File : Lock.Files)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("path"), File.RelativePath);
		Writer->WriteValue(TEXT("hash"), File.Hash);
		Writer->WriteValue(TEXT("sizeBytes"), static_cast<double>(File.SizeBytes));
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteObjectEnd();
	Writer->Close();

	return NormalizePrettyJsonForSave(Output);
}

bool BuildGeneratedFiles(const FESQLUsqliteProject& Project, TMap<FString, FString>& OutFiles, FString& OutError)
{
	OutFiles.Empty();
	OutFiles.Add(TEXT("project.json"), BuildProjectJson(Project));

	for (const FESQLUsqliteTableSchema& Table : Project.Tables)
	{
		OutFiles.Add(MakeSchemaPath(Table.TableName), BuildSchemaJson(Table));

		const TArray<FESQLUsqliteRow>* TableRows = Project.Data.Find(Table.TableName);
		OutFiles.Add(MakeDataPath(Table.TableName), BuildTableDataNdjson(Table, TableRows ? *TableRows : TArray<FESQLUsqliteRow>()));
	}

	for (const FESQLUsqliteMigration& Migration : Project.Migrations)
	{
		if (Migration.RelativePath.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Migration %d is missing a relative path."), Migration.Sequence);
			return false;
		}

		OutFiles.Add(Migration.RelativePath, NormalizeForSave(Migration.Sql));
	}

	return true;
}

bool BuildLockFromFiles(const FESQLUsqliteProject& Project, const TMap<FString, FString>& GeneratedFiles, FESQLUsqliteLock& OutLock, FString& OutError)
{
	OutLock = FESQLUsqliteLock();
	OutLock.HashAlgorithm = ESQLUsqliteHash::GetDefaultAlgorithm();
	OutLock.HighestMigrationSequence = Project.GetHighestMigrationSequence();

	TArray<FString> RelativePaths;
	GeneratedFiles.GetKeys(RelativePaths);
	RelativePaths.Sort();

	FString Aggregate;
	for (const FString& RelativePath : RelativePaths)
	{
		const FString& Content = GeneratedFiles[RelativePath];
		FString Hash;
		if (!ESQLUsqliteHash::ComputeTextHash(Content, OutLock.HashAlgorithm, Hash))
		{
			OutError = FString::Printf(TEXT("Failed to compute %s hash for %s."), *OutLock.HashAlgorithm, *RelativePath);
			return false;
		}

		const int64 SizeBytes = GetUtf8Size(Content);
		OutLock.Files.Add({ RelativePath, Hash, SizeBytes });
		Aggregate += RelativePath + TEXT("|") + Hash + TEXT("|") + FString::Printf(TEXT("%lld"), SizeBytes) + TEXT("\n");
	}

	if (!ESQLUsqliteHash::ComputeTextHash(Aggregate, OutLock.HashAlgorithm, OutLock.ProjectHash))
	{
		OutError = FString::Printf(TEXT("Failed to compute %s project hash."), *OutLock.HashAlgorithm);
		return false;
	}

	return true;
}

bool SaveManagedFile(const FString& AbsolutePath, const FString& Content)
{
	const FString Directory = FPaths::GetPath(AbsolutePath);
	IFileManager::Get().MakeDirectory(*Directory, true);
	return FFileHelper::SaveStringToFile(Content, *AbsolutePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

FString EnsureCreateSqlIsIdempotent(const FString& InSql)
{
	FString Sql = InSql;
	if (!Sql.Contains(TEXT("IF NOT EXISTS"), ESearchCase::IgnoreCase))
	{
		Sql = Sql.Replace(TEXT("CREATE TABLE"), TEXT("CREATE TABLE IF NOT EXISTS"));
		Sql = Sql.Replace(TEXT("CREATE INDEX"), TEXT("CREATE INDEX IF NOT EXISTS"));
		Sql = Sql.Replace(TEXT("CREATE UNIQUE INDEX"), TEXT("CREATE UNIQUE INDEX IF NOT EXISTS"));
	}
	return EnsureSqlHasTerminatingSemicolon(Sql);
}

bool ExtractRowsForTable(TSharedPtr<FESQLDatabase> Database, const FESQLUsqliteTableSchema& Table, TArray<FESQLUsqliteRow>& OutRows, FString& OutError)
{
	const FString SortColumn = Table.PrimaryKeyColumn.IsEmpty() && Table.Columns.Num() > 0 ? Table.Columns[0].Name : Table.PrimaryKeyColumn;
	FString Sql = FString::Printf(TEXT("SELECT * FROM \"%s\""), *Table.TableName);
	if (!SortColumn.IsEmpty())
	{
		Sql += FString::Printf(TEXT(" ORDER BY \"%s\""), *SortColumn);
	}

	const FESQLStatementPrepareResult PrepareResult = Database->Prepare(Sql);
	if (!PrepareResult)
	{
		OutError = PrepareResult.GetErrorMessage();
		return false;
	}
	TSharedPtr<FESQLStatement> Statement = PrepareResult.GetValue();

	while (Statement->Step())
	{
		FESQLUsqliteRow Row;
		for (int32 ColumnIndex = 0; ColumnIndex < Statement->GetColumnCount(); ++ColumnIndex)
		{
			const FString ColumnName = Statement->GetColumnName(ColumnIndex);
			switch (Statement->GetColumnType(ColumnIndex))
			{
			case EESQLColumnType::Null:
				Row.Values.Add(ColumnName, FESQLBindingValue::Null());
				break;

			case EESQLColumnType::Integer:
				Row.Values.Add(ColumnName, FESQLBindingValue::FromInteger(Statement->GetColumnInt(ColumnIndex)));
				break;

			case EESQLColumnType::Float:
				Row.Values.Add(ColumnName, FESQLBindingValue::FromFloat(Statement->GetColumnFloat(ColumnIndex)));
				break;

			case EESQLColumnType::Blob:
				Row.Values.Add(ColumnName, FESQLBindingValue::FromBlob(Statement->GetColumnBlob(ColumnIndex)));
				break;

			case EESQLColumnType::Text:
			default:
				Row.Values.Add(ColumnName, FESQLBindingValue::FromText(Statement->GetColumnText(ColumnIndex)));
				break;
			}
		}

		OutRows.Add(MoveTemp(Row));
	}

	if (!Statement->IsDone())
	{
		OutError = TEXT("Failed while reading table rows for .usqlite export.");
		return false;
	}

	return true;
}

bool LoadJsonObjectFile(const FString& AbsolutePath, TSharedPtr<FJsonObject>& OutObject, FString& OutError, FString* OutFileContent = nullptr)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *AbsolutePath))
	{
		OutError = FString::Printf(TEXT("Failed to read %s"), *AbsolutePath);
		return false;
	}

	if (OutFileContent)
	{
		*OutFileContent = NormalizeForSave(Content);
	}

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
	{
		OutError = FString::Printf(TEXT("Failed to parse JSON file %s"), *AbsolutePath);
		return false;
	}

	return true;
}

bool LoadSchemaFile(const FString& AbsolutePath, FESQLUsqliteTableSchema& OutTable, FString& OutError, FString* OutFileContent = nullptr)
{
	TSharedPtr<FJsonObject> RootObject;
	if (!LoadJsonObjectFile(AbsolutePath, RootObject, OutError, OutFileContent))
	{
		return false;
	}

	OutTable.TableName = RootObject->GetStringField(TEXT("tableName"));
	OutTable.PrimaryKeyColumn = RootObject->GetStringField(TEXT("primaryKey"));
	RootObject->TryGetStringField(TEXT("rowStructPath"), OutTable.RowStructPath);
	RootObject->TryGetStringField(TEXT("labelColumn"), OutTable.LabelColumn);

	const TArray<TSharedPtr<FJsonValue>>* Columns = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("columns"), Columns) || !Columns)
	{
		OutError = FString::Printf(TEXT("Schema file %s is missing a columns array."), *AbsolutePath);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& ColumnValue : *Columns)
	{
		const TSharedPtr<FJsonObject> ColumnObject = ColumnValue.IsValid() ? ColumnValue->AsObject() : nullptr;
		if (!ColumnObject.IsValid())
		{
			OutError = FString::Printf(TEXT("Schema file %s contains an invalid column entry."), *AbsolutePath);
			return false;
		}

		FESQLUsqliteColumnSchema Column;
		Column.Name = ColumnObject->GetStringField(TEXT("name"));
		Column.SQLiteType = CanonicalizeSQLiteType(ColumnObject->GetStringField(TEXT("type")));
		Column.bNullable = ColumnObject->GetBoolField(TEXT("nullable"));
		Column.bPrimaryKey = ColumnObject->GetBoolField(TEXT("primaryKey"));
		ColumnObject->TryGetStringField(TEXT("defaultValue"), Column.DefaultValue);
		OutTable.Columns.Add(MoveTemp(Column));
	}

	return true;
}

bool LoadLockFile(const FString& AbsolutePath, FESQLUsqliteLock& OutLock, FString& OutError)
{
	if (!IFileManager::Get().FileExists(*AbsolutePath))
	{
		return true;
	}

	TSharedPtr<FJsonObject> RootObject;
	if (!LoadJsonObjectFile(AbsolutePath, RootObject, OutError))
	{
		return false;
	}

	RootObject->TryGetStringField(TEXT("hashAlgorithm"), OutLock.HashAlgorithm);
	RootObject->TryGetStringField(TEXT("projectHash"), OutLock.ProjectHash);
	OutLock.HighestMigrationSequence = RootObject->GetIntegerField(TEXT("highestMigrationSequence"));

	const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
	if (RootObject->TryGetArrayField(TEXT("files"), Files) && Files)
	{
		for (const TSharedPtr<FJsonValue>& FileValue : *Files)
		{
			const TSharedPtr<FJsonObject> FileObject = FileValue.IsValid() ? FileValue->AsObject() : nullptr;
			if (!FileObject.IsValid())
			{
				continue;
			}

			FESQLUsqliteFileHash FileHash;
			FileHash.RelativePath = FileObject->GetStringField(TEXT("path"));
			FileHash.Hash = FileObject->GetStringField(TEXT("hash"));
			FileHash.SizeBytes = static_cast<int64>(FileObject->GetNumberField(TEXT("sizeBytes")));
			OutLock.Files.Add(MoveTemp(FileHash));
		}
	}

	return true;
}
}

bool FESQLUsqliteSerializer::IsProjectRootPath(const FString& Path)
{
	FString NormalizedPath = Path;
	NormalizedPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	NormalizedPath.RemoveFromEnd(TEXT("/"));
	return NormalizedPath.EndsWith(TEXT(".usqlite"), ESearchCase::IgnoreCase);
}

FString FESQLUsqliteSerializer::GetProjectRootForDatabasePath(const FString& DatabasePath)
{
	const FString AbsoluteDatabasePath = FESQLPathResolver::ResolveAbsolutePath(DatabasePath);
	FString ProjectRoot = FPaths::ChangeExtension(AbsoluteDatabasePath, TEXT(".usqlite"));
	FPaths::NormalizeFilename(ProjectRoot);
	return ProjectRoot;
}

FString FESQLUsqliteSerializer::GetDerivedDatabasePath(const FString& ProjectRoot, const FString& ProjectHash)
{
	FString CacheDirectory = FPaths::Combine(FESQLPathResolver::ResolveAbsolutePath(ProjectRoot), TEXT(".derived"));
	FPaths::NormalizeFilename(CacheDirectory);
	return FPaths::Combine(CacheDirectory, ProjectHash + TEXT(".db"));
}

bool FESQLUsqliteSerializer::LoadProject(const FString& ProjectRoot, FESQLUsqliteProject& OutProject, FString& OutError)
{
	OutProject = FESQLUsqliteProject();
	TMap<FString, FString> LoadedFiles;

	const FString AbsoluteProjectRoot = FESQLPathResolver::ResolveAbsolutePath(ProjectRoot);
	const FString ProjectFilePath = FPaths::Combine(AbsoluteProjectRoot, TEXT("project.json"));

	TSharedPtr<FJsonObject> RootObject;
	FString ProjectFileContent;
	if (!LoadJsonObjectFile(ProjectFilePath, RootObject, OutError, &ProjectFileContent))
	{
		return false;
	}
	LoadedFiles.Add(TEXT("project.json"), ProjectFileContent);

	OutProject.FormatVersion = RootObject->GetIntegerField(TEXT("formatVersion"));
	OutProject.ProjectName = RootObject->GetStringField(TEXT("projectName"));

	const TArray<TSharedPtr<FJsonValue>>* Tables = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("tables"), Tables) || !Tables)
	{
		OutError = TEXT("project.json is missing the tables array.");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& TableValue : *Tables)
	{
		const TSharedPtr<FJsonObject> TableObject = TableValue.IsValid() ? TableValue->AsObject() : nullptr;
		if (!TableObject.IsValid())
		{
			OutError = TEXT("project.json contains an invalid table entry.");
			return false;
		}

		const FString TableName = TableObject->GetStringField(TEXT("name"));
		const FString SchemaRelativePath = TableObject->GetStringField(TEXT("schemaPath"));
		const FString DataRelativePath = TableObject->GetStringField(TEXT("dataPath"));
		const FString SchemaPath = FPaths::Combine(AbsoluteProjectRoot, SchemaRelativePath);
		const FString DataPath = FPaths::Combine(AbsoluteProjectRoot, DataRelativePath);

		FESQLUsqliteTableSchema Table;
		FString SchemaFileContent;
		if (!LoadSchemaFile(SchemaPath, Table, OutError, &SchemaFileContent))
		{
			return false;
		}
		LoadedFiles.Add(SchemaRelativePath, SchemaFileContent);

		if (Table.TableName != TableName)
		{
			OutError = FString::Printf(TEXT("Table manifest entry '%s' does not match schema table '%s'."), *TableName, *Table.TableName);
			return false;
		}

		OutProject.Tables.Add(Table);

		TArray<FESQLUsqliteRow> TableRows;
		FString DataContent;
		if (IFileManager::Get().FileExists(*DataPath))
		{
			if (!FFileHelper::LoadFileToString(DataContent, *DataPath))
			{
				OutError = FString::Printf(TEXT("Failed to read %s"), *DataPath);
				return false;
			}

			LoadedFiles.Add(DataRelativePath, NormalizeForSave(DataContent, !DataContent.IsEmpty()));

			TArray<FString> Lines;
			DataContent.ParseIntoArrayLines(Lines, false);
			for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
			{
				const FString TrimmedLine = Lines[LineIndex].TrimStartAndEnd();
				if (TrimmedLine.IsEmpty())
				{
					continue;
				}

				TSharedPtr<FJsonValue> JsonValue;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TrimmedLine);
				if (!FJsonSerializer::Deserialize(Reader, JsonValue) || !JsonValue.IsValid() || JsonValue->Type != EJson::Object)
				{
					OutError = FString::Printf(TEXT("Failed to parse %s line %d as JSON object."), *DataPath, LineIndex + 1);
					return false;
				}

				const TSharedPtr<FJsonObject> RowObject = JsonValue->AsObject();
				FESQLUsqliteRow Row;
				for (const FESQLUsqliteColumnSchema& Column : Table.Columns)
				{
					const TSharedPtr<FJsonValue>* FoundJson = RowObject->Values.Find(Column.Name);
					FESQLBindingValue BindingValue;
					if (!FoundJson || !JsonValueToBindingValue(*FoundJson, Column.SQLiteType, BindingValue))
					{
						OutError = FString::Printf(TEXT("Failed to parse column '%s' in %s line %d."), *Column.Name, *DataPath, LineIndex + 1);
						return false;
					}
					Row.Values.Add(Column.Name, MoveTemp(BindingValue));
				}

				TableRows.Add(MoveTemp(Row));
			}
		}

		OutProject.Data.Add(Table.TableName, MoveTemp(TableRows));
	}

	const TArray<TSharedPtr<FJsonValue>>* Migrations = nullptr;
	if (RootObject->TryGetArrayField(TEXT("migrations"), Migrations) && Migrations)
	{
		for (const TSharedPtr<FJsonValue>& MigrationValue : *Migrations)
		{
			const TSharedPtr<FJsonObject> MigrationObject = MigrationValue.IsValid() ? MigrationValue->AsObject() : nullptr;
			if (!MigrationObject.IsValid())
			{
				OutError = TEXT("project.json contains an invalid migration entry.");
				return false;
			}

			FESQLUsqliteMigration Migration;
			Migration.Sequence = MigrationObject->GetIntegerField(TEXT("sequence"));
			Migration.Slug = MigrationObject->GetStringField(TEXT("slug"));
			Migration.RelativePath = MigrationObject->GetStringField(TEXT("path"));
			Migration.FileName = FPaths::GetCleanFilename(Migration.RelativePath);

			const FString AbsoluteMigrationPath = FPaths::Combine(AbsoluteProjectRoot, Migration.RelativePath);
			if (!FFileHelper::LoadFileToString(Migration.Sql, *AbsoluteMigrationPath))
			{
				OutError = FString::Printf(TEXT("Failed to read migration file %s."), *AbsoluteMigrationPath);
				return false;
			}

			Migration.Sql = NormalizeForSave(Migration.Sql);
			LoadedFiles.Add(Migration.RelativePath, Migration.Sql);
			OutProject.Migrations.Add(MoveTemp(Migration));
		}
	}

	const FString LockPath = FPaths::Combine(AbsoluteProjectRoot, TEXT(".usqlite-lock.json"));
	if (!LoadLockFile(LockPath, OutProject.Lock, OutError))
	{
		return false;
	}

	const FESQLUsqliteValidationResult ValidationResult = FESQLUsqliteValidator::ValidateAgainstLock(OutProject, LoadedFiles);
	if (!ValidationResult.bSuccess)
	{
		OutError = ValidationResult.ToErrorString();
		return false;
	}

	TMap<FString, FString> CanonicalGeneratedFiles;
	if (!BuildGeneratedFiles(OutProject, CanonicalGeneratedFiles, OutError))
	{
		return false;
	}

	FESQLUsqliteLock CanonicalLock;
	if (!BuildLockFromFiles(OutProject, CanonicalGeneratedFiles, CanonicalLock, OutError))
	{
		return false;
	}

	OutProject.Lock = MoveTemp(CanonicalLock);

	return true;
}

bool FESQLUsqliteSerializer::SaveProject(const FESQLUsqliteProject& Project, const FString& ProjectRoot, FString& OutError)
{
	const FString AbsoluteProjectRoot = FESQLPathResolver::ResolveAbsolutePath(ProjectRoot);

	TMap<FString, FString> GeneratedFiles;
	if (!BuildGeneratedFiles(Project, GeneratedFiles, OutError))
	{
		return false;
	}

	const FESQLUsqliteValidationResult ValidationResult = FESQLUsqliteValidator::ValidateForSave(Project, GeneratedFiles);
	if (!ValidationResult.bSuccess)
	{
		OutError = ValidationResult.ToErrorString();
		return false;
	}

	FESQLUsqliteLock Lock;
	if (!BuildLockFromFiles(Project, GeneratedFiles, Lock, OutError))
	{
		return false;
	}

	GeneratedFiles.Add(TEXT(".usqlite-lock.json"), BuildLockJson(Lock));

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.DeleteDirectoryRecursively(*FPaths::Combine(AbsoluteProjectRoot, TEXT("schema")));
	PlatformFile.DeleteDirectoryRecursively(*FPaths::Combine(AbsoluteProjectRoot, TEXT("data")));
	PlatformFile.DeleteDirectoryRecursively(*FPaths::Combine(AbsoluteProjectRoot, TEXT("migrations")));

	IFileManager::Get().MakeDirectory(*AbsoluteProjectRoot, true);

	TArray<FString> RelativePaths;
	GeneratedFiles.GetKeys(RelativePaths);
	RelativePaths.Sort();

	for (const FString& RelativePath : RelativePaths)
	{
		const FString AbsolutePath = FPaths::Combine(AbsoluteProjectRoot, RelativePath);
		if (!SaveManagedFile(AbsolutePath, GeneratedFiles[RelativePath]))
		{
			OutError = FString::Printf(TEXT("Failed to write %s"), *AbsolutePath);
			return false;
		}
	}

	return true;
}

bool FESQLUsqliteSerializer::ComputeProjectHash(const FESQLUsqliteProject& Project, FString& OutHash, FString& OutError)
{
	TMap<FString, FString> GeneratedFiles;
	if (!BuildGeneratedFiles(Project, GeneratedFiles, OutError))
	{
		return false;
	}

	FESQLUsqliteLock Lock;
	if (!BuildLockFromFiles(Project, GeneratedFiles, Lock, OutError))
	{
		return false;
	}

	OutHash = Lock.ProjectHash;
	return true;
}

bool FESQLUsqliteSerializer::ExportDatabaseToProject(
	TSharedPtr<FESQLDatabase> Database,
	const FString& ProjectRoot,
	FString& OutError,
	const UESQLTableAsset* MetadataAsset)
{
	if (!Database || !Database->IsOpen())
	{
		OutError = TEXT("Database is not open.");
		return false;
	}

	const FString AbsoluteProjectRoot = FESQLPathResolver::ResolveAbsolutePath(ProjectRoot);

	FESQLUsqliteProject ExistingProject;
	const bool bProjectExists = IFileManager::Get().DirectoryExists(*AbsoluteProjectRoot) && IFileManager::Get().FileExists(*FPaths::Combine(AbsoluteProjectRoot, TEXT("project.json")));
	if (bProjectExists && !LoadProject(AbsoluteProjectRoot, ExistingProject, OutError))
	{
		return false;
	}

	FESQLUsqliteProject Project;
	Project.ProjectName = bProjectExists ? ExistingProject.ProjectName : FPaths::GetBaseFilename(AbsoluteProjectRoot);
	Project.Migrations = ExistingProject.Migrations;
	Project.Lock = ExistingProject.Lock;

	const FESQLQueryResult TablesResult = Database->Execute(
		TEXT("SELECT name, sql FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' AND name != '_esql_schema_version' ORDER BY name"));

	if (!TablesResult.bSuccess)
	{
		OutError = TablesResult.ErrorMessage;
		return false;
	}

	FString InitialMigrationSql;
	for (const FESQLRow& TableRow : TablesResult.Rows)
	{
		FString TableName;
		FString CreateSql;
		if (!TableRow.TryGetString(TEXT("name"), TableName) || !TableRow.TryGetString(TEXT("sql"), CreateSql))
		{
			continue;
		}

		FESQLQueryResult ColumnsResult = Database->Execute(FString::Printf(TEXT("PRAGMA table_info(\"%s\")"), *TableName));
		if (!ColumnsResult.bSuccess)
		{
			OutError = ColumnsResult.ErrorMessage;
			return false;
		}

		FESQLUsqliteTableSchema Table;
		Table.TableName = TableName;
		if (MetadataAsset && MetadataAsset->TableName == TableName)
		{
			Table.PrimaryKeyColumn = MetadataAsset->PrimaryKeyColumn;
			Table.LabelColumn = MetadataAsset->DefaultLabelColumn;
			Table.RowStructPath = MetadataAsset->RowStruct ? MetadataAsset->RowStruct->GetPathName() : FString();
		}

		for (const FESQLRow& ColumnRow : ColumnsResult.Rows)
		{
			FESQLUsqliteColumnSchema Column;
			ColumnRow.TryGetString(TEXT("name"), Column.Name);
			FString RawType;
			ColumnRow.TryGetString(TEXT("type"), RawType);
			Column.SQLiteType = CanonicalizeSQLiteType(RawType);

			int64 NotNullValue = 0;
			ColumnRow.TryGetInt64(TEXT("notnull"), NotNullValue);
			Column.bNullable = NotNullValue == 0;

			int64 PkValue = 0;
			ColumnRow.TryGetInt64(TEXT("pk"), PkValue);
			Column.bPrimaryKey = PkValue > 0;
			if (Column.bPrimaryKey && Table.PrimaryKeyColumn.IsEmpty())
			{
				Table.PrimaryKeyColumn = Column.Name;
			}

			ColumnRow.TryGetString(TEXT("dflt_value"), Column.DefaultValue);
			Table.Columns.Add(MoveTemp(Column));
		}

		Project.Tables.Add(Table);

		TArray<FESQLUsqliteRow> TableRows;
		if (!ExtractRowsForTable(Database, Table, TableRows, OutError))
		{
			return false;
		}

		Project.Data.Add(Table.TableName, MoveTemp(TableRows));
		InitialMigrationSql += EnsureCreateSqlIsIdempotent(CreateSql) + TEXT("\n\n");
	}

	if (Project.Migrations.Num() == 0)
	{
		const FESQLQueryResult IndexResult = Database->Execute(
			TEXT("SELECT sql FROM sqlite_master WHERE type='index' AND sql IS NOT NULL ORDER BY name"));
		if (!IndexResult.bSuccess)
		{
			OutError = IndexResult.ErrorMessage;
			return false;
		}

		for (const FESQLRow& IndexRow : IndexResult.Rows)
		{
			FString IndexSql;
			if (IndexRow.TryGetString(TEXT("sql"), IndexSql) && !IndexSql.IsEmpty())
			{
				InitialMigrationSql += EnsureCreateSqlIsIdempotent(IndexSql) + TEXT("\n");
			}
		}

		FESQLUsqliteMigration InitialMigration;
		InitialMigration.Sequence = 1;
		InitialMigration.Slug = TEXT("initial");
		InitialMigration.FileName = MakeMigrationFileName(InitialMigration.Sequence, InitialMigration.Slug);
		InitialMigration.RelativePath = FPaths::Combine(TEXT("migrations"), InitialMigration.FileName);
		InitialMigration.Sql = NormalizeForSave(InitialMigrationSql);
		Project.Migrations.Add(MoveTemp(InitialMigration));
	}

	return SaveProject(Project, AbsoluteProjectRoot, OutError);
}

bool FESQLUsqliteSerializer::SyncDatabaseAndProject(
	const FString& DatabasePath,
	const FString& ProjectRoot,
	FString& OutError,
	const UESQLTableAsset* MetadataAsset)
{
	const FString AbsoluteDatabasePath = FESQLPathResolver::ResolveAbsolutePath(DatabasePath);
	const FString AbsoluteProjectRoot = FESQLPathResolver::ResolveAbsolutePath(ProjectRoot);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const bool bDatabaseExists = PlatformFile.FileExists(*AbsoluteDatabasePath);
	const bool bProjectExists = PlatformFile.DirectoryExists(*AbsoluteProjectRoot)
		&& PlatformFile.FileExists(*FPaths::Combine(AbsoluteProjectRoot, TEXT("project.json")));

	if (bProjectExists)
	{
		FString BuiltPath;
		return FESQLUsqliteBuilder::BuildProjectRoot(AbsoluteProjectRoot, AbsoluteDatabasePath, BuiltPath, OutError);
	}

	if (!bDatabaseExists)
	{
		return true;
	}

	const FESQLDatabaseOpenResult OpenResult = FESQLDatabase::Open(AbsoluteDatabasePath);
	if (!OpenResult)
	{
		OutError = OpenResult.GetErrorMessage();
		return false;
	}
	TSharedPtr<FESQLDatabase> Database = OpenResult.GetValue();

	const bool bResult = ExportDatabaseToProject(Database, AbsoluteProjectRoot, OutError, MetadataAsset);
	Database->Close();
	return bResult;
}