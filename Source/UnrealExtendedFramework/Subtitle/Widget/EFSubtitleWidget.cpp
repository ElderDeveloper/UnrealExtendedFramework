// Fill out your copyright notice in the Description page of Project Settings.

#include "EFSubtitleWidget.h"
#include "Components/TextBlock.h"
#include "UnrealExtendedFramework/Subtitle/Data/EFSubtitlePluginSettings.h"

void UEFSubtitleWidget::ApplyVisualSettings()
{
	UE_LOG(LogTemp, Warning, TEXT("ApplyVisualSettings"));
	if (SubtitleText != nullptr)
	{
		const auto SubtitleAppearance = GetDefault<UEFSubtitleSettings>();
		SubtitleText->SetFont(SubtitleAppearance->Font);
		SubtitleText->SetColorAndOpacity(SubtitleAppearance->FontColor);
		SubtitleText->SetShadowOffset(SubtitleAppearance->ShadowOffset);
		SubtitleText->SetShadowColorAndOpacity(SubtitleAppearance->ShadowColor);
		StopSubtitle();
	}
}

void UEFSubtitleWidget::ExecuteSubtitle(const FExtendedSubtitle& Subtitle, ESubtitleExecutionType ExecutionType)
{
	SubtitleText->SetText(Subtitle.Subtitle);
	
 	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	TimerManager.ClearTimer(SubtitleTimerHandle);
	TimerManager.SetTimer(SubtitleTimerHandle, this, &UEFSubtitleWidget::StopSubtitle, Subtitle.Duration, false);
}

void UEFSubtitleWidget::StopSubtitle()
{
	SubtitleText->SetText(FText::FromString(""));
}

