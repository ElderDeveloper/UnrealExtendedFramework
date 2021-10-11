// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UEExtendedWidgetLibrary.generated.h"

/**
 * 
 */

class USizeBox;
class UProgressBar;
class UOverlaySlot;
class UOverlay;
class UVerticalBoxSlot;
class UVerticalBox;

UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedWidgetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Texture 2D >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure)
	static FVector2D GetTexture2DSize(UTexture2D* Texture);

	UFUNCTION(BlueprintPure)
	static FVector2D GetTexture2DSizeClamped(UTexture2D* Texture, FVector2D Clamp);


	
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Size Box >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable , Category="Widget|SizeBox")
	static void SetSizeBoxSize(USizeBox* SizeBox , FVector2D Size);


	UFUNCTION(BlueprintPure)
	static FVector2D GetSizeBoxSize(USizeBox* SizeBox);


	
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Progress Bar >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable , Category="Widget|ProgressBar")
	static void SetProgressBarFillImage(UProgressBar* ProgressBar , UTexture2D* Texture);

	UFUNCTION(BlueprintCallable , Category="Widget|ProgressBar")
	static void SetProgressBarFillImageSize(UProgressBar* ProgressBar , FVector2D Size);

	

	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Overlay >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable , Category="Widget|Overlay")
	static UOverlaySlot* AddChildToOverlay(UOverlay* Overlay , UUserWidget* Child , TEnumAsByte<EVerticalAlignment>InVerticalAlignment, TEnumAsByte<EHorizontalAlignment> InHorizontalAlignment);
	

	
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Vertical Box >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable , Category="Widget|VerticalBox")
	static UVerticalBoxSlot* AddChildToVerticalBox(UVerticalBox* VerticalBox , UUserWidget* Child , TEnumAsByte<EVerticalAlignment>InVerticalAlignment, TEnumAsByte<EHorizontalAlignment> InHorizontalAlignment);
};
