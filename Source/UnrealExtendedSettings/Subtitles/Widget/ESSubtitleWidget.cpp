// Fill out your copyright notice in the Description page of Project Settings.


#include "ESSubtitleWidget.h"

#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "UnrealExtendedSettings/Subtitles/Subsystem/ESSubtitleSubsystem.h"

UESSubtitleWidget::UESSubtitleWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}

void UESSubtitleWidget::ReceiveSubtitleRequest(FString Subtitle, float Duration)
{
	TextBlock->SetText(FText::FromString(Subtitle));
	
	if (SubtitleHandle.IsValid())
		GetWorld()->GetTimerManager().ClearTimer(SubtitleHandle);
	
	GetWorld()->GetTimerManager().SetTimer(SubtitleHandle,this,&UESSubtitleWidget::EraseSubtitle,Duration,false);
}

void UESSubtitleWidget::EraseSubtitle()
{
	TextBlock->SetText(FText());
}

void UESSubtitleWidget::InitializeSubtitle()
{
	if(const auto subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UESSubtitleSubsystem>())
	{
		subsystem->OnExecuteSubtitle.AddDynamic(this,&UESSubtitleWidget::UESSubtitleWidget::ReceiveSubtitleRequest);
	}
}


void UESSubtitleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GetWorld()->GetTimerManager().SetTimer(SubtitleHandle,this,&UESSubtitleWidget::InitializeSubtitle,0.5,false);
}
