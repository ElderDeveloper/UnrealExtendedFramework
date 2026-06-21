// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFCloudScriptSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

void UEPFCloudScriptSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEPFCloudScriptSubsystem::Deinitialize()
{
	LastResult = FEPFCloudScriptResult();
	Super::Deinitialize();
}

void UEPFCloudScriptSubsystem::ExecuteFunction(const FString& FunctionName, const FString& FunctionParameterJson)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("FunctionName"), FunctionName);

	// Parse the parameter JSON string into a JsonObject
	TSharedPtr<FJsonObject> ParamObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FunctionParameterJson);
	if (FJsonSerializer::Deserialize(Reader, ParamObj) && ParamObj.IsValid())
	{
		Body->SetObjectField(TEXT("FunctionParameter"), ParamObj);
	}

	SendPlayFabRequestDetailed(
		TEXT("/Client/ExecuteCloudScript"),
		Body,
		true,
		FOnPlayFabResponseDetailed::CreateLambda([this, FunctionName](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			LastResult = FEPFCloudScriptResult();
			LastResult.FunctionName = FunctionName;

			if (Result.bSuccess && Response.IsValid())
			{
				// Extract FunctionResult
				const TSharedPtr<FJsonObject>* FunctionResult = nullptr;
				if (Response->TryGetObjectField(TEXT("FunctionResult"), FunctionResult) && FunctionResult)
				{
					FString ResultString;
					const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
					FJsonSerializer::Serialize((*FunctionResult).ToSharedRef(), Writer);
					LastResult.ResultJson = ResultString;
				}

				LastResult.ExecutionTimeMs = Response->GetIntegerField(TEXT("ExecutionTimeSeconds")) * 1000;

				// Check for CloudScript errors (logic errors, not HTTP errors)
				const TSharedPtr<FJsonObject>* ErrorObj = nullptr;
				if (Response->TryGetObjectField(TEXT("Error"), ErrorObj) && ErrorObj)
				{
					LastResult.bError = true;
					LastResult.ErrorMessage = (*ErrorObj)->GetStringField(TEXT("Message"));
				}

				OnCloudScriptExecuted.Broadcast(LastResult.bError ? FEPFResult::Failure(LastResult.ErrorMessage) : FEPFResult::Success(), LastResult.ResultJson);
			}
			else
			{
				LastResult.bError = true;
				LastResult.ErrorMessage = Result.ErrorMessage.IsEmpty() ? TEXT("Request failed") : Result.ErrorMessage;
				OnCloudScriptExecuted.Broadcast(FEPFResult::Failure(LastResult.ErrorMessage), TEXT(""));
			}
		})
	);
}

namespace
{
	/** Extract a CloudScript result (FunctionResult + Error + timing) from an
	 *  /Client/ExecuteCloudScript or /CloudScript/ExecuteFunction response. */
	FEPFCloudScriptResult ParseCloudScriptResponse(const FString& FunctionName, TSharedPtr<FJsonObject> Response)
	{
		FEPFCloudScriptResult Out;
		Out.FunctionName = FunctionName;
		if (!Response.IsValid())
		{
			return Out;
		}

		const TSharedPtr<FJsonObject>* FunctionResult = nullptr;
		if (Response->TryGetObjectField(TEXT("FunctionResult"), FunctionResult) && FunctionResult)
		{
			FString ResultString;
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
			FJsonSerializer::Serialize((*FunctionResult).ToSharedRef(), Writer);
			Out.ResultJson = ResultString;
		}

		// Legacy ExecuteCloudScript reports ExecutionTimeSeconds; the entity
		// ExecuteFunction reports ExecutionTimeMilliseconds.
		int32 Millis = 0;
		if (Response->TryGetNumberField(TEXT("ExecutionTimeMilliseconds"), Millis))
		{
			Out.ExecutionTimeMs = Millis;
		}
		else
		{
			Out.ExecutionTimeMs = Response->GetIntegerField(TEXT("ExecutionTimeSeconds")) * 1000;
		}

		const TSharedPtr<FJsonObject>* ErrorObj = nullptr;
		if (Response->TryGetObjectField(TEXT("Error"), ErrorObj) && ErrorObj)
		{
			Out.bError = true;
			(*ErrorObj)->TryGetStringField(TEXT("Message"), Out.ErrorMessage);
		}
		return Out;
	}
}

void UEPFCloudScriptSubsystem::ExecuteFunctionWithParams(const FString& FunctionName, const TMap<FString, FString>& Params)
{
	// Convert TMap to JSON string
	TSharedRef<FJsonObject> ParamObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Params)
	{
		ParamObj->SetStringField(Pair.Key, Pair.Value);
	}

	FString ParamJson;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ParamJson);
	FJsonSerializer::Serialize(ParamObj, Writer);

	ExecuteFunction(FunctionName, ParamJson);
}

void UEPFCloudScriptSubsystem::ExecuteAzureFunction(const FString& FunctionName, const FString& FunctionParameterJson)
{
	ExecuteAzureFunctionCallback(FunctionName, FunctionParameterJson,
		FOnEPFCloudScriptComplete::CreateLambda([this](const FEPFResult& Result, const FString& ResultJson)
		{
			OnCloudScriptExecuted.Broadcast(Result, ResultJson);
		}));
}

void UEPFCloudScriptSubsystem::ExecuteAzureFunctionCallback(const FString& FunctionName, const FString& FunctionParameterJson, FOnEPFCloudScriptComplete OnComplete)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("FunctionName"), FunctionName);

	// Target the calling player entity (the EntityToken auth already identifies them,
	// but ExecuteFunction accepts an explicit Entity too).
	TSharedRef<FJsonObject> Entity = MakeShared<FJsonObject>();
	Entity->SetStringField(TEXT("Id"), GetEntityId());
	Entity->SetStringField(TEXT("Type"), GetEntityType());
	Body->SetObjectField(TEXT("Entity"), Entity);

	TSharedPtr<FJsonObject> ParamObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FunctionParameterJson);
	if (FJsonSerializer::Deserialize(Reader, ParamObj) && ParamObj.IsValid())
	{
		Body->SetObjectField(TEXT("FunctionParameter"), ParamObj);
	}

	SendPlayFabRequestDetailed(
		TEXT("/CloudScript/ExecuteFunction"),
		Body,
		EEPFAuthMode::EntityToken,
		FOnPlayFabResponseDetailed::CreateLambda([this, FunctionName, OnComplete](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess && Response.IsValid())
			{
				LastResult = ParseCloudScriptResponse(FunctionName, Response);
				const FEPFResult Broadcast = LastResult.bError
					? FEPFResult::Failure(LastResult.ErrorMessage)
					: FEPFResult::Success();
				OnComplete.ExecuteIfBound(Broadcast, LastResult.ResultJson);
			}
			else
			{
				LastResult = FEPFCloudScriptResult();
				LastResult.FunctionName = FunctionName;
				LastResult.bError = true;
				LastResult.ErrorMessage = Result.ErrorMessage.IsEmpty() ? TEXT("Request failed") : Result.ErrorMessage;
				OnComplete.ExecuteIfBound(FEPFResult::Failure(LastResult.ErrorMessage), TEXT(""));
			}
		})
	);
}

FEPFCloudScriptResult UEPFCloudScriptSubsystem::GetLastResult() const
{
	return LastResult;
}
