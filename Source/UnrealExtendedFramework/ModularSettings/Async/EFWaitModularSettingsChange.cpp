// Fill out your copyright notice in the Description page of Project Settings.


#include "EFWaitModularSettingsChange.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"

 

UEFWaitModularSettingsChange* UEFWaitModularSettingsChange::WaitForModularSettingsChange(UObject* WorldContextObject,FGameplayTag SettingTag)
{
	UEFWaitModularSettingsChange* Node = NewObject<UEFWaitModularSettingsChange>();
	Node->SettingTag = SettingTag;
	Node->WorldContextObject = WorldContextObject;
	
	return Node;
}

void UEFWaitModularSettingsChange::Activate()
{
	Super::Activate();

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("UEFWaitModularSettingsChange: Invalid WorldContextObject provided."));
	}

	if (World->GetGameInstance())
	{
		if (const auto SettingsSubsystem = World->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
		{
			if (auto ChangedSetting = SettingsSubsystem->GetSettingByTag(SettingTag))
			{
				OnSettingChanged(ChangedSetting);
				SettingsSubsystem->OnSettingsChanged.AddDynamic(this, &UEFWaitModularSettingsChange::OnSettingChanged);
			}
		}
	}
}

void UEFWaitModularSettingsChange::OnSettingChanged(UEFModularSettingsBase* ChangedSetting)
{
	if (ChangedSetting && ChangedSetting->SettingTag == SettingTag)
	{
		OnTriggered.Broadcast(ChangedSetting);
	}
}

