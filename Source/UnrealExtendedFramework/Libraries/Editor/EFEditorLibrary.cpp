// Fill out your copyright notice in the Description page of Project Settings.
#include "EFEditorLibrary.h"
#include "GameMapsSettings.h"


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
