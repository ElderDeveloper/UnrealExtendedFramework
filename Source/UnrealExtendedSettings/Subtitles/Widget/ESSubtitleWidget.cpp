// Fill out your copyright notice in the Description page of Project Settings.


#include "ESSubtitleWidget.h"

#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "UnrealExtendedSettings/Subtitles/Subsystem/ESSubtitleSubsystem.h"

UESSubtitleWidget::UESSubtitleWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}



void UESSubtitleWidget::ReceiveSubtitleRequest(FString Subtitle, float Duration, bool UseLetterCountAsDuration , float TimeForEachLetterCount, float TimeForAfterLetterCount, bool AnimateSubtitleLetters)
{

	if (SubtitleHandle.IsValid())
		GetWorld()->GetTimerManager().ClearTimer(SubtitleHandle);
	
	if (SubtitleAnimationHandle.IsValid())
		GetWorld()->GetTimerManager().ClearTimer(SubtitleAnimationHandle);

	TextBlock->SetText(FText::FromString(""));
	SubtitleArray.Empty();
	SubtitleLetterIndex = 0;
	
	if (AnimateSubtitleLetters && UseLetterCountAsDuration)
	{
		Subtitle.ParseIntoArray(SubtitleArray , TEXT(""));
		StoredSubtitle = Subtitle;
		
		GetWorld()->GetTimerManager().SetTimer(SubtitleAnimationHandle,this,&UESSubtitleWidget::SubtitleAnimation,TimeForEachLetterCount,true);
		GetWorld()->GetTimerManager().SetTimer(SubtitleHandle,this,&UESSubtitleWidget::EraseSubtitle,TimeForEachLetterCount* Subtitle.Len() + TimeForAfterLetterCount,false);
	}
	else
	{
		TextBlock->SetText(FText::FromString(Subtitle));
		const float SubtitleTime = !UseLetterCountAsDuration ?  Duration  : TimeForEachLetterCount* Subtitle.Len() + TimeForAfterLetterCount;
		GetWorld()->GetTimerManager().SetTimer(SubtitleHandle,this,&UESSubtitleWidget::EraseSubtitle,SubtitleTime,false);
	}

	
}



void UESSubtitleWidget::EraseSubtitle()
{
	TextBlock->SetText(FText());
}



void UESSubtitleWidget::SubtitleAnimation()
{
	if (SubtitleArray.IsValidIndex(SubtitleLetterIndex))
	{
		const FString SubtitleAnimatedText =  TextBlock->GetText().ToString() + SubtitleArray[SubtitleLetterIndex];
		TextBlock->SetText(FText::FromString(SubtitleAnimatedText));
	}
	else
		GetWorld()->GetTimerManager().ClearTimer(SubtitleAnimationHandle);
	
	SubtitleLetterIndex++;

}



void UESSubtitleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GetWorld()->GetTimerManager().SetTimer(SubtitleHandle,this,&UESSubtitleWidget::InitializeSubtitle,0.5,false);
}



void UESSubtitleWidget::InitializeSubtitle()
{
	if(const auto subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UESSubtitleSubsystem>())
		subsystem->OnExecuteSubtitle.AddDynamic(this,&UESSubtitleWidget::UESSubtitleWidget::ReceiveSubtitleRequest);
}