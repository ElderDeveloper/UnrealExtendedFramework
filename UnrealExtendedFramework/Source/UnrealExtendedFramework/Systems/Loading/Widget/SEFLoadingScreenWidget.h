// SEFLoadingScreenWidget.h — Slate loading screen surface
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class UMaterialInterface;
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

		/**
		 * The materials of UEFLoadingScreenSettings::MaterialLayers, loaded once per loading session
		 * by the subsystem and given in the same order. A null entry is a layer that has no material
		 * or failed to load, and is skipped. The layout of each layer is read from the settings.
		 */
		SLATE_ARGUMENT(TArray<UMaterialInterface*>, LayerMaterials)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** Adds one image per loaded material layer, above the background image. */
	void AddMaterialLayers(const TSharedRef<class SOverlay>& Overlay, const TArray<UMaterialInterface*>& Materials);

	FSlateBrush ColorFillBrush;
	FSlateBrush BackgroundBrush;
	TStrongObjectPtr<UTexture2D> BackgroundTexture;
	/** One brush per drawn layer. Each SImage keeps a pointer to its brush, so they are allocated one by one and never move. */
	TArray<TUniquePtr<FSlateBrush>> LayerBrushes;
	/**
	 * Holds the layer materials for as long as the widget exists. The subsystem holds them too, but
	 * MoviePlayer may keep its copy of the widget after the subsystem has let go, and garbage is
	 * collected during the map load.
	 */
	TArray<TStrongObjectPtr<UMaterialInterface>> LayerMaterials;
	/** Kept as UObject so this header does not have to pull in the font types; FSlateFontInfo takes a UObject anyway. */
	TStrongObjectPtr<UObject> TextFont;
};
