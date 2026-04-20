// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Private/USqlite/ESQLUsqliteValidator.h"

#include "Private/USqlite/ESQLUsqliteHash.h"

namespace
{
bool IsSchemaFile(const FString& RelativePath)
{
	return RelativePath.StartsWith(TEXT("schema/"));
}

bool IsMigrationFile(const FString& RelativePath)
{
	return RelativePath.StartsWith(TEXT("migrations/"));
}
}

FESQLUsqliteValidationResult FESQLUsqliteValidator::ValidateProject(const FESQLUsqliteProject& Project)
{
	FESQLUsqliteValidationResult Result;

	TSet<FString> TableNames;
	for (const FESQLUsqliteTableSchema& Table : Project.Tables)
	{
		if (Table.TableName.IsEmpty())
		{
			Result.AddError(TEXT("project.json"), TEXT("Table name cannot be empty."));
			continue;
		}

		if (TableNames.Contains(Table.TableName))
		{
			Result.AddError(TEXT("project.json"), FString::Printf(TEXT("Duplicate table name '%s'."), *Table.TableName));
		}
		else
		{
			TableNames.Add(Table.TableName);
		}

		if (Table.Columns.Num() == 0)
		{
			Result.AddError(FString::Printf(TEXT("schema/%s.json"), *Table.TableName), TEXT("Table schema has no columns."));
		}

		TSet<FString> ColumnNames;
		bool bFoundPrimaryKey = false;
		for (const FESQLUsqliteColumnSchema& Column : Table.Columns)
		{
			if (Column.Name.IsEmpty())
			{
				Result.AddError(FString::Printf(TEXT("schema/%s.json"), *Table.TableName), TEXT("Column name cannot be empty."));
				continue;
			}

			if (ColumnNames.Contains(Column.Name))
			{
				Result.AddError(FString::Printf(TEXT("schema/%s.json"), *Table.TableName), FString::Printf(TEXT("Duplicate column '%s'."), *Column.Name));
			}
			else
			{
				ColumnNames.Add(Column.Name);
			}

			if (Column.bPrimaryKey)
			{
				if (bFoundPrimaryKey)
				{
					Result.AddError(FString::Printf(TEXT("schema/%s.json"), *Table.TableName), TEXT("Only single-column primary keys are supported."));
				}
				bFoundPrimaryKey = true;
			}
		}

		if (!Table.PrimaryKeyColumn.IsEmpty() && !ColumnNames.Contains(Table.PrimaryKeyColumn))
		{
			Result.AddError(FString::Printf(TEXT("schema/%s.json"), *Table.TableName), FString::Printf(TEXT("Primary key '%s' is not present in the column list."), *Table.PrimaryKeyColumn));
		}

		const TArray<FESQLUsqliteRow>* TableRows = Project.Data.Find(Table.TableName);
		if (TableRows)
		{
			for (int32 RowIndex = 0; RowIndex < TableRows->Num(); ++RowIndex)
			{
				for (const TPair<FString, FESQLBindingValue>& Pair : (*TableRows)[RowIndex].Values)
				{
					if (!ColumnNames.Contains(Pair.Key))
					{
						Result.AddError(FString::Printf(TEXT("data/%s.ndjson"), *Table.TableName), FString::Printf(TEXT("Row %d references unknown column '%s'."), RowIndex + 1, *Pair.Key));
					}
				}
			}
		}
	}

	TSet<int32> MigrationSequences;
	for (const FESQLUsqliteMigration& Migration : Project.Migrations)
	{
		if (Migration.Sequence <= 0)
		{
			Result.AddError(TEXT("project.json"), FString::Printf(TEXT("Migration '%s' must have a positive sequence number."), *Migration.FileName));
		}

		if (MigrationSequences.Contains(Migration.Sequence))
		{
			Result.AddError(TEXT("project.json"), FString::Printf(TEXT("Duplicate migration sequence %d."), Migration.Sequence));
		}
		else
		{
			MigrationSequences.Add(Migration.Sequence);
		}

		if (Migration.RelativePath.IsEmpty())
		{
			Result.AddError(TEXT("project.json"), FString::Printf(TEXT("Migration %d is missing a relative path."), Migration.Sequence));
		}
	}

	return Result;
}

FESQLUsqliteValidationResult FESQLUsqliteValidator::ValidateAgainstLock(const FESQLUsqliteProject& Project, const TMap<FString, FString>& GeneratedFiles)
{
	FESQLUsqliteValidationResult Result = ValidateProject(Project);
	const FString HashAlgorithm = ESQLUsqliteHash::NormalizeAlgorithm(Project.Lock.HashAlgorithm);

	if (!Project.Lock.Files.IsEmpty() && HashAlgorithm.IsEmpty())
	{
		Result.AddError(TEXT(".usqlite-lock.json"), TEXT("Lock file is missing a supported hash algorithm."));
		return Result;
	}

	TMap<FString, FString> GeneratedHashes;
	bool bSchemaChanged = false;
	for (const TPair<FString, FString>& GeneratedFile : GeneratedFiles)
	{
		const FESQLUsqliteFileHash* ExistingHash = Project.Lock.FindFile(GeneratedFile.Key);
		FString NewHash;
		if (!ESQLUsqliteHash::ComputeTextHash(GeneratedFile.Value, HashAlgorithm, NewHash))
		{
			Result.AddError(TEXT(".usqlite-lock.json"), FString::Printf(TEXT("Unsupported lock hash algorithm '%s'."), *HashAlgorithm));
			return Result;
		}

		GeneratedHashes.Add(GeneratedFile.Key, NewHash);

		if (IsSchemaFile(GeneratedFile.Key) && (!ExistingHash || ExistingHash->Hash != NewHash))
		{
			bSchemaChanged = true;
		}

		if (ExistingHash)
		{
			if (IsMigrationFile(GeneratedFile.Key) && ExistingHash->Hash != NewHash)
			{
				Result.AddError(GeneratedFile.Key, TEXT("Existing migration content changed. Migrations are append-only."));
			}
		}
	}

	for (const FESQLUsqliteFileHash& LockedFile : Project.Lock.Files)
	{
		const FString* GeneratedHash = GeneratedHashes.Find(LockedFile.RelativePath);

		if (IsMigrationFile(LockedFile.RelativePath) && !GeneratedHash)
		{
			Result.AddError(LockedFile.RelativePath, TEXT("Existing migration file is missing. Migrations are append-only."));
		}

		if (IsSchemaFile(LockedFile.RelativePath) && !GeneratedHash)
		{
			bSchemaChanged = true;
		}
	}

	for (const FESQLUsqliteMigration& Migration : Project.Migrations)
	{
		if (Project.Lock.HighestMigrationSequence > 0
			&& !Project.Lock.FindFile(Migration.RelativePath)
			&& Migration.Sequence <= Project.Lock.HighestMigrationSequence)
		{
			Result.AddError(
				Migration.RelativePath.IsEmpty() ? TEXT("project.json") : Migration.RelativePath,
				TEXT("New migrations must append after the highest committed migration sequence."));
		}
	}

	if (bSchemaChanged && Project.GetHighestMigrationSequence() <= Project.Lock.HighestMigrationSequence)
	{
		Result.AddError(TEXT("project.json"), TEXT("Schema changed without a new migration sequence. Add a new migration before saving."));
	}

	return Result;
}

FESQLUsqliteValidationResult FESQLUsqliteValidator::ValidateForSave(const FESQLUsqliteProject& Project, const TMap<FString, FString>& GeneratedFiles)
{
	return ValidateAgainstLock(Project, GeneratedFiles);
}