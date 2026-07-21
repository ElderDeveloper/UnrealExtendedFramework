// Copyright Moon Punch Games. All Rights Reserved.

#include "EFUIFixtureFrameworkProviders.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ModularSettings/EFModularSettingsSubsystem.h"
#include "Systems/Subtitle/Data/EFSubtitleData.h"
#include "Systems/Subtitle/Widget/EFSubtitleDisplayWidget.h"

void UEFUIFixtureSubtitleSampleConfig::ApplyToWidget(UUserWidget& Widget) const
{
	UEFSubtitleDisplayWidget* SubtitleWidget = Cast<UEFSubtitleDisplayWidget>(&Widget);
	if (!SubtitleWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UI Lab: Subtitle Sample provider needs a UEFSubtitleDisplayWidget (got '%s')."),
			*Widget.GetClass()->GetName());
		return;
	}

	FEFSubtitleEntry Entry;
	Entry.Text = Text;
	Entry.SpeakerName = SpeakerName;
	Entry.SpeakerColor = SpeakerColor;
	Entry.Duration = Duration;

	FEFSubtitleRequest Request;
	Request.SubtitleKey = TEXT("UILabSample");

	SubtitleWidget->ShowSubtitle(Entry, Request);
}

void UEFUIFixtureModularSettingsConfig::ApplyToWidget(UUserWidget& Widget) const
{
	const UWorld* World = Widget.GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UEFModularSettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<UEFModularSettingsSubsystem>() : nullptr;

	if (!Settings)
	{
		UE_LOG(LogTemp, Warning, TEXT("UI Lab: Modular Settings provider needs the Runtime-Faithful Sandbox host mode "
			"(no UEFModularSettingsSubsystem available; add it to the fixture's RequiredSubsystems)."));
		return;
	}

	if (bLoadFromDisk)
	{
		Settings->LoadFromDisk();
	}
	Settings->RefreshAllSettings();
}
