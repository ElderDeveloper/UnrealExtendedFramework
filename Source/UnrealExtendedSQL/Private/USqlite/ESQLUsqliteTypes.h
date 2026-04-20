// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESQLTypes.h"

struct FESQLUsqliteColumnSchema
{
	FString Name;
	FString SQLiteType;
	bool bNullable = true;
	bool bPrimaryKey = false;
	FString DefaultValue;
};

struct FESQLUsqliteTableSchema
{
	FString TableName;
	FString PrimaryKeyColumn;
	FString RowStructPath;
	FString LabelColumn;
	TArray<FESQLUsqliteColumnSchema> Columns;

	const FESQLUsqliteColumnSchema* FindColumn(const FString& ColumnName) const
	{
		return Columns.FindByPredicate([&ColumnName](const FESQLUsqliteColumnSchema& Column)
		{
			return Column.Name == ColumnName;
		});
	}
};

struct FESQLUsqliteMigration
{
	int32 Sequence = 0;
	FString Slug;
	FString FileName;
	FString RelativePath;
	FString Sql;
};

struct FESQLUsqliteFileHash
{
	FString RelativePath;
	FString Hash;
	int64 SizeBytes = 0;
};

struct FESQLUsqliteLock
{
	FString HashAlgorithm = TEXT("sha256");
	FString ProjectHash;
	int32 HighestMigrationSequence = 0;
	TArray<FESQLUsqliteFileHash> Files;

	const FESQLUsqliteFileHash* FindFile(const FString& RelativePath) const
	{
		return Files.FindByPredicate([&RelativePath](const FESQLUsqliteFileHash& File)
		{
			return File.RelativePath == RelativePath;
		});
	}
	};

struct FESQLUsqliteRow
{
	TMap<FString, FESQLBindingValue> Values;

	const FESQLBindingValue* FindValue(const FString& ColumnName) const
	{
		return Values.Find(ColumnName);
	}
};

struct FESQLUsqliteProject
{
	FString ProjectName;
	int32 FormatVersion = 1;
	TArray<FESQLUsqliteTableSchema> Tables;
	TMap<FString, TArray<FESQLUsqliteRow>> Data;
	TArray<FESQLUsqliteMigration> Migrations;
	FESQLUsqliteLock Lock;

	const FESQLUsqliteTableSchema* FindTable(const FString& TableName) const
	{
		return Tables.FindByPredicate([&TableName](const FESQLUsqliteTableSchema& Table)
		{
			return Table.TableName == TableName;
		});
	}

	int32 GetHighestMigrationSequence() const
	{
		int32 HighestSequence = 0;
		for (const FESQLUsqliteMigration& Migration : Migrations)
		{
			HighestSequence = FMath::Max(HighestSequence, Migration.Sequence);
		}
		return HighestSequence;
	}
};

struct FESQLUsqliteValidationIssue
{
	FString RelativePath;
	FString Message;
};

struct FESQLUsqliteValidationResult
{
	bool bSuccess = true;
	TArray<FESQLUsqliteValidationIssue> Issues;

	void AddError(FString RelativePath, FString Message)
	{
		bSuccess = false;
		Issues.Add({ MoveTemp(RelativePath), MoveTemp(Message) });
	}

	FString ToErrorString() const
	{
		FString Result;
		for (int32 Index = 0; Index < Issues.Num(); ++Index)
		{
			const FESQLUsqliteValidationIssue& Issue = Issues[Index];
			if (Index > 0)
			{
				Result += TEXT("\n");
			}

			if (Issue.RelativePath.IsEmpty())
			{
				Result += Issue.Message;
			}
			else
			{
				Result += FString::Printf(TEXT("%s: %s"), *Issue.RelativePath, *Issue.Message);
			}
		}
		return Result;
	}
};