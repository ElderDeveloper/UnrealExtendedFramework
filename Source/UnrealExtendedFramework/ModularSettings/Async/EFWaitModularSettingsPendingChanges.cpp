// Fill out your copyright notice in the Description page of Project Settings.

#include "EFWaitModularSettingsPendingChanges.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"

UEFWaitModularSettingsPendingChanges* UEFWaitModularSettingsPendingChanges::WaitForModularSettingsPendingChanges(UObject* WorldContextObject)
{
	UEFWaitModularSettingsPendingChanges* Node = NewObject<UEFWaitModularSettingsPendingChanges>();
	Node->WorldContextObject = WorldContextObject;
	Node->bLastKnownPendingState = false;
	return Node;
}

void UEFWaitModularSettingsPendingChanges::Activate()
{
	Super::Activate();

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("UEFWaitModularSettingsPendingChanges: Invalid WorldContextObject provided."));
		return;
	}

	if (World->GetGameInstance())
	{
		if (auto* SettingsSubsystem = World->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
		{
			// Listen to settings changing, loading, and saving
			SettingsSubsystem->OnSettingsChanged.AddDynamic(this, &UEFWaitModularSettingsPendingChanges::OnSettingChanged);
			SettingsSubsystem->OnSettingsLoaded.AddDynamic(this, &UEFWaitModularSettingsPendingChanges::CheckPendingState);

			// Initial check and immediate broadcast so the UI can set itself correctly on begin play
			bLastKnownPendingState = SettingsSubsystem->HasPendingChanges();
			OnStateChanged.Broadcast(bLastKnownPendingState);
		}
	}
}

void UEFWaitModularSettingsPendingChanges::OnSettingChanged(UEFModularSettingsBase* ChangedSetting)
{
	CheckPendingState();
}

void UEFWaitModularSettingsPendingChanges::CheckPendingState()
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("UEFWaitModularSettingsPendingChanges: Invalid WorldContextObject provided."));
		return;
	}
	
	if (World->GetGameInstance())
	{
		if (auto* SettingsSubsystem = World->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
		{
			bool bCurrentState = SettingsSubsystem->HasPendingChanges();
			
			// Only broadcast if the state actually changed (from true to false or false to true)
			if (bCurrentState != bLastKnownPendingState)
			{
				bLastKnownPendingState = bCurrentState;
				OnStateChanged.Broadcast(bCurrentState);
			}
		}
	}
}
