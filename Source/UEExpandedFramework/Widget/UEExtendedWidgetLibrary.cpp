// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedWidgetLibrary.h"

#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/KismetMathLibrary.h"
#include "Slate/SlateBrushAsset.h"

FVector2D UUEExtendedWidgetLibrary::GetTexture2DSize(UTexture2D* Texture)
{
	if (Texture)
		return FVector2D(Texture->GetSizeX(),Texture->GetSizeY());
	
	return FVector2D();
}

FVector2D UUEExtendedWidgetLibrary::GetTexture2DSizeClamped(UTexture2D* Texture , FVector2D Clamp)
{
	if (Texture)
		return FVector2D(UKismetMathLibrary::Clamp(Texture->GetSizeX(),0,Clamp.X),UKismetMathLibrary::Clamp(Texture->GetSizeY(),0,Clamp.Y));
	
	return FVector2D();
}

void UUEExtendedWidgetLibrary::SetSizeBoxSize(USizeBox* SizeBox, FVector2D Size)
{
	if (SizeBox)
	{
		SizeBox->SetWidthOverride(Size.X);SizeBox->SetHeightOverride(Size.Y);
	}
}

FVector2D UUEExtendedWidgetLibrary::GetSizeBoxSize(USizeBox* SizeBox)
{
	if (SizeBox)
	{
		return FVector2D(SizeBox->WidthOverride,SizeBox->HeightOverride);
	}
	return FVector2D();
}

void UUEExtendedWidgetLibrary::SetProgressBarFillImage(UProgressBar* ProgressBar, UTexture2D* Texture)
{
	if (ProgressBar && Texture)
	{
		auto brush =FSlateBrush();
		brush.SetResourceObject(Texture);
		ProgressBar->WidgetStyle.SetFillImage(brush);
	}
}

void UUEExtendedWidgetLibrary::SetProgressBarFillImageSize(UProgressBar* ProgressBar, FVector2D Size)
{
}

UOverlaySlot* UUEExtendedWidgetLibrary::AddChildToOverlay(UOverlay* Overlay, UUserWidget* Child,TEnumAsByte<EVerticalAlignment> InVerticalAlignment, TEnumAsByte<EHorizontalAlignment> InHorizontalAlignment)
{
	if (Overlay && Child)
	{
		const auto Slot = Overlay->AddChildToOverlay(Child);
		Slot->SetVerticalAlignment(InVerticalAlignment);
		Slot->SetHorizontalAlignment(InHorizontalAlignment);
		return Slot;
	}	return nullptr;
}

UVerticalBoxSlot* UUEExtendedWidgetLibrary::AddChildToVerticalBox(UVerticalBox* VerticalBox, UUserWidget* Child,TEnumAsByte<EVerticalAlignment> InVerticalAlignment, TEnumAsByte<EHorizontalAlignment> InHorizontalAlignment)
{
	if (VerticalBox && Child)
	{
		const auto Slot = VerticalBox->AddChildToVerticalBox(Child);
		Slot->SetVerticalAlignment(InVerticalAlignment);
		Slot->SetHorizontalAlignment(InHorizontalAlignment);
		return Slot;
	}	return nullptr;
}
