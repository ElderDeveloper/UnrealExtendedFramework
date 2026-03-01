// Fill out your copyright notice in the Description page of Project Settings.

#include "EFSettingsWidgetBase.h"
#include "../EFModularSettingsSubsystem.h"
#include "../Components/EFWorldSettingsComponent.h"
#include "../Components/EFPlayerSettingsComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Components/TextBlock.h"

void UEFSettingsWidgetBase::OnTrackedSettingsChanged_Implementation(UEFModularSettingsBase* ChangedSetting)
{
	
}

void UEFSettingsWidgetBase::SettingsPreConstruct_Implementation()
{
	if (SettingsLabel)
	{
		FSlateFontInfo SettingsLabelFontInfo = SettingsLabel->GetFont();
		SettingsLabelFontInfo.Size = SettingsLabelFontSize;
		SettingsLabel->SetFont(FSlateFontInfo(SettingsLabelFontInfo));
		SettingsLabel->SetText(SettingsDisplayName);
	}
}

void UEFSettingsWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	SettingsPreConstruct();
	
	TryBindToSettingsSources();
}

void UEFSettingsWidgetBase::TryBindToSettingsSources()
{
	UWorld* World = GetWorld();
	if (!World) return;

	bool bNeedWorld = (SettingsSource == EEFSettingsSource::World || SettingsSource == EEFSettingsSource::Auto);
	bool bNeedPlayer = (SettingsSource == EEFSettingsSource::Player || SettingsSource == EEFSettingsSource::Auto);
	
	bool bFoundWorld = false;
	bool bFoundPlayer = false;

	// Subsystem (Always available)
	if (UEFModularSettingsSubsystem* Subsystem = World->GetGameInstance()->GetSubsystem<UEFModularSettingsSubsystem>())
	{
		Subsystem->OnSettingsChanged.AddUniqueDynamic(this, &UEFSettingsWidgetBase::OnSettingChanged);
	}

	// World
	if (AGameStateBase* GS = World->GetGameState())
	{
		if (UEFWorldSettingsComponent* WorldComp = GS->FindComponentByClass<UEFWorldSettingsComponent>())
		{
			WorldComp->OnSettingChanged.AddUniqueDynamic(this, &UEFSettingsWidgetBase::OnSettingChanged);
			bFoundWorld = true;
		}
	}

	// Player
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APlayerState* PS = PC->PlayerState)
		{
			if (UEFPlayerSettingsComponent* PlayerComp = PS->FindComponentByClass<UEFPlayerSettingsComponent>())
			{
				PlayerComp->OnSettingChanged.AddUniqueDynamic(this, &UEFSettingsWidgetBase::OnSettingChanged);
				bFoundPlayer = true;
			}
		}
	}

	// If we still need something, retry later
	if ((bNeedWorld && !bFoundWorld) || (bNeedPlayer && !bFoundPlayer))
	{
		World->GetTimerManager().SetTimer(BindingRetryTimer, this, &UEFSettingsWidgetBase::TryBindToSettingsSources, 0.5f, false);
	}
	
	// Initial Refresh: Try to find the setting and notify subclasses
	if (const auto Setting = UEFModularSettingsLibrary::GetModularSetting(this, SettingsTag, SettingsSource))
	{
		OnTrackedSettingsChanged(Setting);
	}
}


void UEFSettingsWidgetBase::OnSettingChanged(UEFModularSettingsBase* ChangedSetting)
{
	if (ChangedSetting && ChangedSetting->SettingTag == SettingsTag)
	{
		OnTrackedSettingsChanged(ChangedSetting);
	}
}
