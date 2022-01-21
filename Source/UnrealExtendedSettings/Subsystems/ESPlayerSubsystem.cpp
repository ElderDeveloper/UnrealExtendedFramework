// Fill out your copyright notice in the Description page of Project Settings.


#include "ESPlayerSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "UnrealExtendedSettings/Data/ESSaveGame.h"


void UESPlayerSubsystem::UpdateSettingsFloatValue(TEnumAsByte<EExtendedSettingsFloat> SettingsType, float Value)
{
	FloatSettingsUpdate.Broadcast(SettingsType , Value);
}

void UESPlayerSubsystem::SaveSettings()
{
	if (UGameplayStatics::DoesSaveGameExist(UESettingsSaveName,0))
	{
		FAsyncLoadGameFromSlotDelegate LoadDelegate;
		LoadDelegate.BindUObject(this,&UESPlayerSubsystem::OnSaveGameLoadComplete);
		UGameplayStatics::AsyncLoadGameFromSlot(UESettingsSaveName,0,LoadDelegate);
	}
	else if (const auto Save = Cast<UESSaveGame>(UGameplayStatics::CreateSaveGameObject(UESSaveGame::StaticClass())))
	{
		Save->UESettingsSaveStruct = SettingsStruct;
		UGameplayStatics::SaveGameToSlot(Save,UESettingsSaveName,0);
	}

}

void UESPlayerSubsystem::OnSaveGameLoadComplete(const FString& SaveGameSlot, const int32 PlayerIndex,USaveGame* SettingsSaveObject)
{
	if (!SettingsSaveObject) return;
	
	if (const auto Save = Cast<UESSaveGame>(SettingsSaveObject))
	{
		Save->UESettingsSaveStruct = SettingsStruct;
		UGameplayStatics::SaveGameToSlot(Save,UESettingsSaveName,0);
	}
}

