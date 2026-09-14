// SEFLoadingScreenWidget.cpp
#include "SEFLoadingScreenWidget.h"

#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "UnrealExtendedFramework/Systems/Loading/Data/EFLoadingScreenSettings.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogEFLoadingScreenWidget, Log, All);

void SEFLoadingScreenWidget::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);

	const UEFLoadingScreenSettings* Settings = GetDefault<UEFLoadingScreenSettings>();

	BackgroundBrush = FSlateBrush();
	BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
	BackgroundBrush.TintColor = Settings ? FSlateColor(Settings->BackgroundImageTint) : FSlateColor(FLinearColor::White);

	BackgroundTexture.Reset(InArgs._BackgroundTexture);

	if (BackgroundTexture.IsValid())
	{
		BackgroundTexture->SetForceMipLevelsToBeResident(30.0f);
		BackgroundTexture->WaitForStreaming();
		BackgroundTexture->UpdateResource();
		BackgroundBrush.SetResourceObject(BackgroundTexture.Get());
		const FIntPoint ImportedSize = BackgroundTexture->GetImportedSize();
		const FVector2D BrushImageSize = (ImportedSize.X > 0 && ImportedSize.Y > 0)
			? FVector2D(ImportedSize)
			: FVector2D(BackgroundTexture->GetSizeX(), BackgroundTexture->GetSizeY());
		BackgroundBrush.ImageSize = BrushImageSize;
		UE_LOG(LogEFLoadingScreenWidget, Log, TEXT("Loading screen: loaded background image '%s' resource=%dx%d imported=%dx%d."),
			*BackgroundTexture->GetPathName(),
			BackgroundTexture->GetSizeX(),
			BackgroundTexture->GetSizeY(),
			ImportedSize.X,
			ImportedSize.Y);

		if (BrushImageSize.X <= 64.0f || BrushImageSize.Y <= 64.0f)
		{
			UE_LOG(LogEFLoadingScreenWidget, Warning, TEXT("Loading screen: background image '%s' layout size is only %.0fx%.0f; check the assigned texture asset if a full-screen image was expected."),
				*BackgroundTexture->GetPathName(),
				BrushImageSize.X,
				BrushImageSize.Y);
		}
	}
	else
	{
		BackgroundBrush = *FCoreStyle::Get().GetBrush("WhiteBrush");
		BackgroundBrush.TintColor = Settings ? FSlateColor(Settings->BackgroundColor) : FSlateColor(FLinearColor::Black);
		UE_LOG(LogEFLoadingScreenWidget, Warning, TEXT("Loading screen: no configured background image loaded; using background color."));
	}

	const bool bShowCircularThrobber = !Settings || Settings->bShowCircularThrobber;
	const FVector2D ThrobberOffset = Settings ? Settings->CircularThrobberOffset : FVector2D(0.0f, -64.0f);
	const int32 PieceCount = Settings ? Settings->CircularThrobberPieceCount : 12;
	const float Period = Settings ? Settings->CircularThrobberPeriod : 1.0f;
	const float Radius = Settings ? Settings->CircularThrobberRadius : 36.0f;

	// Solid color fill behind the image so a missing texture has a visible fallback.
	ColorFillBrush = *FCoreStyle::Get().GetBrush("WhiteBrush");
	ColorFillBrush.TintColor = Settings ? FSlateColor(Settings->BackgroundColor) : FSlateColor(FLinearColor::Black);

	TSharedRef<SOverlay> RootOverlay = SNew(SOverlay)
		// Layer 0: solid background colour
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(&ColorFillBrush)
		]
		// Layer 1: background image
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			.StretchDirection(EStretchDirection::Both)
			[
				SNew(SImage)
				.Image(&BackgroundBrush)
			]
		];

	if (bShowCircularThrobber)
	{
		RootOverlay->AddSlot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(ThrobberOffset.X, 0.0f, 0.0f, FMath::Max(0.0f, -ThrobberOffset.Y)))
			[
				SNew(SCircularThrobber)
				.NumPieces(PieceCount)
				.Period(Period)
				.Radius(Radius)
			];
	}

	const bool bShowLoadingText = Settings && Settings->bShowLoadingText && !Settings->LoadingText.IsEmptyOrWhitespace();
	const bool bShowLoadingTip = (!Settings || Settings->bShowLoadingTip) && !InArgs._LoadingTip.IsEmptyOrWhitespace();

	if (bShowLoadingText || bShowLoadingTip)
	{
		const FName Typeface = Settings ? Settings->LoadingTipFontTypeface : FName(TEXT("Regular"));
		const FVector2D ShadowOffset = Settings ? Settings->LoadingTipShadowOffset : FVector2D(1.0f, 1.0f);
		const FLinearColor ShadowColor = Settings ? Settings->LoadingTipShadowColor : FLinearColor(0.0f, 0.0f, 0.0f, 0.75f);

		// Held strongly because the loading screen outlives the settings CDO soft reference resolve.
		TextFont.Reset(Settings ? Settings->LoadingTipFont.LoadSynchronous() : nullptr);

		if (!TextFont.IsValid() && Settings && !Settings->LoadingTipFont.IsNull())
		{
			UE_LOG(LogEFLoadingScreenWidget, Warning, TEXT("Loading screen: font '%s' failed to load; using the default Slate font."),
				*Settings->LoadingTipFont.ToString());
		}

		// Stacked in one box so a tip that wraps onto extra lines pushes the loading text up instead of overlapping it.
		TSharedRef<SVerticalBox> TextStack = SNew(SVerticalBox);

		if (bShowLoadingText)
		{
			const float TextFontSize = static_cast<float>(Settings->LoadingTextFontSize);
			const float Spacing = bShowLoadingTip ? FMath::Max(0.0f, Settings->LoadingTextSpacing) : 0.0f;

			TextStack->AddSlot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(FMargin(0.0f, 0.0f, 0.0f, Spacing))
				[
					SNew(STextBlock)
					.Text(Settings->LoadingText)
					.Font(TextFont.IsValid()
						? FSlateFontInfo(TextFont.Get(), TextFontSize, Typeface)
						: FCoreStyle::GetDefaultFontStyle("Regular", TextFontSize))
					.ColorAndOpacity(FSlateColor(Settings->LoadingTextColor))
					.ShadowOffset(ShadowOffset)
					.ShadowColorAndOpacity(ShadowColor)
					.Justification(ETextJustify::Center)
				];
		}

		if (bShowLoadingTip)
		{
			const float TipFontSize = Settings ? static_cast<float>(Settings->LoadingTipFontSize) : 20.0f;
			const FLinearColor TipColor = Settings ? Settings->LoadingTipColor : FLinearColor(0.85f, 0.80f, 0.68f, 1.0f);
			const float TipWrapWidth = Settings ? Settings->LoadingTipWrapWidth : 1200.0f;

			TextStack->AddSlot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(InArgs._LoadingTip)
					.Font(TextFont.IsValid()
						? FSlateFontInfo(TextFont.Get(), TipFontSize, Typeface)
						: FCoreStyle::GetDefaultFontStyle("Regular", TipFontSize))
					.ColorAndOpacity(FSlateColor(TipColor))
					.ShadowOffset(ShadowOffset)
					.ShadowColorAndOpacity(ShadowColor)
					.Justification(ETextJustify::Center)
					.WrapTextAt(FMath::Max(0.0f, TipWrapWidth))
				];
		}

		const FVector2D StackOffset = Settings ? Settings->LoadingTipOffset : FVector2D(0.0f, -32.0f);

		RootOverlay->AddSlot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(StackOffset.X, 0.0f, 0.0f, FMath::Max(0.0f, -StackOffset.Y)))
			[
				TextStack
			];
	}

	ChildSlot
	[
		RootOverlay
	];
}
