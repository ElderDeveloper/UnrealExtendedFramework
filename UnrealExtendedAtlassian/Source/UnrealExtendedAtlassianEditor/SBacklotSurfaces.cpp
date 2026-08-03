// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "SBacklotSurfaces.h"

#include "ExtendedAtlassianStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "SBacklotStylePrimitives.h"

void SBacklotSurfaceBase::ConstructSurface(const TSharedRef<SWidget>& Content)
{
	SetCanTick(false);
	SetClipping(EWidgetClipping::OnDemand);
	ChildSlot
	[
		Content
	];
}

int32 SBacklotSurfaceBase::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const int32 ContentLayer = SCompoundWidget::OnPaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);
	if (!HasFocusedDescendants() || !FSlateApplication::IsInitialized())
	{
		return ContentLayer;
	}

	const TSharedPtr<SWidget> Focused =
		FSlateApplication::Get().GetKeyboardFocusedWidget();
	if (!Focused.IsValid())
	{
		return ContentLayer;
	}
	const FGeometry& FocusedGeometry = Focused->GetCachedGeometry();
	const FVector2D TopLeft = AllottedGeometry.AbsoluteToLocal(
		FocusedGeometry.LocalToAbsolute(FVector2D::ZeroVector));
	const FVector2D BottomRight = AllottedGeometry.AbsoluteToLocal(
		FocusedGeometry.LocalToAbsolute(FocusedGeometry.GetLocalSize()));
	const FVector2D FocusedSize = BottomRight - TopLeft;
	BacklotStylePrimitives::PaintFocusRing(
		OutDrawElements,
		ContentLayer + 1,
		AllottedGeometry,
		TopLeft,
		FocusedSize,
		FExtendedAtlassianStyle::Metric(TEXT("Backlot.Metric.Focus.OutlineWidth")),
		FExtendedAtlassianStyle::Metric(TEXT("Backlot.Metric.Focus.OutlineOffset")),
		FExtendedAtlassianStyle::Metric(TEXT("Backlot.Metric.Focus.OutlineRadius")),
		FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff"))
			* InWidgetStyle.GetColorAndOpacityTint());
	return ContentLayer + 1;
}

#define DEFINE_BACKLOT_SURFACE(SurfaceClass) \
	void SurfaceClass::Construct(const FArguments& InArgs) \
	{ \
		ConstructSurface(InArgs._Content.Widget); \
	}

DEFINE_BACKLOT_SURFACE(SBacklotDocsSurface)
DEFINE_BACKLOT_SURFACE(SBacklotIssuesSurface)
DEFINE_BACKLOT_SURFACE(SBacklotIssueDetailSurface)
DEFINE_BACKLOT_SURFACE(SBacklotBoardSurface)
DEFINE_BACKLOT_SURFACE(SBacklotPinsSurface)
DEFINE_BACKLOT_SURFACE(SBacklotInboxSurface)

#undef DEFINE_BACKLOT_SURFACE
