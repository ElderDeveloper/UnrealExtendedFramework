// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "EFWidgetLibrary.generated.h"

class USizeBox;
class UProgressBar;
class UOverlaySlot;
class UOverlay;
class UVerticalBoxSlot;
class UVerticalBox;
class UButton;
class UEFButtonStyle;

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFWidgetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintPure , Category="Extended Widget")
	static FVector2D GetGameViewportSize();
	
	UFUNCTION(BlueprintPure , Category="Extended Widget")
	static FVector2D GameGameResolution();

	
	UFUNCTION(BlueprintCallable , Category="Extended Widget|Overlay")
	static UOverlaySlot* AddChildToOverlay(UOverlay* Overlay , UUserWidget* Child , TEnumAsByte<EVerticalAlignment>InVerticalAlignment, TEnumAsByte<EHorizontalAlignment> InHorizontalAlignment);

	UFUNCTION(BlueprintCallable , Category="Extended Widget|VerticalBox")
	static UVerticalBoxSlot* AddChildToVerticalBox(UVerticalBox* VerticalBox , UUserWidget* Child , TEnumAsByte<EVerticalAlignment>InVerticalAlignment, TEnumAsByte<EHorizontalAlignment> InHorizontalAlignment);
	
	
	UFUNCTION(BlueprintPure , Category="Extended Widget|Texture")
	static FVector2D GetTexture2DSize(UTexture2D* Texture);

	UFUNCTION(BlueprintPure , Category="Extended Widget|Texture")
	static FVector2D GetTexture2DSizeClamped(UTexture2D* Texture, FVector2D Clamp);
	
	UFUNCTION(BlueprintCallable , Category="Extended Widget|SizeBox")
	static void SetSizeBoxSize(USizeBox* SizeBox , FVector2D Size);
	
	UFUNCTION(BlueprintPure , Category="Extended Widget|SizeBox")
	static FVector2D GetSizeBoxSize(USizeBox* SizeBox);
	
	UFUNCTION(BlueprintCallable , Category="Extended Widget|ProgressBar")
	static void SetProgressBarFillImage(UProgressBar* ProgressBar , UTexture2D* Texture);

	UFUNCTION(BlueprintCallable , Category="Extended Widget|ProgressBar")
	static void SetProgressBarFillImageSize(UProgressBar* ProgressBar , FVector2D Size);
	
	
	// Apply Button Style using a style object
	UFUNCTION(BlueprintCallable , Category="Extended Widget|Button")
	static void ApplyButtonStyle(UButton* Button, TSubclassOf<UEFButtonStyle> StyleClass);
};
