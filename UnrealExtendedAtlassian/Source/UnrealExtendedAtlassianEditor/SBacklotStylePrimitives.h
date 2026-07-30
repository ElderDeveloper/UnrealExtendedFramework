// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STileView.h"

/** Pixel-clipped repeating 135-degree bands used by Backlot placeholders. */
class SBacklotDiagonalPattern final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBacklotDiagonalPattern)
		: _ColorA(FLinearColor::Black)
		, _ColorB(FLinearColor::Black)
		, _StripeWidth(9.0f)
		, _HAlign(HAlign_Fill)
		, _VAlign(VAlign_Fill)
	{}
		SLATE_ARGUMENT(FLinearColor, ColorA)
		SLATE_ARGUMENT(FLinearColor, ColorB)
		SLATE_ARGUMENT(float, StripeWidth)
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	FLinearColor ColorA = FLinearColor::Black;
	FLinearColor ColorB = FLinearColor::Black;
	float StripeWidth = 9.0f;
};

/** CSS timing functions reproduced exactly so Slate easing matches the authored keyframes. */
namespace BacklotEasing
{
	/** Evaluates a CSS cubic-bezier(X1, Y1, X2, Y2) timing function at progress T. */
	float CubicBezier(float X1, float Y1, float X2, float Y2, float T);

	/** CSS `ease` == cubic-bezier(.25, .1, .25, 1). */
	float Ease(float T);

	/** CSS `ease-in-out` == cubic-bezier(.42, 0, .58, 1). */
	float EaseInOut(float T);
}

/**
 * Reproduces the authored `bl-in` and `bl-toast` keyframes: opacity 0 -> 1 with a
 * translateY rise and an optional 0.99 -> 1 scale. The animation is advanced by a
 * registered active timer rather than per-frame Tick, and reads its clock through an
 * injected accessor so automation can advance it deterministically.
 */
class SBacklotAnimatedPanel final : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal(double, FOnGetTimeSeconds);

	SLATE_BEGIN_ARGS(SBacklotAnimatedPanel)
		: _StartedAtSeconds(0.0)
		, _DurationSeconds(0.14f)
		, _RisePixels(6.0f)
		, _StartScale(1.0f)
		, _Animate(true)
	{}
		/** Clock value captured when the overlay opened. */
		SLATE_ARGUMENT(double, StartedAtSeconds)
		/** Authored keyframe duration; the source uses .12 - .18s for bl-in and .2s for bl-toast. */
		SLATE_ARGUMENT(float, DurationSeconds)
		/** Authored translateY start offset: 6px for bl-in, 10px for bl-toast. */
		SLATE_ARGUMENT(float, RisePixels)
		/** Authored scale start: .99 for bl-in, 1 for bl-toast. */
		SLATE_ARGUMENT(float, StartScale)
		/** False snaps straight to the settled frame for reduced motion and deterministic capture. */
		SLATE_ARGUMENT(bool, Animate)
		SLATE_EVENT(FOnGetTimeSeconds, OnGetTimeSeconds)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Linear 0..1 keyframe position; 1 once settled. Exposed for automation. */
	float GetRawProgress() const;

	/** Eased 0..1 position actually applied to opacity, rise, and scale. */
	float GetEasedProgress() const;

private:
	EActiveTimerReturnType Advance(double InCurrentTime, float InDeltaTime);
	void ApplySettledOrCurrentFrame();

	double StartedAtSeconds = 0.0;
	float DurationSeconds = 0.14f;
	float RisePixels = 6.0f;
	float StartScale = 1.0f;
	bool bAnimate = true;
	FOnGetTimeSeconds TimeAccessor;
};

/**
 * Paints the authored CSS drop shadows (`0 <offset>px <blur>px rgba(0,0,0,<alpha>)`)
 * beneath the content. Slate has no box-shadow primitive, so the blur is approximated
 * with stacked, expanding, decreasing-alpha rounded rectangles.
 */
class SBacklotDropShadow final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBacklotDropShadow)
		: _OffsetY(14.0f)
		, _Blur(34.0f)
		, _ShadowColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f))
		, _CornerRadius(12.0f)
	{}
		SLATE_ARGUMENT(float, OffsetY)
		SLATE_ARGUMENT(float, Blur)
		SLATE_ARGUMENT(FLinearColor, ShadowColor)
		SLATE_ARGUMENT(float, CornerRadius)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	float OffsetY = 14.0f;
	float Blur = 34.0f;
	FLinearColor ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.5f);
	float CornerRadius = 12.0f;
};

/**
 * Paints the authored `:focus-visible` ring around a control or compound control.
 * The ring is two pixels wide, offset by one pixel, and never consumes layout space.
 */
class SBacklotFocusRing final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBacklotFocusRing)
		: _Color(FLinearColor(0.345f, 0.651f, 1.0f, 1.0f))
		, _OutlineWidth(2.0f)
		, _OutlineOffset(1.0f)
		, _CornerRadius(4.0f)
		, _AlwaysShow(false)
	{}
		SLATE_ARGUMENT(FLinearColor, Color)
		SLATE_ARGUMENT(float, OutlineWidth)
		SLATE_ARGUMENT(float, OutlineOffset)
		SLATE_ARGUMENT(float, CornerRadius)
		SLATE_ARGUMENT(bool, AlwaysShow)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	FLinearColor Color;
	float OutlineWidth = 2.0f;
	float OutlineOffset = 1.0f;
	float CornerRadius = 4.0f;
	bool bAlwaysShow = false;
};

/**
 * Dedicated hover/pressed brightness layer used where CSS applies `filter: brightness(...)`.
 * It also supplies the pointer cursor instead of relying on platform-default SBorder behavior.
 */
class SBacklotHoverBrightness final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBacklotHoverBrightness)
		: _HoverOverlay(FLinearColor(1.0f, 1.0f, 1.0f, 0.035f))
		, _PressedOverlay(FLinearColor(0.0f, 0.0f, 0.0f, 0.08f))
		, _Cursor(EMouseCursor::Hand)
	{}
		SLATE_ARGUMENT(FLinearColor, HoverOverlay)
		SLATE_ARGUMENT(FLinearColor, PressedOverlay)
		SLATE_ARGUMENT(TOptional<EMouseCursor::Type>, Cursor)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual void OnMouseEnter(
		const FGeometry& MyGeometry,
		const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(
		const FGeometry& MyGeometry,
		const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(
		const FGeometry& MyGeometry,
		const FPointerEvent& MouseEvent) override;

private:
	FLinearColor HoverOverlay;
	FLinearColor PressedOverlay;
	bool bPressed = false;
};

/**
 * Viewport-virtualized host for fully-authored Backlot row widgets.
 *
 * The widgets retain their custom parity layout, while SListView owns attachment,
 * layout, paint, and scrolling so off-screen rows do not participate in Slate's
 * live widget tree. Use this for data-backed surfaces whose row count is unbounded.
 */
class SBacklotVirtualizedWidgetList final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBacklotVirtualizedWidgetList)
		: _ScrollBarStyle(nullptr)
	{}
		SLATE_ARGUMENT(TArray<TSharedRef<SWidget>>, Widgets)
		SLATE_ARGUMENT(const FScrollBarStyle*, ScrollBarStyle)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Number of logical rows owned by the list; exposed for performance automation. */
	int32 GetItemCount() const { return Items.Num(); }

private:
	struct FItem
	{
		explicit FItem(const TSharedRef<SWidget>& InWidget)
			: Widget(InWidget)
		{
		}

		TSharedRef<SWidget> Widget;
	};

	using FItemPtr = TSharedPtr<FItem>;

	TSharedRef<ITableRow> GenerateRow(
		FItemPtr Item,
		const TSharedRef<STableViewBase>& OwnerTable) const;

	TArray<FItemPtr> Items;
};

/** Virtualized tile equivalent used for the responsive Pins card grid at high counts. */
class SBacklotVirtualizedWidgetTileList final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBacklotVirtualizedWidgetTileList)
		: _ItemWidth(310.0f)
		, _ItemHeight(360.0f)
		, _ScrollBarStyle(nullptr)
	{}
		SLATE_ARGUMENT(TArray<TSharedRef<SWidget>>, Widgets)
		SLATE_ARGUMENT(float, ItemWidth)
		SLATE_ARGUMENT(float, ItemHeight)
		SLATE_ARGUMENT(const FScrollBarStyle*, ScrollBarStyle)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	int32 GetItemCount() const { return Items.Num(); }

private:
	struct FItem
	{
		explicit FItem(const TSharedRef<SWidget>& InWidget)
			: Widget(InWidget)
		{
		}

		TSharedRef<SWidget> Widget;
	};

	using FItemPtr = TSharedPtr<FItem>;

	TSharedRef<ITableRow> GenerateTile(
		FItemPtr Item,
		const TSharedRef<STableViewBase>& OwnerTable) const;

	TArray<FItemPtr> Items;
};

/**
 * Reproduces `@keyframes bl-pulse`: a 3s ease-in-out loop expanding a spread ring from
 * 0px at rgba(88,166,255,.5) to 6px at rgba(88,166,255,0) and back.
 */
class SBacklotSyncPulse final : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal(double, FOnGetTimeSeconds);

	SLATE_BEGIN_ARGS(SBacklotSyncPulse)
		: _PulseColor(FLinearColor(0.345f, 0.651f, 1.0f, 1.0f))
		, _MaxSpread(6.0f)
		, _PeriodSeconds(3.0f)
		, _CornerRadius(4.0f)
		, _Animate(true)
	{}
		SLATE_ARGUMENT(FLinearColor, PulseColor)
		SLATE_ARGUMENT(float, MaxSpread)
		SLATE_ARGUMENT(float, PeriodSeconds)
		SLATE_ARGUMENT(float, CornerRadius)
		SLATE_ARGUMENT(bool, Animate)
		SLATE_EVENT(FOnGetTimeSeconds, OnGetTimeSeconds)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Current spread in pixels and alpha, exposed for automation. */
	void GetPulseState(float& OutSpread, float& OutAlpha) const;

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	EActiveTimerReturnType Advance(double InCurrentTime, float InDeltaTime);

	FLinearColor PulseColor = FLinearColor(0.345f, 0.651f, 1.0f, 1.0f);
	float MaxSpread = 6.0f;
	float PeriodSeconds = 3.0f;
	float CornerRadius = 4.0f;
	bool bAnimate = true;
	FOnGetTimeSeconds TimeAccessor;
};

/** A one-device-pixel horizontal or vertical rule aligned to a half-pixel paint coordinate. */
class SBacklotPixelRule final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SBacklotPixelRule)
		: _Orientation(Orient_Horizontal)
		, _Color(FLinearColor::White)
	{}
		SLATE_ARGUMENT(EOrientation, Orientation)
		SLATE_ARGUMENT(FLinearColor, Color)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	EOrientation Orientation = Orient_Horizontal;
	FLinearColor Color = FLinearColor::White;
};
