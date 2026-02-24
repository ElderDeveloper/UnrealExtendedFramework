// Fill out your copyright notice in the Description page of Project Settings.


#include "EFWidgetLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Blueprint/UserWidget.h"
#include "Slate/SObjectWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"




FVector2D UEFWidgetLibrary::GetGameViewportSize()
{
	FVector2D Result = FVector2D(1, 1);
 
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(Result);
	}
	return Result;
}

FVector2D UEFWidgetLibrary::GetGameResolution()
{
	return FVector2D(GSystemResolution.ResX, GSystemResolution.ResY);
}

FVector2D UEFWidgetLibrary::GetTexture2DSize(UTexture2D* Texture)
{
	if (Texture)
		return FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
	
	return FVector2D();
}

FVector2D UEFWidgetLibrary::GetTexture2DSizeClamped(UTexture2D* Texture, FVector2D Clamp)
{
	if (Texture)
	{
		return FVector2D(
			FMath::Clamp((float)Texture->GetSizeX(), 0.f, (float)Clamp.X),
			FMath::Clamp((float)Texture->GetSizeY(), 0.f, (float)Clamp.Y));
	}
	return FVector2D();
}

void UEFWidgetLibrary::SetSizeBoxSize(USizeBox* SizeBox, FVector2D Size)
{
	if (SizeBox)
	{
		SizeBox->SetWidthOverride(Size.X);
		SizeBox->SetHeightOverride(Size.Y);
	}
}

FVector2D UEFWidgetLibrary::GetSizeBoxSize(USizeBox* SizeBox)
{
	if (SizeBox)
	{
		return FVector2D(SizeBox->GetWidthOverride(), SizeBox->GetHeightOverride());
	}
	return FVector2D();
}

void UEFWidgetLibrary::SetProgressBarFillImage(UProgressBar* ProgressBar, UTexture2D* Texture)
{
	if (ProgressBar && Texture)
	{
		auto brush = FSlateBrush();
		brush.SetResourceObject(Texture);
		FProgressBarStyle BarStyle;
		BarStyle.FillImage = brush;
		ProgressBar->SetWidgetStyle(BarStyle);
	}
}

UOverlaySlot* UEFWidgetLibrary::AddChildToOverlay(UOverlay* Overlay, UUserWidget* Child, TEnumAsByte<EVerticalAlignment> InVerticalAlignment, TEnumAsByte<EHorizontalAlignment> InHorizontalAlignment)
{
	if (Overlay && Child)
	{
		const auto Slot = Overlay->AddChildToOverlay(Child);
		Slot->SetVerticalAlignment(InVerticalAlignment);
		Slot->SetHorizontalAlignment(InHorizontalAlignment);
		return Slot;
	}
	return nullptr;
}



UVerticalBoxSlot* UEFWidgetLibrary::AddChildToVerticalBox(UVerticalBox* VerticalBox, UUserWidget* Child, TEnumAsByte<EVerticalAlignment> InVerticalAlignment, TEnumAsByte<EHorizontalAlignment> InHorizontalAlignment)
{
	if (VerticalBox && Child)
	{
		const auto Slot = VerticalBox->AddChildToVerticalBox(Child);
		Slot->SetVerticalAlignment(InVerticalAlignment);
		Slot->SetHorizontalAlignment(InHorizontalAlignment);
		return Slot;
	}
	return nullptr;
}


UWidget* UEFWidgetLibrary::GetWidgetUnderCursor()
{
	if (!FSlateApplication::IsInitialized())
	{
		return nullptr;
	}

	FSlateApplication& SlateApp = FSlateApplication::Get();
	
	const FVector2D CursorPos = SlateApp.GetCursorPos();
	const FWidgetPath WidgetPath = SlateApp.LocateWindowUnderMouse(CursorPos, SlateApp.GetInteractiveTopLevelWindows());

	if (WidgetPath.IsValid())
	{
		// Walk from leaf to root looking for an SObjectWidget that wraps a UUserWidget
		for (int32 i = WidgetPath.Widgets.Num() - 1; i >= 0; --i)
		{
			const TSharedRef<SWidget>& SlateWidget = WidgetPath.Widgets[i].Widget;
			
			if (SlateWidget->GetType() == TEXT("SObjectWidget"))
			{
				const SObjectWidget* ObjectWidget = static_cast<const SObjectWidget*>(&SlateWidget.Get());
				if (UUserWidget* UserWidget = ObjectWidget->GetWidgetObject())
				{
					return UserWidget;
				}
			}
		}
	}

	return nullptr;
}

