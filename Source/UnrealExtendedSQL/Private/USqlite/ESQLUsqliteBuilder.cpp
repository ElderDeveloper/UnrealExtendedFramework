// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Private/USqlite/ESQLUsqliteBuilder.h"

#include "Private/Paths/ESQLPathResolver.h"
#include "Private/USqlite/ESQLUsqliteSerializer.h"
#include "Private/USqlite/ESQLUsqliteValidator.h"
#include "Core/ESQLDatabase.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Base64.h"

namespace
{
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

TArray<FString> ParseSqlStatements(const FString& SqlContent)
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

bool DeleteExistingDatabaseArtifacts(const FString& DatabasePath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.FileExists(*DatabasePath))
	{
		PlatformFile.DeleteFile(*DatabasePath);
	}

	PlatformFile.DeleteFile(*(DatabasePath + TEXT("-wal")));
	PlatformFile.DeleteFile(*(DatabasePath + TEXT("-shm")));
	return true;
}

bool DeleteDatabaseSidecars(const FString& DatabasePath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.DeleteFile(*(DatabasePath + TEXT("-wal")));
	PlatformFile.DeleteFile(*(DatabasePath + TEXT("-shm")));
	return true;
}

bool CanReuseDerivedDatabase(const FString& DatabasePath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	return PlatformFile.FileExists(*DatabasePath)
		&& !PlatformFile.FileExists(*(DatabasePath + TEXT("-wal")))
		&& !PlatformFile.FileExists(*(DatabasePath + TEXT("-shm")));
}

bool EnsureSchemaVersionTable(TSharedPtr<FESQLDatabase> Database, FString& OutError)
{
	const FESQLQueryResult Result = Database->Execute(
		TEXT("CREATE TABLE IF NOT EXISTS \"_esql_schema_version\" (\"Sequence\" INTEGER NOT NULL, \"Migration\" TEXT NOT NULL)"));

	if (!Result.bSuccess)
	{
		OutError = Result.ErrorMessage;
		return false;
	}

	const FESQLQueryResult ClearResult = Database->Execute(TEXT("DELETE FROM \"_esql_schema_version\""));
	if (!ClearResult.bSuccess)
	{
		OutError = ClearResult.ErrorMessage;
		return false;
	}

	return true;
}

bool UpdateSchemaVersion(TSharedPtr<FESQLDatabase> Database, const FESQLUsqliteMigration& Migration, FString& OutError)
{
	const TArray<FESQLBindingValue> Bindings = {
		FESQLBindingValue::FromInteger(Migration.Sequence),
		FESQLBindingValue::FromText(Migration.FileName.IsEmpty() ? Migration.RelativePath : Migration.FileName)
	};

	const FESQLQueryResult Result = Database->Execute(
		TEXT("INSERT INTO \"_esql_schema_version\" (\"Sequence\", \"Migration\") VALUES (?1, ?2)"),
		Bindings);

	if (!Result.bSuccess)
	{
		OutError = Result.ErrorMessage;
		return false;
	}

	return true;
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

bool InsertTableRows(TSharedPtr<FESQLDatabase> Database, const FESQLUsqliteTableSchema& Table, const TArray<FESQLUsqliteRow>& Rows, FString& OutError)
{
	if (Rows.Num() == 0)
	{
		return true;
	}

	FString ColumnList;
	FString PlaceholderList;
	for (int32 ColumnIndex = 0; ColumnIndex < Table.Columns.Num(); ++ColumnIndex)
	{
		if (ColumnIndex > 0)
		{
			ColumnList += TEXT(", ");
			PlaceholderList += TEXT(", ");
		}

		ColumnList += FString::Printf(TEXT("\"%s\""), *Table.Columns[ColumnIndex].Name);
		PlaceholderList += FString::Printf(TEXT("?%d"), ColumnIndex + 1);
	}

	const FString Sql = FString::Printf(
		TEXT("INSERT OR REPLACE INTO \"%s\" (%s) VALUES (%s)"),
		*Table.TableName,
		*ColumnList,
		*PlaceholderList);

	for (const FESQLUsqliteRow& Row : Rows)
	{
		TArray<FESQLBindingValue> Bindings;
		Bindings.Reserve(Table.Columns.Num());

		for (const FESQLUsqliteColumnSchema& Column : Table.Columns)
		{
			if (const FESQLBindingValue* ExistingValue = Row.FindValue(Column.Name))
			{
				Bindings.Add(*ExistingValue);
			}
			else
			{
				Bindings.Add(FESQLBindingValue::Null());
			}
		}

		const FESQLQueryResult InsertResult = Database->Execute(Sql, Bindings);
		if (!InsertResult.bSuccess)
		{
			OutError = InsertResult.ErrorMessage;
			return false;
		}
	}

	return true;
}
}

bool FESQLUsqliteBuilder::BuildProjectRoot(
	const FString& ProjectRoot,
	const FString& PreferredOutputDbPath,
	FString& OutResolvedDatabasePath,
	FString& OutError)
{
	FESQLUsqliteProject Project;
	if (!FESQLUsqliteSerializer::LoadProject(ProjectRoot, Project, OutError))
	{
		return false;
	}

	return BuildProject(Project, ProjectRoot, PreferredOutputDbPath, OutResolvedDatabasePath, OutError);
}

bool FESQLUsqliteBuilder::BuildProject(
	const FESQLUsqliteProject& Project,
	const FString& ProjectRoot,
	const FString& PreferredOutputDbPath,
	FString& OutResolvedDatabasePath,
	FString& OutError)
{
	const FESQLUsqliteValidationResult ValidationResult = FESQLUsqliteValidator::ValidateProject(Project);
	if (!ValidationResult.bSuccess)
	{
		OutError = ValidationResult.ToErrorString();
		return false;
	}

	if (PreferredOutputDbPath.IsEmpty())
	{
		FString ProjectHash;
		if (!FESQLUsqliteSerializer::ComputeProjectHash(Project, ProjectHash, OutError))
		{
			return false;
		}

		OutResolvedDatabasePath = FESQLUsqliteSerializer::GetDerivedDatabasePath(ProjectRoot, ProjectHash);
	}
	else
	{
		OutResolvedDatabasePath = FESQLPathResolver::ResolveAbsolutePath(PreferredOutputDbPath);
	}

	if (PreferredOutputDbPath.IsEmpty() && CanReuseDerivedDatabase(OutResolvedDatabasePath))
	{
		return true;
	}

	DeleteExistingDatabaseArtifacts(OutResolvedDatabasePath);

	const FESQLDatabaseOpenResult OpenResult = FESQLDatabase::Open(OutResolvedDatabasePath);
	if (!OpenResult)
	{
		OutError = OpenResult.GetErrorMessage();
		return false;
	}
	TSharedPtr<FESQLDatabase> Database = OpenResult.GetValue();

	if (!EnsureSchemaVersionTable(Database, OutError))
	{
		Database->Close();
		return false;
	}

	const FESQLErrorResult BeginResult = Database->BeginTransaction();
	if (!BeginResult.bSuccess)
	{
		Database->Close();
		OutError = BeginResult.GetErrorMessage();
		return false;
	}

	for (const FESQLUsqliteMigration& Migration : Project.Migrations)
	{
		for (const FString& Statement : ParseSqlStatements(Migration.Sql))
		{
			const FESQLQueryResult Result = Database->Execute(Statement);
			if (!Result.bSuccess)
			{
				Database->RollbackTransaction();
				Database->Close();
				OutError = FString::Printf(TEXT("Migration '%s' failed: %s"), *Migration.RelativePath, *Result.ErrorMessage);
				return false;
			}
		}

		if (!UpdateSchemaVersion(Database, Migration, OutError))
		{
			Database->RollbackTransaction();
			Database->Close();
			return false;
		}
	}

	for (const FESQLUsqliteTableSchema& Table : Project.Tables)
	{
		const TArray<FESQLUsqliteRow>* TableRows = Project.Data.Find(Table.TableName);
		if (!TableRows)
		{
			continue;
		}

		if (!InsertTableRows(Database, Table, *TableRows, OutError))
		{
			Database->RollbackTransaction();
			Database->Close();
			return false;
		}
	}

	const FESQLErrorResult CommitResult = Database->CommitTransaction();
	if (!CommitResult.bSuccess)
	{
		Database->Close();
		OutError = CommitResult.GetErrorMessage();
		return false;
	}

	const FESQLQueryResult CheckpointResult = Database->Execute(TEXT("PRAGMA wal_checkpoint(TRUNCATE)"));
	if (!CheckpointResult.bSuccess)
	{
		Database->Close();
		OutError = CheckpointResult.ErrorMessage;
		return false;
	}

	const FESQLQueryResult IntegrityCheck = Database->Execute(TEXT("PRAGMA integrity_check"));
	Database->Close();
	DeleteDatabaseSidecars(OutResolvedDatabasePath);

	if (!IntegrityCheck.bSuccess || !IntegrityCheck.HasRows())
	{
		OutError = IntegrityCheck.bSuccess ? TEXT("PRAGMA integrity_check returned no rows.") : IntegrityCheck.ErrorMessage;
		return false;
	}

	FString IntegrityMessage;
	if (!IntegrityCheck.Rows[0].TryGetString(TEXT("integrity_check"), IntegrityMessage))
	{
		OutError = TEXT("PRAGMA integrity_check did not return a readable result.");
		return false;
	}

	if (!IntegrityMessage.Equals(TEXT("ok"), ESearchCase::IgnoreCase))
	{
		OutError = FString::Printf(TEXT("PRAGMA integrity_check failed: %s"), *IntegrityMessage);
		return false;
	}

	return true;
}