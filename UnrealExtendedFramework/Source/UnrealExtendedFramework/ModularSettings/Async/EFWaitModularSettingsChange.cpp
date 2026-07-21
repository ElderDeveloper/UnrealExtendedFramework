// Fill out your copyright notice in the Description page of Project Settings.


#include "EFWaitModularSettingsChange.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "UnrealExtendedFramework/ModularSettings/EFModularSettingsSubsystem.h"
#include "UnrealExtendedFramework/ModularSettings/Components/EFWorldSettingsComponent.h"
#include "UnrealExtendedFramework/ModularSettings/Components/EFPlayerSettingsComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

 

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
		UE_LOG(LogTemp, Error, TEXT("UEFWaitModularSettingsChange: Invalid WorldContextObject provided."));
		return;
	}

	if (World->GetGameInstance())
	{
		if (const auto SettingsSubsystem = World->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
		{
			SettingsSubsystem->OnSettingsChanged.AddDynamic(this, &UEFWaitModularSettingsChange::OnSettingChanged);
		}
	}

	if (AGameStateBase* GS = World->GetGameState())
	{
		if (UEFWorldSettingsComponent* WorldComp = GS->FindComponentByClass<UEFWorldSettingsComponent>())
		{
			WorldComp->OnSettingChanged.AddDynamic(this, &UEFWaitModularSettingsChange::OnSettingChanged);
		}
	}

	if (World->GetFirstPlayerController() && World->GetFirstPlayerController()->PlayerState)
	{
		if (UEFPlayerSettingsComponent* PlayerComp = World->GetFirstPlayerController()->PlayerState->FindComponentByClass<UEFPlayerSettingsComponent>())
		{
			PlayerComp->OnSettingChanged.AddDynamic(this, &UEFWaitModularSettingsChange::OnSettingChanged);
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

