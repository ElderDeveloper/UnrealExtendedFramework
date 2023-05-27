// Fill out your copyright notice in the Description page of Project Settings.


#include "ESSubtitleWidget.h"

#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Kismet/KismetStringLibrary.h"
#include "UnrealExtendedSettings/Subtitles/Subsystem/ESSubtitleSubsystem.h"

UESSubtitleWidget::UESSubtitleWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}



void UESSubtitleWidget::ReceiveSubtitleRequest(FString Subtitle, float Duration)
{
	if (SubtitleHandle.IsValid())
		GetWorld()->GetTimerManager().ClearTimer(SubtitleHandle);

	
	if (SubtitleAnimationHandle.IsValid())
		GetWorld()->GetTimerManager().ClearTimer(SubtitleAnimationHandle);
	
	GetSubtitleSubsystemVariables();
	
	if (SubtitleStoredLanguageFont.FontObject)
		TextBlock->SetFont(SubtitleStoredLanguageFont);
	else
		TextBlock->SetFont(SubtitleStoredFont);

	
	TextBlock->SetText(FText::FromString(""));
	const auto Color = Border->GetBrushColor();
	bUseBorder ?  Border->SetBrushColor(FLinearColor(Color.R,Color.G,Color.B,1)) : Border->SetBrushColor(FLinearColor(Color.R,Color.G,Color.B,0));

	
	SubtitleArray.Empty();
	SubtitleLetterIndex = 0;

	
	if (AnimateSubtitleLetters && UseLetterCountAsDuration)
	{
		SubtitleArray = UKismetStringLibrary::GetCharacterArrayFromString(Subtitle);
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
	Border->SetBrushColor(FLinearColor(0,0,0,0));
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



void UESSubtitleWidget::GetSubtitleSubsystemVariables()
{
	if (const auto Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UESSubtitleSubsystem>())
	{
		bUseBorder = Subsystem->bUseBorder;
		AnimateSubtitleLetters = Subsystem->AnimateSubtitleLetters;
		TimeForEachLetterCount = Subsystem->TimeForEachLetterCount;
		TimeForAfterLetterCount = Subsystem->TimeForAfterLetterCount;
		UseLetterCountAsDuration = Subsystem->UseLetterCountAsDuration;
		SubtitleStoredLanguageFont = Subsystem->GetExtendedSubtitleLanguage().Font;
	}
}



void UESSubtitleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GetWorld()->GetTimerManager().SetTimer(SubtitleHandle,this,&UESSubtitleWidget::InitializeSubtitle,0.5,false);
	SubtitleStoredFont = TextBlock->GetFont();
}



void UESSubtitleWidget::InitializeSubtitle()
{
	if(const auto subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UESSubtitleSubsystem>())
		subsystem->OnExecuteSubtitle.AddDynamic(this,&UESSubtitleWidget::UESSubtitleWidget::ReceiveSubtitleRequest);
}