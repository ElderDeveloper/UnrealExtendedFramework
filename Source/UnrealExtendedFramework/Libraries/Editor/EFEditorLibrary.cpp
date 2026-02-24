// Fill out your copyright notice in the Description page of Project Settings.
#include "EFEditorLibrary.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"


void UEFEditorLibrary::ExtendedBlueprintLOG(EExtendedLog ExtendedLogType, FString Log)
{
	switch (ExtendedLogType)
	{
	case EExtendedLog::Log:
		UE_LOG(LogTemp, Log, TEXT("Extended Log : %s"), *Log);
		break;

	case EExtendedLog::Warning:
		UE_LOG(LogTemp, Warning, TEXT("Extended Log : %s"), *Log);
		break;

	case EExtendedLog::Error:
		UE_LOG(LogTemp, Error, TEXT("Extended Log : %s"), *Log);
		break;
	default: break;
	}
}


void UEFEditorLibrary::ExecuteFunction(UObject* RequestOwner, UObject* TargetObject, const FString FunctionToExecute)
{
	if (!TargetObject || !RequestOwner)
	{
		return;
	}

	// BUG FIX: Previously used `new FFrame` + `CallFunction` which leaked memory
	// (FFrame was never deleted). ProcessEvent is the standard, safe UE pattern
	// for calling UFUNCTIONs by name.
	if (UFunction* Function = TargetObject->FindFunction(*FunctionToExecute))
	{
		TargetObject->ProcessEvent(Function, nullptr);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UEFEditorLibrary::ExecuteFunction : Function '%s' not found on %s"),
			*FunctionToExecute, *TargetObject->GetName());
	}
}


float UEFEditorLibrary::GetGameplayFramePerSecond(const UObject* WorldContextObject)
{
	// BUG FIX: Previously cast WorldContextObject directly to UWorld, which almost always
	// returned nullptr (WorldContext is typically an Actor, not a UWorld).
	// Now uses the engine's proper world resolution.
	if (!WorldContextObject)
	{
		return 0.f;
	}

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		const float DeltaSeconds = World->GetDeltaSeconds();
		if (DeltaSeconds > 0.f)
		{
			return (1.f / DeltaSeconds) * UGameplayStatics::GetGlobalTimeDilation(World);
		}
	}
	return 0.f;
}


float UEFEditorLibrary::GetDeltaSeconds(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return 0.f;
	}

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		return World->GetDeltaSeconds();
	}
	return 0.f;
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

	// BUG FIX: GGameIni is deprecated in UE 5.x.
	// Read directly from DefaultGame.ini using the project config path.
	GConfig->GetString(
		TEXT("/Script/EngineSettings.GeneralProjectSettings"),
		TEXT("ProjectVersion"),
		ProjectVersion,
		FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini")
	);

	return ProjectVersion;
}

FString UEFEditorLibrary::GetPlatformName()
{
	return FString(FPlatformProperties::IniPlatformName());
}