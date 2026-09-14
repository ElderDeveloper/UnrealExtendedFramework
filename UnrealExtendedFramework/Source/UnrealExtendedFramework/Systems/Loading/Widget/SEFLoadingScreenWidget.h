// SEFLoadingScreenWidget.h — Slate loading screen surface
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class UTexture2D;

/**
 * Slate loading screen drawn by UEFLoadingScreenSubsystem, both through MoviePlayer during the
 * blocking map load and as a plain viewport overlay afterwards.
 *
 * Deliberately raw Slate rather than UMG: MoviePlayer renders this on the slate thread while the
 * game thread is blocked inside the map load, where a UUserWidget cannot tick or construct.
 */
class UNREALEXTENDEDFRAMEWORK_API SEFLoadingScreenWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEFLoadingScreenWidget)
		: _BackgroundTexture(nullptr)
	{}
		/** Texture chosen once per loading session by the subsystem; null falls back to the background color. */
		SLATE_ARGUMENT(UTexture2D*, BackgroundTexture)

		/** Tip line chosen once per loading session by the subsystem; empty hides the tip. */
		SLATE_ARGUMENT(FText, LoadingTip)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FSlateBrush ColorFillBrush;
	FSlateBrush BackgroundBrush;
	TStrongObjectPtr<UTexture2D> BackgroundTexture;
	/** Kept as UObject so this header does not have to pull in the font types; FSlateFontInfo takes a UObject anyway. */
	TStrongObjectPtr<UObject> TextFont;
};
