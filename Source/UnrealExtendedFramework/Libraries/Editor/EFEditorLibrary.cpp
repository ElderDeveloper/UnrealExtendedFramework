// Fill out your copyright notice in the Description page of Project Settings.
#include "EFEditorLibrary.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"


void UEFEditorLibrary::ExtendedBlueprintLOG(EExtendedLog ExtendedLogType , FString Log)
{
	switch (ExtendedLogType)
	{
	case EExtendedLog::Log :
		UE_LOG(LogTemp,Log,TEXT("Extended Log : %s"), *Log);
		break;

	case EExtendedLog::Warning :
		UE_LOG(LogTemp,Warning,TEXT("Extended Log : %s"), *Log);
		break;

	case EExtendedLog::Error:
		UE_LOG(LogTemp,Error,TEXT("Extended Log : %s"), *Log);
		break;
	default: break;
	};
}




void UEFEditorLibrary::ExecuteFunction(UObject* RequestOwner , UObject* TargetObject, const FString FunctionToExecute)
{
	if(TargetObject && RequestOwner)
	{
		UE_LOG(LogTemp,Warning,TEXT("Request Owner: %s") , *RequestOwner->GetName());


		//FindFunction will return a pointer to a UFunction based on a
		//given FName. We use an asterisk before our FString in order to
		//convert the FString variable to FName
		if(const auto Function = TargetObject->FindFunction(*FunctionToExecute))
		{
			//The following pointer is a void pointer,
			//this means that it can point to anything - from raw memory to all the known types -
			void* locals = nullptr;
 
			//In order to execute our function we need to reserve a chunk of memory in 
			//the execution stack for it.
			if(const auto frame = new FFrame(RequestOwner, Function, locals))
			{
				//Unfortunately the source code of the engine doesn't explain what the locals
				//pointer is used for.
				//After some trial and error I ended up on this code which actually works without any problem.
		
				//Actual call of our UFunction
				TargetObject->CallFunction(*frame, locals, Function);
 
				//delete our pointer to avoid memory leaks!
			}
		}
	}
}


float UEFEditorLibrary::GetGameplayFramePerSecond(const UObject* WorldContextObject)
{
	if (const auto World = Cast<UWorld>(WorldContextObject))
		return (1/World->GetDeltaSeconds())* UGameplayStatics::GetGlobalTimeDilation(World);
	return 0;
}

void UEFEditorLibrary::SetDefaultGameMap(const FString& MapName)
{
	if (const auto GameMaps = UGameMapsSettings::GetGameMapsSettings())
	{
		GameMaps->SetGameDefaultMap(MapName);
		GameMaps->SaveConfig();
	}
}


FString UEFEditorLibrary::GetDefaultGameMap()
{
	if (const auto GameMaps = UGameMapsSettings::GetGameMapsSettings())
	{
		return GameMaps->GetGameDefaultMap();
	}
	return FString();
}

bool UEFEditorLibrary::IsInEditor()
{
	return GIsEditor;
}

FString UEFEditorLibrary::GetProjectVersion()
{
	FString ProjectVersion;

	GConfig->GetString(
		TEXT("/Script/EngineSettings.GeneralProjectSettings"),
		TEXT("ProjectVersion"),
		ProjectVersion,
		GGameIni
	);

	return ProjectVersion;
}