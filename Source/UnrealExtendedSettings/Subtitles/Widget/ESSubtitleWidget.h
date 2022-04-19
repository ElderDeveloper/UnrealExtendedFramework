// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ESSubtitleWidget.generated.h"

/**
 * 
 */
class UTextBlock;
class UScaleBox;
class USizeBox;
class UBorder;

UCLASS(BlueprintType , Blueprintable)
class UNREALEXTENDEDSETTINGS_API UESSubtitleWidget : public UUserWidget
{
	GENERATED_BODY()

	UESSubtitleWidget(const FObjectInitializer& ObjectInitializer);

public:

	UPROPERTY(EditAnywhere , BlueprintReadWrite , meta=(BindWidget))
	USizeBox* SizeBox;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , meta=(BindWidget))
	UBorder* Border;
	
	UPROPERTY(EditAnywhere , BlueprintReadWrite , meta=(BindWidget))
	UScaleBox* ScaleBox;

	UPROPERTY(EditAnywhere , BlueprintReadWrite, meta=(BindWidget))
	UTextBlock* TextBlock;


	FTimerHandle SubtitleHandle;
	FTimerHandle SubtitleAnimationHandle;

	
	UFUNCTION()
	void ReceiveSubtitleRequest(FString Subtitle , float Duration , bool UseLetterCountAsDuration , float TimeForEachLetterCount , float TimeForAfterLetterCount , bool AnimateSubtitleLetters);

	void EraseSubtitle();
	void InitializeSubtitle();
	void SubtitleAnimation();



	
	virtual void NativeConstruct() override;

protected:

	TArray<FString> SubtitleArray;
	FString StoredSubtitle;
	int32 SubtitleLetterIndex;
	
};
