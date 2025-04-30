// Fill out your copyright notice in the Description page of Project Settings.


#include "EFWidgetLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/KismetMathLibrary.h"
#include "Slate/SlateBrushAsset.h"
#include "EFButtonStyle.h"
#include "Kismet/GameplayStatics.h"





FVector2D UEFWidgetLibrary::GetGameViewportSize()
{
	FVector2D Result = FVector2D( 1, 1 );
 
	if ( GEngine && GEngine->GameViewport )
	{
		GEngine->GameViewport->GetViewportSize(Result );
	}
	return Result;
}

FVector2D UEFWidgetLibrary::GameGameResolution()
{
	FVector2D Result;
	Result.X = GSystemResolution.ResX;
	Result.Y = GSystemResolution.ResY;
	return Result;
}

FVector2D UEFWidgetLibrary::GetTexture2DSize(UTexture2D* Texture)
{
	if (Texture)
		return FVector2D(Texture->GetSizeX(),Texture->GetSizeY());
	
	return FVector2D();
}

FVector2D UEFWidgetLibrary::GetTexture2DSizeClamped(UTexture2D* Texture , FVector2D Clamp)
{
	if (Texture)
		return FVector2D(UKismetMathLibrary::Clamp(Texture->GetSizeX(),0,Clamp.X),UKismetMathLibrary::Clamp(Texture->GetSizeY(),0,Clamp.Y));
	
	return FVector2D();
}

void UEFWidgetLibrary::SetSizeBoxSize(USizeBox* SizeBox, FVector2D Size)
{
	if (SizeBox)
	{
		SizeBox->SetWidthOverride(Size.X);SizeBox->SetHeightOverride(Size.Y);
	}
}

FVector2D UEFWidgetLibrary::GetSizeBoxSize(USizeBox* SizeBox)
{
	if (SizeBox)
	{
		return FVector2D(SizeBox->GetWidthOverride(),SizeBox->GetHeightOverride());
	}
	return FVector2D();
}

void UEFWidgetLibrary::SetProgressBarFillImage(UProgressBar* ProgressBar, UTexture2D* Texture)
{
	if (ProgressBar && Texture)
	{
		auto brush =FSlateBrush();
		brush.SetResourceObject(Texture);
		FProgressBarStyle BarStyle;
		BarStyle.FillImage = brush;
		ProgressBar->SetWidgetStyle(BarStyle);
	}
}

void UEFWidgetLibrary::SetProgressBarFillImageSize(UProgressBar* ProgressBar, FVector2D Size)
{
}

UOverlaySlot* UEFWidgetLibrary::AddChildToOverlay(UOverlay* Overlay, UUserWidget* Child,TEnumAsByte<EVerticalAlignment> InVerticalAlignment, TEnumAsByte<EHorizontalAlignment> InHorizontalAlignment)
{
	if (Overlay && Child)
	{
		const auto Slot = Overlay->AddChildToOverlay(Child);
		Slot->SetVerticalAlignment(InVerticalAlignment);
		Slot->SetHorizontalAlignment(InHorizontalAlignment);
		return Slot;
	}	return nullptr;
}



UVerticalBoxSlot* UEFWidgetLibrary::AddChildToVerticalBox(UVerticalBox* VerticalBox, UUserWidget* Child,TEnumAsByte<EVerticalAlignment> InVerticalAlignment, TEnumAsByte<EHorizontalAlignment> InHorizontalAlignment)
{
	if (VerticalBox && Child)
	{
		const auto Slot = VerticalBox->AddChildToVerticalBox(Child);
		Slot->SetVerticalAlignment(InVerticalAlignment);
		Slot->SetHorizontalAlignment(InHorizontalAlignment);
		return Slot;
	}	return nullptr;
}


void UEFWidgetLibrary::ApplyButtonStyle(UButton* Button, TSubclassOf<UEFButtonStyle> StyleClass)
{
	if (!Button || !StyleClass)
	{
		return;
	}

	// Apply button style
	Button->SetStyle(StyleClass.GetDefaultObject()->ButtonStyle);
}
