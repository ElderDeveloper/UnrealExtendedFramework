// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Data/EFSubtitleData.h"
#include "Blueprint/UserWidget.h"
#include "EFSubtitleWidget.generated.h"

class UTextBlock;

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* SubtitleText;

	void ApplyVisualSettings();
	void ExecuteSubtitle(const FExtendedSubtitle& Subtitle, ESubtitleExecutionType ExecutionType);

protected:
	FTimerHandle SubtitleTimerHandle;
	void StopSubtitle();
};
