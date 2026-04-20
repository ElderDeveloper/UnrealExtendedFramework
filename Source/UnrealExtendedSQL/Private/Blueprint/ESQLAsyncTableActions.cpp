// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Blueprint/ESQLAsyncTableActions.h"

#include "Subsystem/ESQLSubsystem.h"
#include "TableAsset/ESQLTableAsset.h"
#include "UObject/UnrealType.h"

namespace
{
	FESQLQueryResult MakeAsyncFailureResult(const FString& ErrorMessage)
	{
		return FESQLQueryResult::Failure(ErrorMessage);
	}

	UESQLSubsystem* ResolveSQLSubsystem(UObject* WorldContextObject, FString& OutError)
	{
		if (!WorldContextObject)
		{
			OutError = TEXT("WorldContextObject is null");
			return nullptr;
		}

		UWorld* World = WorldContextObject->GetWorld();
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		UESQLSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UESQLSubsystem>() : nullptr;
		if (!Subsystem)
		{
			OutError = TEXT("Failed to get UESQLSubsystem");
			return nullptr;
		}

		OutError.Reset();
		return Subsystem;
	}
}

void UESQLAsyncTableQueryActionBase::InitializeAction(UObject* InWorldContextObject, UESQLTableAsset* InTableAsset)
{
	WorldContextObject = InWorldContextObject;
	TableAsset = InTableAsset;
	RegisterWithGameInstance(InWorldContextObject);
}

bool UESQLAsyncTableQueryActionBase::ResolveActivationInputs(FString& OutError) const
{
	if (!WorldContextObject.IsValid())
	{
		OutError = TEXT("WorldContextObject is null");
		return false;
	}

	if (!TableAsset.IsValid())
	{
		OutError = TEXT("TableAsset is null");
		return false;
	}

	OutError.Reset();
	return true;
}

void UESQLAsyncTableQueryActionBase::BroadcastResultAndFinish(const FESQLQueryResult& Result)
{
	if (Result.bSuccess)
	{
		OnSuccess.Broadcast(Result);
	}
	else
	{
		OnFailure.Broadcast(Result);
	}

	SetReadyToDestroy();
}

UESQLAsyncLoadSQLRow* UESQLAsyncLoadSQLRow::AsyncLoadSQLRow(UObject* WorldContextObject, UESQLTableAsset* TableAsset, FESQLId RowId)
{
	UESQLAsyncLoadSQLRow* Action = NewObject<UESQLAsyncLoadSQLRow>();
	Action->InitializeAction(WorldContextObject, TableAsset);
	Action->RowId = MoveTemp(RowId);
	return Action;
}

void UESQLAsyncLoadSQLRow::Activate()
{
	FString Error;
	if (!ResolveActivationInputs(Error))
	{
		BroadcastResultAndFinish(MakeAsyncFailureResult(Error));
		return;
	}

	UESQLTableAsset* StrongTableAsset = TableAsset.Get();
	UESQLSubsystem* Subsystem = ResolveSQLSubsystem(WorldContextObject.Get(), Error);
	if (!Subsystem)
	{
		BroadcastResultAndFinish(MakeAsyncFailureResult(Error));
		return;
	}

	FESQLQuerySpec QuerySpec;
	QuerySpec.Filters.Add(FESQLFieldFilter(StrongTableAsset->GetSchemaDescriptor().PrimaryKeyColumn, EESQLFilterOperation::Equal, FESQLBindingValue::FromText(RowId.Value)));
	QuerySpec.Limit = 1;
	Subsystem->AsyncQueryTable(StrongTableAsset, QuerySpec, [WeakAction = TWeakObjectPtr<UESQLAsyncLoadSQLRow>(this)](const FESQLQueryResult& Result)
	{
		if (UESQLAsyncLoadSQLRow* Action = WeakAction.Get())
		{
			Action->BroadcastResultAndFinish(Result);
		}
	});
}

UESQLAsyncLoadSQLRows* UESQLAsyncLoadSQLRows::AsyncLoadSQLRows(UObject* WorldContextObject, UESQLTableAsset* TableAsset, int32 MaxRows)
{
	UESQLAsyncLoadSQLRows* Action = NewObject<UESQLAsyncLoadSQLRows>();
	Action->InitializeAction(WorldContextObject, TableAsset);
	Action->MaxRows = MaxRows;
	return Action;
}

void UESQLAsyncLoadSQLRows::Activate()
{
	FString Error;
	if (!ResolveActivationInputs(Error))
	{
		BroadcastResultAndFinish(MakeAsyncFailureResult(Error));
		return;
	}

	UESQLTableAsset* StrongTableAsset = TableAsset.Get();
	UESQLSubsystem* Subsystem = ResolveSQLSubsystem(WorldContextObject.Get(), Error);
	if (!Subsystem)
	{
		BroadcastResultAndFinish(MakeAsyncFailureResult(Error));
		return;
	}

	FESQLQuerySpec QuerySpec;
	QuerySpec.Limit = MaxRows > 0 ? MaxRows : 0;
	Subsystem->AsyncQueryTable(StrongTableAsset, QuerySpec, [WeakAction = TWeakObjectPtr<UESQLAsyncLoadSQLRows>(this)](const FESQLQueryResult& Result)
	{
		if (UESQLAsyncLoadSQLRows* Action = WeakAction.Get())
		{
			Action->BroadcastResultAndFinish(Result);
		}
	});
}

UESQLAsyncFindSQLRows* UESQLAsyncFindSQLRows::AsyncFindSQLRows(UObject* WorldContextObject, UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec)
{
	UESQLAsyncFindSQLRows* Action = NewObject<UESQLAsyncFindSQLRows>();
	Action->InitializeAction(WorldContextObject, TableAsset);
	Action->QuerySpec = QuerySpec;
	return Action;
}

void UESQLAsyncFindSQLRows::Activate()
{
	FString Error;
	if (!ResolveActivationInputs(Error))
	{
		BroadcastResultAndFinish(MakeAsyncFailureResult(Error));
		return;
	}

	UESQLTableAsset* StrongTableAsset = TableAsset.Get();
	UESQLSubsystem* Subsystem = ResolveSQLSubsystem(WorldContextObject.Get(), Error);
	if (!Subsystem)
	{
		BroadcastResultAndFinish(MakeAsyncFailureResult(Error));
		return;
	}

	Subsystem->AsyncQueryTable(StrongTableAsset, QuerySpec, [WeakAction = TWeakObjectPtr<UESQLAsyncFindSQLRows>(this)](const FESQLQueryResult& Result)
	{
		if (UESQLAsyncFindSQLRows* Action = WeakAction.Get())
		{
			Action->BroadcastResultAndFinish(Result);
		}
	});
}

UESQLAsyncSaveSQLRow* UESQLAsyncSaveSQLRow::AsyncSaveSQLRow(UObject* WorldContextObject, UESQLTableAsset* TableAsset, const FString& RowIdOverride, const int32& RowData)
{
	return nullptr;
}

DEFINE_FUNCTION(UESQLAsyncSaveSQLRow::execAsyncSaveSQLRow)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_PROPERTY(FStrProperty, RowIdOverride);

	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FProperty* StructProperty = Stack.MostRecentProperty;
	void* StructData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	UESQLAsyncSaveSQLRow* Action = NewObject<UESQLAsyncSaveSQLRow>();
	Action->InitializeAction(WorldContextObject, TableAsset, RowIdOverride);
	Action->CaptureRowPayload(StructProperty, StructData);
	*(UESQLAsyncSaveSQLRow**)RESULT_PARAM = Action;
}

void UESQLAsyncSaveSQLRow::InitializeAction(UObject* InWorldContextObject, UESQLTableAsset* InTableAsset, const FString& InRowIdOverride)
{
	WorldContextObject = InWorldContextObject;
	TableAsset = InTableAsset;
	RowIdOverride = InRowIdOverride;
	RegisterWithGameInstance(InWorldContextObject);
}

bool UESQLAsyncSaveSQLRow::CaptureRowPayload(FProperty* StructProperty, const void* StructData)
{
	ResetCapturedRowPayload();
	ActivationError.Reset();

	const FStructProperty* StructProp = CastField<FStructProperty>(StructProperty);
	if (!StructProp || !StructData || !StructProp->Struct)
	{
		ActivationError = TEXT("Async Save SQL Row requires a valid input struct pin");
		return false;
	}

	RowStructType = StructProp->Struct;
	CapturedRowPayload.SetNumUninitialized(RowStructType->GetStructureSize());
	RowStructType->InitializeStruct(CapturedRowPayload.GetData());
	RowStructType->CopyScriptStruct(CapturedRowPayload.GetData(), StructData);
	return true;
}

void UESQLAsyncSaveSQLRow::Activate()
{
	if (!ActivationError.IsEmpty())
	{
		BroadcastFailureAndFinish(MakeAsyncFailureResult(ActivationError));
		return;
	}

	if (!WorldContextObject.IsValid())
	{
		BroadcastFailureAndFinish(MakeAsyncFailureResult(TEXT("WorldContextObject is null")));
		return;
	}

	UESQLTableAsset* StrongTableAsset = TableAsset.Get();
	if (!StrongTableAsset)
	{
		BroadcastFailureAndFinish(MakeAsyncFailureResult(TEXT("TableAsset is null")));
		return;
	}

	if (!RowStructType || CapturedRowPayload.Num() == 0)
	{
		BroadcastFailureAndFinish(MakeAsyncFailureResult(TEXT("Async Save SQL Row has no captured row payload")));
		return;
	}

	FString Error;
	UESQLSubsystem* Subsystem = ResolveSQLSubsystem(WorldContextObject.Get(), Error);
	if (!Subsystem)
	{
		BroadcastFailureAndFinish(MakeAsyncFailureResult(Error));
		return;
	}

	Subsystem->AsyncSaveRowFromStruct(WorldContextObject.Get(), StrongTableAsset, CapturedRowPayload.GetData(), RowStructType, [WeakAction = TWeakObjectPtr<UESQLAsyncSaveSQLRow>(this)](const FESQLQueryResult& Result, const FString& ResolvedRowId)
	{
		if (UESQLAsyncSaveSQLRow* Action = WeakAction.Get())
		{
			if (Result.bSuccess)
			{
				Action->OnSuccess.Broadcast(Result, ResolvedRowId);
			}
			else
			{
				Action->OnFailure.Broadcast(Result);
			}

			Action->ResetCapturedRowPayload();
			Action->SetReadyToDestroy();
		}
	}, RowIdOverride);
}

void UESQLAsyncSaveSQLRow::BeginDestroy()
{
	ResetCapturedRowPayload();
	Super::BeginDestroy();
}

void UESQLAsyncSaveSQLRow::ResetCapturedRowPayload()
{
	if (RowStructType && CapturedRowPayload.Num() > 0)
	{
		RowStructType->DestroyStruct(CapturedRowPayload.GetData());
	}

	CapturedRowPayload.Reset();
	RowStructType = nullptr;
}

void UESQLAsyncSaveSQLRow::BroadcastFailureAndFinish(const FESQLQueryResult& Result)
{
	OnFailure.Broadcast(Result);
	ResetCapturedRowPayload();
	SetReadyToDestroy();
}