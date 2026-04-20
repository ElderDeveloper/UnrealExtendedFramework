// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Shared/ESQLId.h"
#include "Shared/ESQLTypes.h"
#include "ESQLAsyncTableActions.generated.h"

class UESQLTableAsset;
class UScriptStruct;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnESQLAsyncSaveComplete, const FESQLQueryResult&, Result, const FString&, ResolvedRowId);

UCLASS(Abstract)
class UESQLAsyncTableQueryActionBase : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnESQLQueryComplete OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FOnESQLQueryComplete OnFailure;

protected:
	void InitializeAction(UObject* InWorldContextObject, UESQLTableAsset* InTableAsset);
	bool ResolveActivationInputs(FString& OutError) const;
	void BroadcastResultAndFinish(const FESQLQueryResult& Result);

	TWeakObjectPtr<UObject> WorldContextObject;
	TWeakObjectPtr<UESQLTableAsset> TableAsset;
};

UCLASS(meta = (DisplayName = "Async Load SQL Row", HasDedicatedAsyncNode))
class UESQLAsyncLoadSQLRow : public UESQLAsyncTableQueryActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(
	BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Async Load SQL Row"), Category = "SQL|Async")
	static UESQLAsyncLoadSQLRow* AsyncLoadSQLRow(UObject* WorldContextObject, UESQLTableAsset* TableAsset, UPARAM(DisplayName = "SQL Id") FESQLId RowId);

	virtual void Activate() override;

private:
	FESQLId RowId;
};

UCLASS(meta = (DisplayName = "Async Load SQL Rows", HasDedicatedAsyncNode))
class UESQLAsyncLoadSQLRows : public UESQLAsyncTableQueryActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(
	BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Async Load SQL Rows", AdvancedDisplay = "MaxRows"), Category = "SQL|Async")
	static UESQLAsyncLoadSQLRows* AsyncLoadSQLRows(UObject* WorldContextObject, UESQLTableAsset* TableAsset, int32 MaxRows = 0);

	virtual void Activate() override;

private:
	int32 MaxRows = 0;
};

UCLASS(meta = (DisplayName = "Async Find SQL Rows", HasDedicatedAsyncNode))
class UESQLAsyncFindSQLRows : public UESQLAsyncTableQueryActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(
	BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Async Find SQL Rows"), Category = "SQL|Async")
	static UESQLAsyncFindSQLRows* AsyncFindSQLRows(UObject* WorldContextObject, UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec);

	virtual void Activate() override;

private:
	FESQLQuerySpec QuerySpec;
};

UCLASS(meta = (DisplayName = "Async Save SQL Row", HasDedicatedAsyncNode))
class UESQLAsyncSaveSQLRow : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(
	BlueprintCallable, CustomThunk, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", CustomStructureParam = "RowData", DisplayName = "Async Save SQL Row", AdvancedDisplay = "RowIdOverride"), Category = "SQL|Async")
	static UESQLAsyncSaveSQLRow* AsyncSaveSQLRow(UObject* WorldContextObject, UESQLTableAsset* TableAsset, const FString& RowIdOverride, const int32& RowData);
	DECLARE_FUNCTION(execAsyncSaveSQLRow);

	UPROPERTY(BlueprintAssignable)
	FOnESQLAsyncSaveComplete OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FOnESQLQueryComplete OnFailure;

	virtual void Activate() override;
	virtual void BeginDestroy() override;

	bool CaptureRowPayload(FProperty* StructProperty, const void* StructData);
	void InitializeAction(UObject* InWorldContextObject, UESQLTableAsset* InTableAsset, const FString& InRowIdOverride);

private:
	void ResetCapturedRowPayload();
	void BroadcastFailureAndFinish(const FESQLQueryResult& Result);

	TWeakObjectPtr<UObject> WorldContextObject;
	TWeakObjectPtr<UESQLTableAsset> TableAsset;
	FString RowIdOverride;

	UPROPERTY()
	TObjectPtr<UScriptStruct> RowStructType = nullptr;

	TArray<uint8> CapturedRowPayload;
	FString ActivationError;
};
