// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Widgets/SLeafWidget.h"

class FEEUILabInputLog;
class UUserWidget;
class UWidget;

/**
 * UL-5 focus and navigation visualizer.
 *
 * Painted over the emulated viewport. For every focusable widget in the hosted tree it shows
 * a color-coded outline (green = focused, cyan = focusable, orange = focusable but currently
 * ineligible), its automation identity or UMG name, and its explicit UMG navigation rules
 * ("(auto)" when navigation is resolved automatically). Focus changes are logged to the shared
 * input log as "Focus" events, forming the focus history.
 *
 * In click-to-focus mode the overlay becomes hit-testable and a click sets Slate user focus
 * to the focusable widget under the cursor; otherwise it is HitTestInvisible.
 */
class SEEUILabFocusOverlay : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SEEUILabFocusOverlay) {}
		/** Resolves the currently hosted widget (may return null). */
		SLATE_ATTRIBUTE(UUserWidget*, HostedWidget)
		/** Whether the overlay renders at all. */
		SLATE_ATTRIBUTE(bool, ShowOverlay)
		/** Whether clicking sets focus (hit-testable) or the overlay is display-only. */
		SLATE_ATTRIBUTE(bool, ClickToFocus)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FEEUILabInputLog>& InLog);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	struct FFocusEntry
	{
		TWeakPtr<SWidget> SlateWidget;
		FString Label;
		FString NavSummary;
		bool bFocused = false;
		bool bEligible = true;
	};

	void CollectEntries(UUserWidget& Root, TArray<FFocusEntry>& OutEntries) const;
	static FString DescribeWidget(const UWidget& Widget);
	static FString DescribeNavigation(const UWidget& Widget);

	TAttribute<UUserWidget*> HostedWidget;
	TAttribute<bool> ShowOverlay;
	TAttribute<bool> ClickToFocus;
	TSharedPtr<FEEUILabInputLog> Log;
	TWeakPtr<SWidget> LastFocusedWidget;
};

#endif // WITH_EDITOR
