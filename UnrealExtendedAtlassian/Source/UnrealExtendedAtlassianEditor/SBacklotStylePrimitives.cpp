// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "SBacklotStylePrimitives.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Views/STableRow.h"

void SBacklotDiagonalPattern::Construct(const FArguments& InArgs)
{
	ColorA = InArgs._ColorA;
	ColorB = InArgs._ColorB;
	StripeWidth = FMath::Max(1.0f, InArgs._StripeWidth);
	SetCanTick(false);
	SetClipping(EWidgetClipping::ClipToBounds);
	ChildSlot
	.HAlign(InArgs._HAlign)
	.VAlign(InArgs._VAlign)
	[
		InArgs._Content.Widget
	];
}

int32 SBacklotDiagonalPattern::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	static const FSlateColorBrush WhiteBrush(FLinearColor::White);
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		&WhiteBrush,
		ESlateDrawEffect::None,
		ColorB * InWidgetStyle.GetColorAndOpacityTint());

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float Step = StripeWidth * 2.0f;
	const float LineThickness = StripeWidth * FMath::Sqrt(2.0f);
	for (float Offset = -Size.Y - Step; Offset < Size.X + Step; Offset += Step)
	{
		TArray<FVector2D> Points;
		Points.Add(FVector2D(Offset, 0.0f));
		Points.Add(FVector2D(Offset + Size.Y, Size.Y));
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			ColorA * InWidgetStyle.GetColorAndOpacityTint(),
			false,
			LineThickness);
	}

	return SCompoundWidget::OnPaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId + 2,
		InWidgetStyle,
		bParentEnabled);
}

namespace BacklotEasing
{
	float CubicBezier(float X1, float Y1, float X2, float Y2, float T)
	{
		if (T <= 0.0f)
		{
			return 0.0f;
		}
		if (T >= 1.0f)
		{
			return 1.0f;
		}

		// Browsers solve X(S) = T for the bezier parameter S, then return Y(S).
		auto CurveX = [X1, X2](float S)
		{
			const float OneMinus = 1.0f - S;
			return 3.0f * OneMinus * OneMinus * S * X1
				+ 3.0f * OneMinus * S * S * X2
				+ S * S * S;
		};
		auto CurveY = [Y1, Y2](float S)
		{
			const float OneMinus = 1.0f - S;
			return 3.0f * OneMinus * OneMinus * S * Y1
				+ 3.0f * OneMinus * S * S * Y2
				+ S * S * S;
		};
		auto SlopeX = [X1, X2](float S)
		{
			const float OneMinus = 1.0f - S;
			return 3.0f * OneMinus * OneMinus * X1
				+ 6.0f * OneMinus * S * (X2 - X1)
				+ 3.0f * S * S * (1.0f - X2);
		};

		float Guess = T;
		for (int32 Iteration = 0; Iteration < 8; ++Iteration)
		{
			const float Error = CurveX(Guess) - T;
			if (FMath::Abs(Error) < 1.0e-6f)
			{
				return CurveY(Guess);
			}
			const float Slope = SlopeX(Guess);
			if (FMath::Abs(Slope) < 1.0e-6f)
			{
				break;
			}
			Guess -= Error / Slope;
		}

		// Bisection fallback keeps the result stable where Newton-Raphson stalls.
		float Low = 0.0f;
		float High = 1.0f;
		Guess = T;
		for (int32 Iteration = 0; Iteration < 32; ++Iteration)
		{
			const float Current = CurveX(Guess);
			if (FMath::Abs(Current - T) < 1.0e-6f)
			{
				break;
			}
			if (Current < T)
			{
				Low = Guess;
			}
			else
			{
				High = Guess;
			}
			Guess = (Low + High) * 0.5f;
		}
		return CurveY(Guess);
	}

	float Ease(float T)
	{
		return CubicBezier(0.25f, 0.1f, 0.25f, 1.0f, T);
	}

	float EaseInOut(float T)
	{
		return CubicBezier(0.42f, 0.0f, 0.58f, 1.0f, T);
	}
}

void SBacklotAnimatedPanel::Construct(const FArguments& InArgs)
{
	StartedAtSeconds = InArgs._StartedAtSeconds;
	DurationSeconds = FMath::Max(InArgs._DurationSeconds, 0.0f);
	RisePixels = InArgs._RisePixels;
	StartScale = InArgs._StartScale;
	bAnimate = InArgs._Animate;
	TimeAccessor = InArgs._OnGetTimeSeconds;

	SetCanTick(false);
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	ChildSlot
	[
		InArgs._Content.Widget
	];

	ApplySettledOrCurrentFrame();
	if (bAnimate && DurationSeconds > 0.0f && TimeAccessor.IsBound())
	{
		RegisterActiveTimer(
			0.0f,
			FWidgetActiveTimerDelegate::CreateSP(this, &SBacklotAnimatedPanel::Advance));
	}
}

float SBacklotAnimatedPanel::GetRawProgress() const
{
	if (!bAnimate || DurationSeconds <= 0.0f || !TimeAccessor.IsBound())
	{
		return 1.0f;
	}
	const double Elapsed = TimeAccessor.Execute() - StartedAtSeconds;
	return FMath::Clamp(
		static_cast<float>(Elapsed / static_cast<double>(DurationSeconds)),
		0.0f,
		1.0f);
}

float SBacklotAnimatedPanel::GetEasedProgress() const
{
	return BacklotEasing::Ease(GetRawProgress());
}

void SBacklotAnimatedPanel::ApplySettledOrCurrentFrame()
{
	const float Eased = GetEasedProgress();
	SetRenderOpacity(Eased);
	if (FMath::IsNearlyEqual(Eased, 1.0f))
	{
		SetRenderTransform(TOptional<FSlateRenderTransform>());
		return;
	}
	const float Scale = FMath::Lerp(StartScale, 1.0f, Eased);
	SetRenderTransform(
		FSlateRenderTransform(
			FScale2D(Scale),
			FVector2f(0.0f, RisePixels * (1.0f - Eased))));
}

EActiveTimerReturnType SBacklotAnimatedPanel::Advance(
	double InCurrentTime,
	float InDeltaTime)
{
	(void)InCurrentTime;
	(void)InDeltaTime;
	ApplySettledOrCurrentFrame();
	return FMath::IsNearlyEqual(GetRawProgress(), 1.0f)
		? EActiveTimerReturnType::Stop
		: EActiveTimerReturnType::Continue;
}

void SBacklotDropShadow::Construct(const FArguments& InArgs)
{
	OffsetY = InArgs._OffsetY;
	Blur = FMath::Max(InArgs._Blur, 0.0f);
	ShadowColor = InArgs._ShadowColor;
	CornerRadius = InArgs._CornerRadius;
	SetCanTick(false);
	ChildSlot
	[
		InArgs._Content.Widget
	];
}

int32 SBacklotDropShadow::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	// CSS blurs a shadow over roughly `Blur` pixels of falloff. Slate cannot blur a
	// box, so stack expanding rounded rectangles whose alpha decays over that radius.
	const int32 Steps = FMath::Clamp(FMath::RoundToInt(Blur / 4.0f), 1, 12);
	const FVector2D Size = AllottedGeometry.GetLocalSize();

	// Size the storage before taking any element's address: MakeBox keeps the pointer and Slate
	// reads it during the render pass, so growing the array mid-loop would leave earlier draw
	// elements pointing into freed memory.
	// Reset rather than SetNum*: the brush holds non-trivial members, and this both destroys the
	// previous frame's entries and reserves the capacity the loop below fills.
	ShadowBrushes.Reset(Steps);
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		ShadowBrushes.Emplace(
			FLinearColor(ShadowColor.R, ShadowColor.G, ShadowColor.B, 1.0f),
			CornerRadius + Blur * 0.5f
				* (static_cast<float>(Steps - Step) / static_cast<float>(Steps)));
	}

	for (int32 Step = Steps; Step >= 1; --Step)
	{
		const float Fraction = static_cast<float>(Step) / static_cast<float>(Steps);
		const float Spread = Blur * 0.5f * Fraction;
		// Squared falloff approximates the tail of a Gaussian more closely than linear.
		const float Alpha =
			ShadowColor.A * (1.0f - Fraction) * (1.0f - Fraction) / static_cast<float>(Steps)
			* 2.0f;
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(
					static_cast<float>(Size.X) + Spread * 2.0f,
					static_cast<float>(Size.Y) + Spread * 2.0f),
				FSlateLayoutTransform(FVector2f(-Spread, -Spread + OffsetY))),
			&ShadowBrushes[Steps - Step],
			ESlateDrawEffect::None,
			FLinearColor(
				1.0f,
				1.0f,
				1.0f,
				FMath::Clamp(Alpha, 0.0f, 1.0f))
				* InWidgetStyle.GetColorAndOpacityTint());
	}

	return SCompoundWidget::OnPaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId + 1,
		InWidgetStyle,
		bParentEnabled);
}

void SBacklotFocusRing::Construct(const FArguments& InArgs)
{
	Color = InArgs._Color;
	OutlineWidth = FMath::Max(0.0f, InArgs._OutlineWidth);
	OutlineOffset = FMath::Max(0.0f, InArgs._OutlineOffset);
	CornerRadius = FMath::Max(0.0f, InArgs._CornerRadius);
	bAlwaysShow = InArgs._AlwaysShow;
	RingBrush = FSlateRoundedBoxBrush(
		FLinearColor::Transparent,
		CornerRadius + OutlineOffset + OutlineWidth,
		Color,
		OutlineWidth);
	SetCanTick(false);
	SetClipping(EWidgetClipping::OnDemand);
	ChildSlot
	[
		InArgs._Content.Widget
	];
}

int32 SBacklotFocusRing::OnPaint(
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
	if (!bAlwaysShow && !HasKeyboardFocus() && !HasFocusedDescendants())
	{
		return ContentLayer;
	}

	const float Expansion = OutlineOffset + OutlineWidth;
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		ContentLayer + 1,
		AllottedGeometry.ToPaintGeometry(
			FVector2f(
				static_cast<float>(Size.X) + Expansion * 2.0f,
				static_cast<float>(Size.Y) + Expansion * 2.0f),
			FSlateLayoutTransform(FVector2f(-Expansion, -Expansion))),
		// Owned by the widget: see RingBrush. A stack brush here is freed before the
		// render pass reads it, which painted a white box instead of an outline.
		&RingBrush,
		ESlateDrawEffect::None,
		InWidgetStyle.GetColorAndOpacityTint());
	return ContentLayer + 1;
}

void SBacklotHoverBrightness::Construct(const FArguments& InArgs)
{
	HoverOverlay = InArgs._HoverOverlay;
	PressedOverlay = InArgs._PressedOverlay;
	SetCanTick(false);
	SetClipping(EWidgetClipping::ClipToBounds);
	SetCursor(InArgs._Cursor);
	ChildSlot
	[
		InArgs._Content.Widget
	];
}

int32 SBacklotHoverBrightness::OnPaint(
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
	if (!IsHovered() && !bPressed)
	{
		return ContentLayer;
	}

	static const FSlateColorBrush WhiteBrush(FLinearColor::White);
	const FLinearColor Overlay = bPressed ? PressedOverlay : HoverOverlay;
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		ContentLayer + 1,
		AllottedGeometry.ToPaintGeometry(),
		&WhiteBrush,
		ESlateDrawEffect::None,
		Overlay * InWidgetStyle.GetColorAndOpacityTint());
	return ContentLayer + 1;
}

void SBacklotHoverBrightness::OnMouseEnter(
	const FGeometry& MyGeometry,
	const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SBacklotHoverBrightness::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	bPressed = false;
	SCompoundWidget::OnMouseLeave(MouseEvent);
	Invalidate(EInvalidateWidgetReason::Paint);
}

FReply SBacklotHoverBrightness::OnMouseButtonDown(
	const FGeometry& MyGeometry,
	const FPointerEvent& MouseEvent)
{
	(void)MyGeometry;
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bPressed = true;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
	return FReply::Unhandled();
}

FReply SBacklotHoverBrightness::OnMouseButtonUp(
	const FGeometry& MyGeometry,
	const FPointerEvent& MouseEvent)
{
	(void)MyGeometry;
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bPressed = false;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
	return FReply::Unhandled();
}

void SBacklotVirtualizedWidgetList::Construct(const FArguments& InArgs)
{
	SetCanTick(false);
	SetClipping(EWidgetClipping::ClipToBounds);
	Items.Reserve(InArgs._Widgets.Num());
	for (const TSharedRef<SWidget>& Widget : InArgs._Widgets)
	{
		Items.Add(MakeShared<FItem>(Widget));
	}

	ChildSlot
	[
		SNew(SListView<FItemPtr>)
		.ListItemsSource(&Items)
		.SelectionMode(ESelectionMode::None)
		.ScrollBarStyle(InArgs._ScrollBarStyle)
		.OnGenerateRow(this, &SBacklotVirtualizedWidgetList::GenerateRow)
	];
}

TSharedRef<ITableRow> SBacklotVirtualizedWidgetList::GenerateRow(
	FItemPtr Item,
	const TSharedRef<STableViewBase>& OwnerTable) const
{
	check(Item.IsValid());
	return SNew(STableRow<FItemPtr>, OwnerTable)
		.Padding(0.0f)
		[
			Item->Widget
		];
}

void SBacklotVirtualizedWidgetTileList::Construct(const FArguments& InArgs)
{
	SetCanTick(false);
	SetClipping(EWidgetClipping::ClipToBounds);
	Items.Reserve(InArgs._Widgets.Num());
	for (const TSharedRef<SWidget>& Widget : InArgs._Widgets)
	{
		Items.Add(MakeShared<FItem>(Widget));
	}

	ChildSlot
	[
		SNew(STileView<FItemPtr>)
		.ListItemsSource(&Items)
		.SelectionMode(ESelectionMode::None)
		.ItemWidth(InArgs._ItemWidth)
		.ItemHeight(InArgs._ItemHeight)
		.ScrollBarStyle(InArgs._ScrollBarStyle)
		.OnGenerateTile(this, &SBacklotVirtualizedWidgetTileList::GenerateTile)
	];
}

TSharedRef<ITableRow> SBacklotVirtualizedWidgetTileList::GenerateTile(
	FItemPtr Item,
	const TSharedRef<STableViewBase>& OwnerTable) const
{
	check(Item.IsValid());
	return SNew(STableRow<FItemPtr>, OwnerTable)
		.Padding(FMargin(0.0f, 0.0f, 9.0f, 9.0f))
		[
			Item->Widget
		];
}

void SBacklotSyncPulse::Construct(const FArguments& InArgs)
{
	PulseColor = InArgs._PulseColor;
	MaxSpread = InArgs._MaxSpread;
	PeriodSeconds = FMath::Max(InArgs._PeriodSeconds, KINDA_SMALL_NUMBER);
	CornerRadius = InArgs._CornerRadius;
	bAnimate = InArgs._Animate;
	TimeAccessor = InArgs._OnGetTimeSeconds;
	SetCanTick(false);
	ChildSlot
	[
		InArgs._Content.Widget
	];
	if (bAnimate && TimeAccessor.IsBound())
	{
		RegisterActiveTimer(
			0.0f,
			FWidgetActiveTimerDelegate::CreateSP(this, &SBacklotSyncPulse::Advance));
	}
}

void SBacklotSyncPulse::GetPulseState(float& OutSpread, float& OutAlpha) const
{
	if (!bAnimate || !TimeAccessor.IsBound())
	{
		// The 0% keyframe is the settled frame: no spread, full ring alpha.
		OutSpread = 0.0f;
		OutAlpha = 0.5f;
		return;
	}
	const double Elapsed = TimeAccessor.Execute();
	const float Phase = static_cast<float>(
		FMath::Fmod(Elapsed, static_cast<double>(PeriodSeconds))
		/ static_cast<double>(PeriodSeconds));
	// 0% and 100% are identical, 50% is the fully expanded transparent ring.
	const float Triangle = Phase <= 0.5f ? Phase * 2.0f : (1.0f - Phase) * 2.0f;
	const float Eased = BacklotEasing::EaseInOut(Triangle);
	OutSpread = MaxSpread * Eased;
	OutAlpha = FMath::Lerp(0.5f, 0.0f, Eased);
}

EActiveTimerReturnType SBacklotSyncPulse::Advance(
	double InCurrentTime,
	float InDeltaTime)
{
	(void)InCurrentTime;
	(void)InDeltaTime;
	// Repainting is the whole point of the pulse; the ring is drawn in OnPaint.
	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

int32 SBacklotSyncPulse::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	float Spread = 0.0f;
	float Alpha = 0.0f;
	GetPulseState(Spread, Alpha);
	if (Alpha > KINDA_SMALL_NUMBER)
	{
		const FVector2D Size = AllottedGeometry.GetLocalSize();

		// Reassigned rather than built locally: the spread animates, but the brush still has to
		// outlive OnPaint because MakeBox only records its address.
		PulseBrush = FSlateRoundedBoxBrush(
			FLinearColor::Transparent,
			CornerRadius + Spread,
			FLinearColor(PulseColor.R, PulseColor.G, PulseColor.B, 1.0f),
			FMath::Max(Spread, 1.0f));
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(
					static_cast<float>(Size.X) + Spread * 2.0f,
					static_cast<float>(Size.Y) + Spread * 2.0f),
				FSlateLayoutTransform(FVector2f(-Spread, -Spread))),
			&PulseBrush,
			ESlateDrawEffect::None,
			FLinearColor(1.0f, 1.0f, 1.0f, Alpha)
				* InWidgetStyle.GetColorAndOpacityTint());
	}

	return SCompoundWidget::OnPaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId + 1,
		InWidgetStyle,
		bParentEnabled);
}

void SBacklotPixelRule::Construct(const FArguments& InArgs)
{
	Orientation = InArgs._Orientation;
	Color = InArgs._Color;
	SetCanTick(false);
}

FVector2D SBacklotPixelRule::ComputeDesiredSize(
	float LayoutScaleMultiplier) const
{
	const float LogicalPixel =
		1.0f / FMath::Max(LayoutScaleMultiplier, KINDA_SMALL_NUMBER);
	return Orientation == Orient_Horizontal
		? FVector2D(0.0f, LogicalPixel)
		: FVector2D(LogicalPixel, 0.0f);
}

int32 SBacklotPixelRule::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	(void)Args;
	(void)MyCullingRect;
	(void)bParentEnabled;
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	TArray<FVector2D> Points;
	if (Orientation == Orient_Horizontal)
	{
		Points.Add(FVector2D(0.0f, 0.5f));
		Points.Add(FVector2D(Size.X, 0.5f));
	}
	else
	{
		Points.Add(FVector2D(0.5f, 0.0f));
		Points.Add(FVector2D(0.5f, Size.Y));
	}
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		Points,
		ESlateDrawEffect::None,
		Color * InWidgetStyle.GetColorAndOpacityTint(),
		false,
		1.0f);
	return LayerId;
}
