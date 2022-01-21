// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "EGTargetingWidget.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGTargetingWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	
	void InitializeTargetWidget(UTexture2D* Image , FVector2D ImageSize)
	{
		if (!Image)
		{
			UE_LOG(LogBlueprint,Error,TEXT("UEExtended Targeting Widget Texture Not Valid"));
			return;
		}
		
		if (WidgetTree)
		{
			if (const auto RootWidget = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TargetingImage")))
			{
				WidgetTree->RootWidget = RootWidget;
				TSharedRef<SWidget> Widget = Super::RebuildWidget();
				RootWidget->SetBrushFromTexture(Image);
				RootWidget->SetBrushSize(ImageSize);
				UE_LOG(LogBlueprint,Log,TEXT("UEExtended Targeting Widget Initialized"));
				return;
			}
			UE_LOG(LogBlueprint,Error,TEXT("UEExtended Targeting Widget Construction of Root Widget Failed"));
		}
		else
			UE_LOG(LogBlueprint,Error,TEXT("UEExtended Targeting Widget WidgetTree is Null"));
	}
};
