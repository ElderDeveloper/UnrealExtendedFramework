// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedLibrary.h"

void UUEExtendedLibrary::ExtendedBlueprintLOG(TEnumAsByte<EExtendedLog> ExtendedLogType , FString Log)
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

