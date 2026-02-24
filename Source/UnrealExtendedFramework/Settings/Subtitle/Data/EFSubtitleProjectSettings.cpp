// EFSubtitleProjectSettings.cpp

#include "EFSubtitleProjectSettings.h"
#include "UnrealExtendedFramework/Settings/Subtitle/Widget/EFSubtitleDisplayWidget.h"

UEFSubtitleProjectSettings::UEFSubtitleProjectSettings()
{
	DefaultFont = FSlateFontInfo();
	DefaultFontColor = FLinearColor::White;
	ShadowOffset = FVector2D(1.0f, 1.0f);
	ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.5f);
	SubtitleWidgetClass = UEFSubtitleDisplayWidget::StaticClass();
}
