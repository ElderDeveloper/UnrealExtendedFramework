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

	Body->SetBoolField(TEXT("GeneratePlayStreamEvent"), true);

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

FEPFCloudScriptResult UEPFCloudScriptSubsystem::GetLastResult() const
{
	return LastResult;
}
