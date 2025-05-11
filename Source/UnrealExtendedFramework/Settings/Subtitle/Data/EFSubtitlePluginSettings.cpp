// Fill out your copyright notice in the Description page of Project Settings.

#include "EFSubtitlePluginSettings.h"


UEFSubtitleSettings::UEFSubtitleSettings()
{
	Font = FSlateFontInfo();
	FontColor = FLinearColor::White;

	ShadowOffset = FVector2D(1, 1);
	ShadowColor = FLinearColor{0,0,0,0};
		
	BorderSettings = FExtendedSubtitleBorderSettings();
	BackgroundSettings = FExtendedSubtitleBackgroundSettings();

	CategoryName = TEXT("Extended Framework");
}


